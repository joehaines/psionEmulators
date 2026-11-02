/*
 * linux/drivers/pcmica/pxa2xx_netbookpro.c
 *
 * Based on: linux/drivers/pcmcia/pxa2xx_mainstone.c
 *
 * NetBookPro PCMCIA specific routines.
 *
 * Modifications for NetBookPro by:
 * Author:      Ben Dooks
 * Modified:    Jun 08, 2004
 * Copyright:   Simtec Electronics <linux@simtec.co.uk>
 *
 * Original:
 * Created:	May 12, 2004
 * Author:	Nicolas Pitre
 * Copyright:	MontaVista Software Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>

#include <pcmcia/ss.h>

#include <asm/hardware.h>
#include <asm/irq.h>

#include "soc_common.h"

#define PFX "nbp-pcmia: "

#define dbg(x...)	do { } while (0)

/* GPIO Lines
 * Socket:         0              1
 *   CardDetect    75             73
 *   StatusChange  76             72
 *   Ready/IRQ     77             71
 *   Reset         74             70
 */

/* IRQ lines for our Slots */

struct our_socket {
	unsigned long irq;        /* irq line for socket */
	unsigned long detect;     /* gpio bit for detect */
	unsigned long ready;      /* gpio bit for ready/irq */
	unsigned long reset;      /* gpio bit for card reset */
};

/* define the IO lines for each socket */

static struct our_socket sockets[2] = {
	[1] = {
		.irq       = IRQ_GPIO(77),
		.detect    = 1 << (75 - 64),
		.ready     = 1 << (77 - 64),
		.reset     = 1 << (74 - 64),
	},
	[0] = {
		.irq       = IRQ_GPIO(71),
		.detect    = 1 << (73 - 64),
		.ready     = 1 << (71 - 64),
		.reset     = 1 << (70 - 64),
	}
};

static struct pcmcia_irqs irqs[] = {
	{ 1, IRQ_GPIO(75), "PCMCIA Change Detect" },
	{ 0, IRQ_GPIO(73), "CF Change Detect" },
	{ 1, IRQ_GPIO(76), "PCMCIA Status Change" },
	{ 0, IRQ_GPIO(72), "CF Status Change" },
};

static int nbp_pcmcia_hw_init(struct soc_pcmcia_socket *skt)
{
	struct our_socket *oursock = &sockets[skt->nr];

	dbg(PFX "initialising socket %d, irq %ld\n",
	    skt->nr, oursock->irq);
	
	skt->irq = oursock->irq;

	return soc_pcmcia_request_irqs(skt, irqs, ARRAY_SIZE(irqs));
}

static void nbp_pcmcia_hw_shutdown(struct soc_pcmcia_socket *skt)
{
	soc_pcmcia_free_irqs(skt, irqs, ARRAY_SIZE(irqs));
}

static void nbp_pcmcia_socket_state(struct soc_pcmcia_socket *skt,
				    struct pcmcia_state *state)
{
	struct our_socket *oursock = &sockets[skt->nr];
	unsigned long gpio;

	gpio = GPLR2;

	state->detect = (gpio & oursock->detect) ? 0 : 1;
	state->ready  = (gpio & oursock->ready) ? 1 : 0;

	/* none of these are available on this implementation */
	state->bvd1   = 1;
	state->bvd2   = 1;
	state->vs_3v  = 1;
	state->vs_Xv  = 0;
	state->wrprot = 0;

	dbg(PFX "skt[%d]: gpio=%08lx, detect=%d, ready=%d\n",
	    skt->nr, gpio, state->detect, state->ready);
}

static int nbp_pcmcia_configure_socket(struct soc_pcmcia_socket *skt,
				       const socket_state_t *state)
{
	struct our_socket *oursock = &sockets[skt->nr];
	int ret = 0;

	dbg(PFX "configure_socket %d:\n", skt->nr);

	if (skt->nr != 0 && skt->nr != 1) {
		printk("nbp_pcmcia_configure_socket: invalid socket %d\n", skt->nr);
		return -1;
	}

	/* we currently haven't got the code sorted out to 
	 * send the PCon chip any configuration info for the 
	 * socket
	 */

	if (state->flags & SS_RESET) {
		/* assert reset for socket */
		dbg(PFX "socket %d: reset on\n", skt->nr);
		GPCR2 = oursock->reset;

	} else {
		/* de-assert reset */

		dbg(PFX "socket %d: reset off\n", skt->nr);
		GPSR2 = oursock->reset;
	}

	return ret;
}

static void nbp_pcmcia_socket_init(struct soc_pcmcia_socket *skt)
{
	soc_pcmcia_enable_irqs(skt, irqs, ARRAY_SIZE(irqs));
}

static void nbp_pcmcia_socket_suspend(struct soc_pcmcia_socket *skt)
{
	soc_pcmcia_disable_irqs(skt, irqs, ARRAY_SIZE(irqs));
}

static struct pcmcia_low_level nbp_pcmcia_ops = {
	.owner			= THIS_MODULE,
	.hw_init		= nbp_pcmcia_hw_init,
	.hw_shutdown		= nbp_pcmcia_hw_shutdown,
	.socket_state		= nbp_pcmcia_socket_state,
	.configure_socket	= nbp_pcmcia_configure_socket,
	.socket_init		= nbp_pcmcia_socket_init,
	.socket_suspend		= nbp_pcmcia_socket_suspend,
	.nr			= 2,
	.features		= SS_CAP_NOPOWER,
};

static struct platform_device *nbp_pcmcia_device;

static int __init nbp_pcmcia_init(void)
{
	int ret;

	printk("NetBookPro: installing PCMICA driver device\n");

	nbp_pcmcia_device = kmalloc(sizeof(*nbp_pcmcia_device), GFP_KERNEL);
	if (!nbp_pcmcia_device)
		return -ENOMEM;

	memset(nbp_pcmcia_device, 0, sizeof(*nbp_pcmcia_device));

	nbp_pcmcia_device->name = "pxa2xx-pcmcia";
	nbp_pcmcia_device->dev.platform_data = &nbp_pcmcia_ops;

	ret = platform_device_register(nbp_pcmcia_device);
	if (ret)
		kfree(nbp_pcmcia_device);

	return ret;
}

static void __exit nbp_pcmcia_exit(void)
{
	/*
	 * This call is supposed to free our nbp_pcmcia_device.
	 * Unfortunately platform_device don't have a free method, and
	 * we can't assume it's free of any reference at this point so we
	 * can't free it either.
	 */
	platform_device_unregister(nbp_pcmcia_device);
}

module_init(nbp_pcmcia_init);
module_exit(nbp_pcmcia_exit);

MODULE_LICENSE("GPL");
