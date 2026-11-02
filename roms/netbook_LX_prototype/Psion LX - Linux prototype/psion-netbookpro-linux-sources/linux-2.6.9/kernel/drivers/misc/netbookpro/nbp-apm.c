/* drivers/misc/netbookpro/nbp-apm.c
 *
 * (c) 2004 Simtec Electronics
 * Written by Ben Dooks
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

#include <asm/apm.h>

#include <asm/arch/netbookpro.h>
#include <asm/arch/netbookpro-pcon.h>
#include <linux/i2c-algo-pxa.h>

#include "nbp-power.h"

static void nbp_apm_get_powerstatus(struct apm_power_info *pwr)
{
	struct pcon_battinfo batt;

	pr_debug("nbp_apm_get_powerstatus(%p)\n", pwr);

	if (nbp_battmon_getstate(&batt)) {
		pr_debug("apm: failed to read power info\n");
		return;
	}

	if (batt.flags & NBP_PCON_PWRF_ACPresent)
		pwr->ac_line_status = APM_AC_ONLINE;
	else 
		pwr->ac_line_status = APM_AC_OFFLINE;

	if (batt.flags & NBP_PCON_PWRF_BatteryPreset) {
		if (batt.charge_state == NBP_PCON_CS_Charging) {
			pwr->battery_status = APM_BATTERY_STATUS_CHARGING;
			pwr->battery_flag = APM_BATTERY_FLAG_CHARGING;
		} else {
			pwr->battery_flag = 0;
		}

		if (batt.main_batt_capacity_percent >= 20) {
			pwr->battery_status = APM_BATTERY_STATUS_HIGH;
			pwr->battery_flag |= APM_BATTERY_FLAG_HIGH;
		} else if (batt.main_batt_capacity_percent >= 5) {
			pwr->battery_status = APM_BATTERY_STATUS_LOW;
			pwr->battery_flag |= APM_BATTERY_FLAG_LOW;
		} else {
			pwr->battery_status = APM_BATTERY_STATUS_CRITICAL;
			pwr->battery_flag |= APM_BATTERY_FLAG_CRITICAL;
		}

		if (batt.discharge_current) {
			/*
			 * max_capacity is in mAh, discharge_current is mA,
			 * so mAh/mA = hours not minutes. --rmk
			 */
			pwr->units = APM_UNITS_SECS;
			pwr->time = batt.max_capacity *
				    batt.main_batt_capacity_percent * 36 /
				      batt.discharge_current;

			if (pwr->time >= 32767) {
				pwr->time /= 60;
				pwr->units = APM_UNITS_MINS;
			}
		}

		pwr->battery_life = batt.main_batt_capacity_percent;
	} else {
		pwr->battery_status = APM_BATTERY_STATUS_NOT_PRESENT;
		pwr->battery_flag = APM_BATTERY_FLAG_NOT_PRESENT;
	}
}

/*
 * irq 1 is "power failing"
 * irq 2 is "please suspend"
 */
static irqreturn_t power_irq(int irqno, void *arg, struct pt_regs *regs)
{
	apm_queue_event((apm_event_t)(int)arg);
	return IRQ_HANDLED;
}

static int __init nbp_apm_init(void)
{
	int ret;

	ret = request_irq(IRQ_GPIO(0), power_irq, SA_INTERRUPT,
			  "Battery Warning", (void *)APM_CRITICAL_SUSPEND);
	if (ret) {
		printk(KERN_ERR "Failed to get irq for battery warning\n");
		return ret;
	}

	ret = request_irq(IRQ_GPIO(1), power_irq, SA_INTERRUPT,
			  "Suspend Request", (void *)APM_SYS_SUSPEND);
	if (ret) {
		printk(KERN_ERR "Failed to get irq for suspend request\n");
		free_irq(IRQ_GPIO(0), (void *)APM_CRITICAL_SUSPEND);
		return ret;
	}

	set_irq_type(IRQ_GPIO(0), IRQT_RISING);
	set_irq_type(IRQ_GPIO(1), IRQT_BOTHEDGE);

	apm_get_power_status = nbp_apm_get_powerstatus;

	return 0;
}

static void __exit nbp_apm_exit(void)
{
	free_irq(IRQ_GPIO(1), (void *)APM_SYS_SUSPEND);
	free_irq(IRQ_GPIO(0), (void *)APM_CRITICAL_SUSPEND);

	apm_get_power_status = NULL;
}

module_init(nbp_apm_init);
module_exit(nbp_apm_exit);

MODULE_AUTHOR("Ben Dooks, Simtec Electronics <ben@simtec.co.uk>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Psion Teklogix NetBookPro APM interface");
