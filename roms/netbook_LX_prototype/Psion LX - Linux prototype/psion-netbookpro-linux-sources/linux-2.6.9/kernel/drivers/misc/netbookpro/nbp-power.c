/* drivers/misc/netbookpro/nbp-power.c
 *
 * (c) 2004 Simtec Electronics
 * Written by Ben Dooks
 *
 * $Id: nbp-power.c,v 1.16 2004/09/14 20:24:50 rmk Exp $ 
*/

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/compiler.h>
#include <linux/init.h>
#include <linux/completion.h>
#include <linux/suspend.h>
#include <linux/interrupt.h>
#include <linux/i2c-algo-pxa.h>

#include <asm/hardware.h>
#include <asm/irq.h>

#include <asm/arch/netbookpro.h>
#include <asm/arch/netbookpro-pcon.h>

#include "nbp-power.h"

#define PFX "nbp-power: "

static int debug;

#define DBG(x...)					\
	do {						\
		if (debug)				\
			pr_debug("nbp-power: " x);	\
	} while (0)

static struct i2c_eeprom_emu *nbp_pwr_eeprom;

static int pid;
static int time_to_die;
static int recv_data;

static DECLARE_WAIT_QUEUE_HEAD(ext_wait);
static DECLARE_WAIT_QUEUE_HEAD(thr_wait);
static DECLARE_COMPLETION(thr_complete);

static pcon_pwrstate_t   pwr_state;
static unsigned long     pwr_lastseen;

/* code */


static const char *nbp_pwr_decodestate(pcon_pwrstate_t state)
{
	switch (state) {
	case PCON_PWRSTATE_HWReset:
		return "Hardware-Reset";

	case PCON_PWRSTATE_SWReset:
		return "Software-Reset";

	case PCON_PWRSTATE_Off:
		return "Off";

	case PCON_PWRSTATE_Suspend:
		return "Suspend";

	case PCON_PWRSTATE_Sleep:
		return "Sleep";

	case PCON_PWRSTATE_Booting:
		return "Booting";

	case PCON_PWRSTATE_LoadingOS:
		return "Loading-OS";

	case PCON_PWRSTATE_Idle:
		return "Idle";

	case PCON_PWRSTATE_Run:
		return "Run";
		
	case PCON_PWRSTATE_Comatose:
		return "Comatose";

	case PCON_PWRSTATE_PowerFail:
		return "Power-Fail";
	}

	return "Unknown-Power-State";
}

static int nbp_pwr_eeprom_write(struct i2c_eeprom_emu *eep, 
				 int addr, int val)
{
	DBG("nbp_pwr_eeprom_write(%p,%d,0x%02x)\n", eep, addr, val);

	recv_data = 1;
	wmb();
	wake_up(&thr_wait);
	wake_up(&ext_wait);

	DBG("received power level %d (%s)\n", val, nbp_pwr_decodestate(val));
	
	return 0;
}

static struct i2c_eeprom_emu_watcher nbp_pwr_watcher = {
	.read  = NULL,
	.write = nbp_pwr_eeprom_write
};

/*
 * This is inherently racy.
 */
int nbp_pwr_waitchange(void)
{
	unsigned long timeout = HZ*2;

	interruptible_sleep_on_timeout(&ext_wait, timeout);

	if (timeout == 0) {
		printk(KERN_ERR ": failed to get power state change\n");
		return -ETIMEDOUT;
	}

	return nbp_pwr_eeprom->bytes[NETBOOKPRO_I2C_EEPEMU_POWER_START].val;
}


#ifndef CONFIG_APM
/*
 * This is the "old" suspend management code.  We now go via
 * the APM layer so that userspace is informed of changes.
 * See nbp-apm.c for implementation.
 */
static int do_suspend;
static int suspmon_pid;
static DECLARE_WAIT_QUEUE_HEAD(suspmon_wait);
static DECLARE_COMPLETION(suspmon_complete);

static irqreturn_t power_irq(int irqno, void *arg, struct pt_regs *regs)
{
	do_suspend = 1;
	wmb();
	wake_up(&suspmon_wait);

	return IRQ_HANDLED;
}

static int nbp_pwr_suspmon(void *arg)
{
	daemonize("nbp-suspendmon");
	current->flags |= PF_NOFREEZE;

	while (1) {
		wait_event_interruptible(suspmon_wait, do_suspend || time_to_die);

		if (time_to_die)
			break;

		if (do_suspend) {
			do_suspend = 0;

			DBG("System suspending\n");
			pm_suspend(PM_SUSPEND_MEM);
			DBG("pm_suspend() returned\n");
		}

		DBG("suspendmon thread run\n");
	}

	DBG("suspendmon thread exiting\n");

	complete_and_exit (&suspmon_complete, 0);	
}

static int __init suspmon_init(void)
{
	int ret;

	ret = kernel_thread(nbp_pwr_suspmon, NULL, CLONE_KERNEL);
	if (ret < 0) {
		printk(PFX "error %d starting driver thread", suspmon_pid);
		return ret;
	}
	if (request_irq(IRQ_GPIO(0), power_irq,
			SA_INTERRUPT, "Battery Warning", NULL) < 0) {
		printk(KERN_ERR "Failed to get irq for battery warning\n");
	}

	if (request_irq(IRQ_GPIO(1), power_irq,
			SA_INTERRUPT, "Suspend Request", NULL) < 0) {
		printk(KERN_ERR "Failed to get irq for suspend request\n");
	}

	set_irq_type(IRQ_GPIO(0), IRQT_RISING);
	set_irq_type(IRQ_GPIO(1), IRQT_BOTHEDGE);

	return 0;
}

static void suspmon_exit(void)
{
	wake_up(&suspmon_wait);
	wait_for_completion(&suspmon_complete);
}
#else
#define suspmon_init() (0)
#define suspmon_exit() do { } while (0)
#endif


static int nbp_pwr_thread(void *arg)
{
	daemonize("nbp-powermgr");
	current->flags |= PF_NOFREEZE;

	nbp_pcon_command(PCON_POWER_STATE, PCON_PWRSTATE_Run, 0, NULL);

	while (1) {
		wait_event_interruptible(thr_wait, recv_data || time_to_die);

		if (time_to_die) {
			DBG("time to die\n");
			break;
		}

		if (recv_data) {
			pcon_pwrstate_t state;
			recv_data = 0;
		     
			state = nbp_pwr_eeprom->bytes[NETBOOKPRO_I2C_EEPEMU_POWER_START].val;
			printk(KERN_INFO "Power state is now %s (%d)\n",
			       nbp_pwr_decodestate(state), state);

			switch (state) {
			case PCON_PWRSTATE_Run:
				printk(KERN_INFO "starting power monitor\n");
				nbp_battmon_start();
				break;

			default:
				state = state;
			}

			pwr_state = state;
			pwr_lastseen = jiffies;
		}
	}

	DBG("thread exiting\n");

	complete_and_exit (&thr_complete, 0);
}

static void nbp_pwr_power_off(void)
{
	nbp_pcon_command(PCON_POWER_STATE, PCON_PWRSTATE_Off, 0, NULL);
}

static void nbp_pwr_restart(void)
{
	nbp_pcon_command(PCON_POWER_STATE, PCON_PWRSTATE_HWReset, 0, NULL);
}

static int nbp_pwr_add(struct pcon_device *cdev)
{
	nbp_pwr_eeprom = i2c_pxa_get_eeprom();

	i2c_eeprom_emu_addwatcher(nbp_pwr_eeprom,
				  NETBOOKPRO_I2C_EEPEMU_POWER_START,
				  NETBOOKPRO_I2C_EEPEMU_POWER_SIZE,
				  &nbp_pwr_watcher);

	pid = kernel_thread(nbp_pwr_thread, NULL, CLONE_KERNEL);
	if (pid < 0) {
		printk(PFX "error %d starting driver thread\n", pid);
		return pid;
	}

	DBG("driver started thread %d\n", pid);

	pm_power_off = nbp_pwr_power_off;
	pm_restart = nbp_pwr_restart;

	return 0;
}

static void nbp_pwr_remove(struct pcon_device *cdev)
{
	int ret;

	pm_restart = NULL;
	pm_power_off = NULL;

	DBG("Killing driver thread\n");

	if (pid >= 0) {
		time_to_die = 1;
		wmb();
		ret = kill_proc(pid, SIGTERM, 1);

		if (ret < 0) {
			printk(KERN_ERR PFX "failed to signal thread exit\n");
		} else {
			wait_for_completion(&thr_complete);
		}
	}

	/* FIXME - what about our eeprom? */
}

static struct pcon_interface nbp_pwr_intf = {
	.add	= nbp_pwr_add,
	.remove	= nbp_pwr_remove,
};

static int __init nbp_pwr_init(void)
{
	int ret;

	printk(KERN_INFO "NetBookPro Power Manager, (c) 2004 Simtec Electronics\n");

	ret = suspmon_init();
	if (ret == 0) {
		ret = pcon_drv_add(&nbp_pwr_intf);
		if (ret)
			suspmon_exit();
	}

	return ret;
}

static void __exit nbp_pwr_exit(void)
{
	pcon_drv_remove(&nbp_pwr_intf);
	suspmon_exit();
}

module_init(nbp_pwr_init);
module_exit(nbp_pwr_exit);

MODULE_AUTHOR("Ben Dooks, Simtec Electronics <ben@simtec.co.uk>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Psion Teklogix NetBookPro Power Management");

module_param(debug, int, 0600);
