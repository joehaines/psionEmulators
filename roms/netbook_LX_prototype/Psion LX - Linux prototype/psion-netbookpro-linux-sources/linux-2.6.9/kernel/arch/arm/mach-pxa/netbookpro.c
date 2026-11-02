/*
 *  linux/arch/arm/mach-pxa/netbookpro.c
 *
 *  Author:     Ben Dooks
 *  Copyright   (c) 2004 Simtec Electronics
 *
 *  Based on:
 *    linux/arch/arm/mach-pxa/lubbock.c
 *
 *    Author:	Nicolas Pitre
 *    Created:	Jun 15, 2001
 *    Copyright:	MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/major.h>
#include <linux/fb.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/usb_sl811.h>

#include <asm/setup.h>
#include <asm/memory.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/irq.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/udc.h>
#include <asm/arch/pxafb.h>
#include <asm/arch/netbookpro.h>
#include <asm/arch/netbookpro-pcon.h>
#include <asm/arch/i2c.h>
#include <asm/arch/mmc.h>

#include "generic.h"

/*extern void printascii(const char *);*/

static void *sleep_save;

int netbookpro_pm_pre_sleep(unsigned long resume_addr)
{
	/* printk(KERN_INFO "suspending to %08lx\n", resume_addr); */

	if (sleep_save == NULL)
		sleep_save = ioremap(0xA0000000, 0x1000);

	if (sleep_save == NULL) {
		printk(KERN_ERR "system: cannot remap suspend save area\n");
		return -ENOMEM;
	}

	writel(resume_addr, sleep_save);
	writel(3,           sleep_save+0xfc);

	/* printascii("netbookpro_pm_pre_sleep: issuing suspend to pcon\n");*/

	nbp_pcon_write2(NETBOOKPRO_PCON_POWER, PCON_PWRSTATE_Suspend);

	/* printascii("(done)\n");*/

	nbp_pwr_waitchange();
	
	/* printascii("netbookpro_pm_pre_sleep: done pcon suspend\n");*/

	PWER = 0;
	PFER = 0;
	PRER = 0;
	PCFR = PCFR_OPDE | PCFR_FS;

	return 0;
}

void netbookpro_pm_post_sleep(void)
{
	/*printascii("netbokkpro_pm_post_sleep:\n");*/

	CKEN |= CKEN14_I2C;
	ISAR = 0xE0 >> 1;

   	ICR = I2C_ICR_INIT;
	ICR |= ICR_SADIE | ICR_ALDIE | ICR_SSDIE;
	ICR &= ~ICR_SCLE;
	ICR |= ICR_IUE;

	/*printascii("netbookpro_pm_post_sleep: going back to run\n");*/

#if 0
	nbp_pcon_command(PCON_POWER_STATE, PCON_PWRSTATE_Run,
			 PCON_FLG_ASYNC, NULL);
#else
	nbp_pcon_write2(NETBOOKPRO_PCON_POWER, PCON_PWRSTATE_Run);
#endif

	/*printascii("netbookpro_pm_post_sleep: waiting for ack\n");*/
	nbp_pwr_waitchange();

	/*printascii("netbookpro_pm_post_sleep: resuming serial power\n");*/

	nbp_pcon_command(I2C_SETSERIALPOWER, 1, PCON_FLG_ASYNC, NULL);

	/*printascii("netbookpro_pm_post_sleep: done\n");*/
}

static void __init netbookpro_init_irq(void)
{
	/*printascii("netbookpro_init_irq:\n");*/
	pxa_init_irq();
}

static u64 spi_dma_mask = ~(u64)0;

static struct platform_device spi_device = {
	.name        = "netbookpro-spi",
	.id          = 0,
	.dev         = {
		.platform_data     = NULL,
		.dma_mask          = &spi_dma_mask,
		.coherent_dma_mask = 0xffffffff
	}
};

static struct resource fb_resources[] = {
	[0] = {
		.start	= 0x14000000 + (1<<21),
		.end	= 0x14000000 + (1<<21) + (1280*1024)-1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= 0x14000000,
		.end	= 0x14000fff,
		.flags	= IORESOURCE_MEM,
	}
};

static struct platform_device fb_device = {
	.name        = "epson-s1d138xxx",
	.id          = 0,
	.dev         = {
		.platform_data     = NULL,
	},
	.resource      = fb_resources,
	.num_resources = ARRAY_SIZE(fb_resources)
};

static struct platform_device keyb_device = {
	.name        = "netbookpro-keyboard",
	.id          = 0,
	.dev         = {
		.platform_data     = NULL,
	},
};

static struct resource sl811_resources[] = {
	[0] = {
		.start	= PXA_CS4_PHYS + 0x000,
		.end	= PXA_CS4_PHYS + 0x100,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= PXA_CS4_PHYS + (1<<16) + 0x000,
		.end	= PXA_CS4_PHYS + (1<<16) + 0x100,
		.flags	= IORESOURCE_MEM,
	},
	[2] = {
		.start  = IRQ_GPIO(2),
		.end    = IRQ_GPIO(2),
		.flags  = IORESOURCE_IRQ,
	}
};

static void netbook2_port_power(struct device *dev, int is_on)
{
	nbp_pcon_command(I2C_SETUSBHOSTPOWER, is_on, PCON_FLG_ASYNC, NULL);
}

static void netbook2_hc_reset(struct device *dev)
{
	GPCR(14) = GPIO_bit(14);
	ndelay(400);	/* nRST for 16+ clocks @ 48MHz */
	GPSR(14) = GPIO_bit(14);
}

static struct sl811_platform_data netbook2_sl811 = {
	// .can_wakeup	= 1,
	.potpg		= 100,
	.port_power	= netbook2_port_power,
	.reset		= netbook2_hc_reset,
};

static struct platform_device sl811_device = {
	.name        = "sl811-hcd",
	.id          = -1,
	.dev         = {
		.platform_data     = &netbook2_sl811,
	},
	.resource      = sl811_resources,
	.num_resources = ARRAY_SIZE(sl811_resources)
};

static struct platform_device nand_device = {
	.name		= "netbookpro-nand",
	.id		= -1,
	.num_resources  = 0,
	.resource	= NULL
};


static struct platform_device *devices[] __initdata = {
	&spi_device,
	&fb_device,
	&keyb_device,
	&sl811_device,
	&nand_device,
};

static LIST_HEAD(pcon_devs);
static LIST_HEAD(pcon_drvs);
static DECLARE_MUTEX(pcon_sem);

struct pcon_device *pcon_dev_add(const char *name, struct device *dev, void *data)
{
	struct pcon_device *pcon;

	pcon = kmalloc(sizeof(struct pcon_device), GFP_KERNEL);
	if (pcon) {
		struct pcon_interface *drv;

		memset(pcon, 0, sizeof(struct pcon_device));
		pcon->dev = dev;
		pcon->data = data;

		down(&pcon_sem);
		list_add(&pcon->node, &pcon_devs);
		list_for_each_entry(drv, &pcon_drvs, node) {
			drv->add(pcon);
		}
		up(&pcon_sem);
	} else {
		pcon = ERR_PTR(-ENOMEM);
	}
	return pcon;
}
EXPORT_SYMBOL(pcon_dev_add);

void pcon_dev_remove(struct pcon_device *pcon)
{
	struct pcon_interface *drv;

	down(&pcon_sem);
	list_del(&pcon->node);
	list_for_each_entry(drv, &pcon_drvs, node) {
		drv->remove(pcon);
	}
	up(&pcon_sem);
	kfree(pcon);
}
EXPORT_SYMBOL(pcon_dev_remove);

int pcon_dev_suspend(struct pcon_device *pcon, int state)
{
	struct pcon_interface *drv;
	int ret = 0;

	down(&pcon_sem);
	list_for_each_entry(drv, &pcon_drvs, node) {
		if (drv->suspend) {
			ret = drv->suspend(pcon, state);
			if (ret)
				break;
		}
	}
	up(&pcon_sem);
	return ret;
}
EXPORT_SYMBOL(pcon_dev_suspend);

int pcon_dev_resume(struct pcon_device *pcon)
{
	struct pcon_interface *drv;
	int ret = 0;

	down(&pcon_sem);
	list_for_each_entry(drv, &pcon_drvs, node) {
		if (drv->resume) {
			ret = drv->resume(pcon);
			if (ret)
				break;
		}
	}
	up(&pcon_sem);
	return ret;
}
EXPORT_SYMBOL(pcon_dev_resume);

int pcon_drv_add(struct pcon_interface *drv)
{
	struct pcon_device *pcon;

	down(&pcon_sem);
	list_add(&drv->node, &pcon_drvs);
	list_for_each_entry(pcon, &pcon_devs, node) {
		drv->add(pcon);
	}
	up(&pcon_sem);
	return 0;
}
EXPORT_SYMBOL(pcon_drv_add);

void pcon_drv_remove(struct pcon_interface *drv)
{
	struct pcon_device *pcon;

	down(&pcon_sem);
	list_del(&drv->node);
	list_for_each_entry(pcon, &pcon_devs, node) {
		drv->remove(pcon);
	}
	up(&pcon_sem);
}
EXPORT_SYMBOL(pcon_drv_remove);


static irqreturn_t (*nbp_irq)(int, void *, struct pt_regs *);

static void nbp_mci_timeout(unsigned long data)
{
	nbp_irq(IRQ_GPIO(9), (void *)data, NULL);
}

static struct timer_list nbp_irq_timer = TIMER_INITIALIZER(nbp_mci_timeout, 0, 0);

static irqreturn_t nbp_mci_irq(int irq, void *data, struct pt_regs *regs)
{
	mod_timer(&nbp_irq_timer, jiffies + 5 * HZ/100);
	return IRQ_HANDLED;
}

static int nbp_mci_init(struct device *dev, irqreturn_t (*fn)(int, void *, struct pt_regs *), void *data)
{
	int ret;

	nbp_irq = fn;
	nbp_irq_timer.data = (unsigned long)data;

	ret = request_irq(IRQ_GPIO(9), nbp_mci_irq, 0, "pxamci", data);
	if (ret == 0)
		set_irq_type(IRQ_GPIO(9), IRQT_BOTHEDGE);
	return ret;
}

static void nbp_mci_setpower(struct device *dev, unsigned int vdd_bits)
{
	nbp_pcon_command(I2C_SETMMCPOWER, vdd_bits ? 1 : 0, 0, NULL);
}

static void nbp_mci_exit(struct device *dev, void *data)
{
	free_irq(IRQ_GPIO(9), data);
}

static struct pxamci_platform_data nbp_mci_info = {
	.ocr_mask = MMC_VDD_32_33 | MMC_VDD_33_34,
	.init	  = nbp_mci_init,
	.setpower = nbp_mci_setpower,
	.exit	  = nbp_mci_exit,
};

static void __init netbookpro_init(void)
{
	/*printascii("netbookpro_init:\n");*/

	nbp_pcon_command(I2C_SETSERIALPOWER, 1, PCON_FLG_ASYNC, NULL);
        platform_add_devices(devices, ARRAY_SIZE(devices));

        pxa_set_mci_info(&nbp_mci_info);
}

static struct map_desc netbookpro_io_desc[] __initdata = {
};

#define show_reg(x) printk(KERN_INFO "%s = %08x\n", #x, x)

static void __init netbookpro_map_io(void)
{
	pxa_map_io();
	iotable_init(netbookpro_io_desc, ARRAY_SIZE(netbookpro_io_desc));

	show_reg(MDCNFG);
	show_reg(MDREFR);
	show_reg(SXCNFG);

	/*printascii("sending command to setserialpower on\n");*/
}

MACHINE_START(NETBOOKPRO, "Psion Teklogix NetBookPro")
	MAINTAINER("Ben Dooks, Vincent Sanders. Simtec Electronics <linux@simtec.co.uk>")
	BOOT_MEM(0xa0000000, 0x40000000, io_p2v(0x40000000))
        BOOT_PARAMS(0xA0400100)
	MAPIO(netbookpro_map_io)
	INITIRQ(netbookpro_init_irq)
	INITTIME(pxa_init_time)
	INIT_MACHINE(netbookpro_init)
MACHINE_END
