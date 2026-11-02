/*
 * pxa-wm97xx-touch.c  --  PXA Continuous Touch screen driver for 
 *                         Wolfson WM97xx AC97 Codecs.
 *
 * Copyright 2004 Wolfson Microelectronics PLC.
 * Author: Liam Girdwood
 *         liam.girdwood@wolfsonmicro.com or linux@wolfsonmicro.com
 * Parts Copyright : Ian Molton <spyro@f2s.com>
 *                   Andrew Zabolotny <zap@homelink.ru>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Notes:
 *     This code uses an API exposed by ac97_plugin_wm97xx to capture touch
 *     data in a continuous manner on the Intel XScale archictecture
 * 
 *  Features:
 *       - codecs supported:- WM9705, WM9712, WM9713
 *       - processors supported:- Intel XScale PXA25x, PXA26x, PXA27x
 *
 *  Revision history
 *    18th Aug 2004   Initial version.
 * 
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/arch/pxa-regs.h>

#include "wm97xx.h"

#define TS_NAME			"pxa_wm97xx_touch"
#define WM_TS_VERSION		"0.4"

#define WM97XX_PXA_SLOT		5

/*
 * The WM97xx codecs can signal a pen down event by either sending an irq over
 * the AC97 link (not wm9705) or by asserting a pen down GPIO to interrupt the 
 * OS. Set WM_PEN_DOWN_GPIO to 1 if you are using the dedicated GPIO to signal
 * pen down events, otherwise set to 0 if you want to signal over the AC link.
 */ 
#define WM_PEN_DOWN_GPIO 0

/*
 * Pen sampling frequency in continuous mode.
 *
 * This parameter needs an additional architecture-dependent driver
 * such as pxa-ac97-wm97xx.
 *
 * The sampling frequency is given by a number in range 0-3:
 *
 * 0: 93.75Hz  1: 187.5Hz  2: 375Hz  3: 750Hz
 */
static int cont_rate = 1;
module_param(cont_rate, int, 0);
MODULE_PARM_DESC(cont_rate, "Sampling rate in continuous mode (0-3, default 2)");


/*
 * Pen down detection.
 * 
 * This driver can either poll or use an interrupt to indicate a pen down
 * event. If the irq request fails then it will fall back to polling mode.  
 */
static int pen_int = 1;
module_param(pen_int, int, 0);
MODULE_PARM_DESC(pen_int, "Pen down detection (1 = interrupt, 0 = polling)");

/* flush AC97 slot 5 FIFO on pxa machines */
#define PXA_FLUSH_FIFO    \
	do {                  \
		int count = 0;    \
		for (; count < 16; count++) MODR; \
		} while (0); 
			
static struct wm97xx_device *dev;		

static int pxa_wm97xx_cont_read (struct input_dev* idev, int pen_down_time, int pressure)
{
	u16 x, y, p = 0x100 | WM97XX_ADCSEL_PRES;
	int count = 0, find_x = 3;
		 
	/* sometimes there is stale data in the FIFO just after a pen down */
	if (pen_down_time < 6 ) {
		PXA_FLUSH_FIFO;
		return RC_PENDOWN;
	}

	/* find the first X and sync */
	while (find_x--) {
		x = MODR;
		if ((x & 0x7000) == WM97XX_ADCSEL_X)
		 	break;
	}

	/* are we now syncronised X,Y[,P],X,Y[,P],X... ? */
	if (!find_x)
		return RC_PENDOWN;
		
	/* read Y [and pressure] */
	y = MODR & 0xffff;
	if (pressure)
		p = MODR & 0xffff;
	
#ifdef CONFIG_PXA27x
	while (MISR & (1 << 2)) {
#else
	while (count < 16) {
#endif
		/* are samples valid */
		if ((x & 0x7000) != WM97XX_ADCSEL_X || 
			(y & 0x7000) != WM97XX_ADCSEL_Y ||
			(p & 0x7000) != WM97XX_ADCSEL_PRES)
				goto next;
		
		/* is pen down pressure great enough ? */		
		if (pressure && ((p & 0xfff) > pressure))
			goto next;
		
		/* coordinate is good */
		//printk("x %x y %x p %x\n", x,y,p);
		input_report_abs (idev, ABS_X, x & 0xfff);
		input_report_abs (idev, ABS_Y, y & 0xfff);
		input_report_abs (idev, ABS_PRESSURE, p & 0xfff);
		input_sync (idev);

next:		
		x = MODR;
		y = MODR;
		count += 2;
		if (pressure) {
			p = MODR;
			count++;
		}
	}

	return RC_PENDOWN;
}

#ifdef CONFIG_PXA27x
/*
 * Sometimes the PXA27x cannot reset sticky codec GPIO's. In this case we
 * also need to reset the GPIO in codec space.
 */
static int pxa27x_clear_sticky(int status)
{
	volatile u32 *reg_addr;
	u32 gsr;

	reg_addr = (u32 *)&PAC_REG_BASE + (0x54 >> 1);
	*reg_addr = (status & ~WM97XX_GPIO_13) << 1;
		
	udelay(50);
    gsr = GSR;
    GSR = gsr & GSR_CDONE;
 
	if (!(gsr & GSR_CDONE)) {
		printk(KERN_CRIT "%s: write codec register timeout.\n", __FUNCTION__);
        return 0;
	}
	return 1;
}
#endif


/*
 * PXA arch pen down handler.
 */
static int pxa_pen_handler(int status)
{
	u32 gsr;
#ifdef CONFIG_PXA27x
	pxa27x_clear_sticky(status);
#endif
	gsr = GSR;
	GSR = gsr & GSR_GSCI;
	return 0;
}
	

static int pxa_wm97xx_probe(void)
{
	/* codec specific irq config */
	if (pen_int) {
		switch (wm97xx_id & 0xffff) {
			case WM9705_ID2:
				wm97xx_set_pen_irq(dev, IRQ_GPIO(4));
				set_irq_type(IRQ_GPIO(4), IRQT_BOTHEDGE);
				break;
			case WM9712_ID2:
			case WM9713_ID2:
				/* enable pen down interrupt */
#if WM_PEN_DOWN_GPIO
				/* use the PEN_DOWN GPIO */
				wm97xx_set_pen_irq(dev, MAINSTONE_PEN_IRQ);
				wm97xx_config_codec_gpio(dev, WM97XX_GPIO_3, WM97XX_GPIO_OUT,
					WM97XX_GPIO_POL_HIGH, WM97XX_GPIO_NOTSTICKY, WM97XX_GPIO_NOWAKE);
#else
				/* use the virtual PEN_DOWN GPIO to send irq over AC97 */
				wm97xx_set_codec_irq(dev, IRQ_AC97, 1);
				wm97xx_request_codec_event(dev, WM97XX_GPIO_13, pxa_pen_handler);
				GCR |= GCR_GIE;
#endif
				break;
			default:
				printk("pen down irq not supported on this device\n");
				pen_int = 0;
				break;
		}
	}
	
	/* Go you big red fire engine */
	wm97xx_set_continuous_mode (dev, 1, WM97XX_PXA_SLOT, cont_rate);
	return 0;
}

static int pxa_wm97xx_remove(void)
{
	/* codec specific deconfig */
	if (pen_int) {
		switch (wm97xx_id & 0xffff) {
			case WM9705_ID2:
				wm97xx_clear_pen_irq(dev);
				break;
			case WM9712_ID2:
			case WM9713_ID2:
				/* disable interrupt */
#if WM_PEN_DOWN_GPIO
				wm97xx_clear_pen_irq(dev);
#else
				if (wm97xx_clear_codec_irq(dev) == 0)
					GCR &= ~GCR_GIE;
				wm97xx_free_codec_event(dev, WM97XX_GPIO_13);
#endif
				break;
		}
	}
	
	wm97xx_set_continuous_mode (dev, 0, 0, 0);
	return 0;
}

#if defined(CONFIG_PM)
static int pxa_wm97xx_suspend(u32 state, u32 level)
{
	return 0;
}

static int pxa_wm97xx_resume(u32 level)
{
	return 0;
}
#else
#define pxa_wm97xx_suspend NULL
#define pxa_wm97xx_resume NULL
#endif

static struct wm97xx_device pxa_wm97xx_touch_driver = {
        .name   = "pxa-wm97xx-touch",
        .probe  = pxa_wm97xx_probe,
        .remove = pxa_wm97xx_remove,
        .suspend= pxa_wm97xx_suspend,
        .resume = pxa_wm97xx_resume,
		.cont_read = pxa_wm97xx_cont_read,
};

static int __init pxa_wm97xx_touch_init(void)
{
	dev = &pxa_wm97xx_touch_driver;
	return wm97xx_driver_register(dev);
}

static void __exit pxa_wm97xx_touch_exit(void)
{
	wm97xx_driver_unregister(&pxa_wm97xx_touch_driver);
	dev = NULL;
}

/* Module information */
MODULE_AUTHOR("Liam Girdwood <liam.girdwood@wolfsonmicro.com>");
MODULE_DESCRIPTION("AC97 continuous touch driver for PXA25x,PXA26x and PXA27x");
MODULE_LICENSE("GPL");

module_init(pxa_wm97xx_touch_init);
module_exit(pxa_wm97xx_touch_exit);
