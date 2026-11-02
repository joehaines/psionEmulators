/* drivers/misc/netbookpro/nbp-battmon.c
 *
 * (c) 2004 Simtec Electronics
 * Written by Ben Dooks
*/

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/compiler.h>
#include <linux/init.h>
#include <linux/completion.h>
#include <linux/suspend.h>
#include <linux/proc_fs.h>

#include <linux/i2c.h>

#include <asm/arch/netbookpro.h>
#include <asm/arch/netbookpro-pcon.h>
#include <linux/i2c-algo-pxa.h>

#include "nbp-power.h"

#define PFX "nbp-battmon: "

static int pid;
static int time_to_die;
static int recv_data;
static int thr_wakeup;

static unsigned long last_at;
static struct pcon_battinfo last_data;

static wait_queue_head_t thr_wait;
static wait_queue_head_t thr_sleep;
static struct completion thr_complete;

static struct proc_dir_entry *proc_drv_ent;
static struct proc_dir_entry *proc_stat_ent;

static char proc_fname_drv[] = "driver/battery";
static char proc_fname_stat[] = "driver/battery-stats";

struct power_stats {
	unsigned long ok;
	unsigned long bad;
};

static struct power_stats stats;

   
/* code */

static int nbp_battmon_setstate(int enable)
{
	int ret;

	printk(KERN_DEBUG PFX "nbp_battmon_setstate: %d\n", enable);

	ret = nbp_pcon_command(PCON_MONITORBATTERY, enable, 0, NULL);

	printk(KERN_DEBUG PFX "nbp_battmon_setstate: returned %d\n", ret);

	return ret < 0 ? 1 : 0;
}

void nbp_battmon_start(void)
{
	printk(KERN_DEBUG PFX "issuing start...\n");

	thr_wakeup = 1;
	wmb();
	wake_up(&thr_sleep);
}

static int nbp_battmon_thread(void *param)
{
	unsigned long timeout = 60*HZ;
	int frozen = 0;

	printk(KERN_DEBUG PFX "thread starting\n");

	daemonize("nbp-battmon");
	allow_signal(SIGTERM);

	while (1) {
		timeout = HZ*60;

		do {
			timeout = interruptible_sleep_on_timeout(&thr_sleep,
								 timeout);

			/* make swsusp happy with our thread */
			if (current->flags & PF_FREEZE) {
				frozen = 1;
				refrigerator(PF_FREEZE);
			}

		} while (!signal_pending(current) &&
			 timeout > 0 && !thr_wakeup);

		if (thr_wakeup || timeout <= 0)
			break;
	}

	printk(KERN_DEBUG PFX "thread woken\n");

	if (nbp_battmon_setstate(1) == 0) {
		printk(KERN_INFO PFX "Battery monitor ready\n");
	}

	while (1) {
	retry:
		timeout = 30*HZ;

		do {
			timeout = interruptible_sleep_on_timeout(&thr_wait,
								 timeout);

			/* make swsusp happy with our thread */
			if (current->flags & PF_FREEZE) {
				frozen = 1;
				refrigerator(PF_FREEZE);
			}
		} while (!signal_pending(current) &&
			 timeout > 0 &&
			 !recv_data && !thr_wakeup);

		if (signal_pending(current)) {
			printk(KERN_DEBUG PFX "signal pending\n");
			break;
		}

		if (time_to_die) {
			printk(KERN_DEBUG PFX "time to die\n");
			break;
		}

		if (frozen) {
			printk(KERN_DEBUG PFX "waiting for re-start\n");
			
			if (!thr_wakeup) {
				goto retry;
			}

			if (nbp_battmon_setstate(1) == 0) {
				printk(KERN_INFO PFX "Battery monitor ready\n");
			}

			printk(KERN_DEBUG PFX "ok, restarting\n");
			frozen = 0;
		}

		while (recv_data) {
			recv_data--;
		}

		if (thr_wakeup) {
			thr_wakeup = 0;

			if (0) {
				nbp_battmon_setstate(1);
				nbp_spi_reset();
			}

			printk(KERN_INFO PFX "system restarted\n");
		}

		if (timeout <= 0) {
			printk(KERN_INFO PFX "timeout waiting for state, "
				"retrying enable%s\n",
				(jiffies - last_at) > (HZ*60) ? " and reset" : "");

			/* policy on timeout:
			 * first, we'll try re-issuing the battery monitor cmd
			 * second, issue the command and reset the spi
			 */

			if (1) {
				nbp_battmon_setstate(1);

				if ((jiffies - last_at) > (HZ * 60)) {
					nbp_spi_reset();
				}
			}
		}
	}

	printk(KERN_DEBUG PFX "thread exiting\n");

	complete_and_exit (&thr_complete, 0);
}

void nbp_battmon_rx(void *data)
{
	unsigned int *src = data;
	unsigned int *dst;
	struct pcon_battinfo tmp_data;
	unsigned int tmp, res;
	int count;

	dst = (unsigned int *)&tmp_data;

	// need to 16bit byteswap the data

	pr_debug(PFX "%ld seconds since last good measurement\n",
	       (jiffies - last_at) / HZ);
	       
	for (count = 0; count < sizeof(last_data)/sizeof(*dst); count++) {
		tmp = src[count];
		res = tmp >> 8;
		res &= ~0xff00;
		res |= (tmp << 8) & ~0xff0000;
		dst[count] = res;
	}

	pr_debug(PFX "=> magic=%04x, flags=%04x, batt_cap=%d\n",
	       tmp_data.magic, tmp_data.flags,
	       tmp_data.main_batt_capacity_percent);

	if (tmp_data.magic == NETBOOKPRO_PCON_BATTINFO_MAGIC) {
		memcpy(&last_data, &tmp_data, sizeof(last_data));

		last_at = jiffies;
		recv_data++;
		stats.ok++;

		wake_up(&thr_wait);
	} else {
		stats.bad++;
	}
}

int nbp_battmon_getstate(pcon_battinfo_t *info)
{
	unsigned long flags;
	int ret = -ENOENT;

	local_irq_save(flags);
	if (last_data.magic == NETBOOKPRO_PCON_BATTINFO_MAGIC) {
		ret = 0;
		memcpy(info, &last_data, sizeof(*info));
	}
	local_irq_restore(flags);
	return ret;
}


int nbp_battmon_proc_rd_stats(char *page,
			      char **start,
			      off_t off,
			      int count,
			      int *eof,
			      void *data)
{
	unsigned long timediff;
	int i;

	/* work out how long since last management frame received */
	timediff = jiffies - last_at;
	timediff /= (HZ/10);

	i = sprintf(page+0, "Battery Monitoring Stats:\n");
	i+= sprintf(page+i, "\n");

	i+= sprintf(page+i, "Good frames:           %ld\n", stats.ok);
	i+= sprintf(page+i, "Bad frames:            %ld\n", stats.bad);
	i+= sprintf(page+i, "Time since last frame  %ld.%ld seconds\n",
		    timediff / 10, timediff % 10);

	return i;
}
 

int nbp_battmon_proc_rd_drv(char *page,
			    char **start,
			    off_t off,
			    int count,
			    int *eof,
			    void *data)
{
	struct pcon_battinfo bi;
	unsigned long last_wrote;
	unsigned long flags;
	int i;

	local_irq_save(flags);
	last_wrote = last_at;
	memcpy(&bi, &last_data, sizeof(bi));
	local_irq_restore(flags);

	i  = sprintf(page+0, "\nBattery State:");

	i += sprintf(page+i, " Main Battery %sPresent\n",
		     ( bi.flags & NBP_PCON_PWRF_BatteryPreset ) ? "" : "Not ");
	
	i += sprintf(page+i, "               AC %sPlugged In\n",
		     ( bi.flags & NBP_PCON_PWRF_ACPresent) ? "" : "Not " );

	i += sprintf(page+i, "               Charging %s\n",
		     ( bi.flags & NBP_PCON_PWRF_ChargeEnabled ) ? "Enabled" : "Disabled");

	if( !(bi.flags & NBP_PCON_PWRF_BackupBatteryPreset) )
		i += sprintf(page+i, "               Backup Battery Not Present\n");
	if( !(bi.flags & NBP_PCON_PWRF_Calibrated) )
		i += sprintf(page+i, "               Main Battery Not Calibrated\n");
	if( bi.flags & NBP_PCON_PWRF_Calibrating )
		i += sprintf(page+i, "               Main Battery Calibrating\n");
	i += sprintf(page+i, "\nBattery Levels:");
	i += sprintf(page+i, "Maximum Capacity: %d\n",
		     bi.max_capacity );
	i += sprintf(page+i, "               Temperature: %d\n",
		     bi.temperature );
	i += sprintf(page+i, "               Main Battery mV: %d\n",
		     bi.main_batt_voltage );
	i += sprintf(page+i, "               Backup Battery mV: %d\n",
		     bi.backup_batt_voltage );
	i += sprintf(page+i, "               Main Battery Capacity %%: %d\n",
		     bi.main_batt_capacity_percent );
	i += sprintf(page+i, "               Backup Battery Capacity %%: %d\n",
		     bi.backup_batt_capacity_percent );
	i += sprintf(page+i, "               Charge Current: %d\n",
		     bi.charge_current );
	i += sprintf(page+i, "               Discharge Current: %d\n",
		     bi.discharge_current );
	i += sprintf(page+i, "\nCharge State:");

	switch( (nbp_pcon_chargestate_t)bi.charge_state )
	{
        case NBP_PCON_CS_NotCharging:
                i += sprintf(page+i, "  Not Charging\n");
		break;
        case NBP_PCON_CS_Charging:
                i += sprintf(page+i, "  Charging\n");
		break;
        case NBP_PCON_CS_QuickChargeComplete:
                i += sprintf(page+i, "  Quick Charging\n");
		break;
        case NBP_PCON_CS_ChargeComplete:
                i += sprintf(page+i, "  Charging Complete\n");
		break;
	default:
		i += sprintf(page+i, "  Unknown state (%d)\n", bi.charge_state);
		break;
	}

	return i;
}





int __init nbp_battmon_init(void)
{
	printk(KERN_INFO "NetBookPro Battery Monitor, (c) 2004 Simtec Electronics");

	memset(&stats, 0, sizeof(stats));

	init_waitqueue_head (&thr_wait);
	init_waitqueue_head (&thr_sleep);
	init_completion (&thr_complete);

	pid = -1;
	time_to_die = 0;
	thr_wakeup = 0;

	if (1) {
		pid = kernel_thread(nbp_battmon_thread, NULL, 0);
		if (pid < 0) {
			printk(PFX "error %d starting driver thread\n", pid);
			return pid;
		}
		
		printk(KERN_DEBUG "started thread %d\n", pid);
	}
	
	proc_drv_ent = create_proc_read_entry(proc_fname_drv, 0, NULL,
					      nbp_battmon_proc_rd_drv, NULL);

	if (proc_drv_ent == NULL) {
		printk(KERN_ERR PFX "failed to create /proc/%s entry\n",
		       proc_fname_drv);
	} else {
		printk(KERN_INFO PFX "registers /proc/%s\n", proc_fname_drv);
	}

	proc_stat_ent = create_proc_read_entry(proc_fname_stat, 0, NULL,
					  nbp_battmon_proc_rd_stats, NULL);

	if (proc_stat_ent == NULL) {
		printk(KERN_ERR PFX "failed to create /proc/%s entry\n",
		       proc_fname_stat);
	} else {
		printk(KERN_INFO PFX "registers /proc/%s\n", proc_fname_stat);
	}

	return 0;
}

void __exit nbp_battmon_exit(void)
{
	int ret;

	printk(KERN_DEBUG PFX "killing driver thread\n");

	if (pid >= 0) {
		time_to_die = 1;
		wmb();

		ret = kill_proc(pid, SIGTERM, 1);

		if (ret < 0) {
			printk(KERN_ERR PFX "failed to signal thread exit\n");
			return;
		}

		wait_for_completion(&thr_complete);
	}
}

module_init(nbp_battmon_init);
module_exit(nbp_battmon_exit);

MODULE_AUTHOR("Ben Dooks, Simtec Electronics <ben@simtec.co.uk>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Psion Teklogix NetBookPro Battery Monitor");
