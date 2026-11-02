/*
 * ac97_plugin_wm97xx.c  --  Touch screen driver for Wolfson WM9705 and WM9712
 *                           AC97 Codecs.
 *
 * Copyright 2003, 2004 Wolfson Microelectronics PLC.
 * Author: Liam Girdwood
 *         liam.girdwood@wolfsonmicro.com or linux@wolfsonmicro.com
 * Parts Copyright : Ian Molton <spyro@f2s.com>
 *                   Andrew Zabolotny <zap@homelink.ru>
 *                   Russell King
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
 *
 *  Features:
 *       - supports WM9705, WM9712
 *       - polling mode
 *       - coordinate polling
 *       - continuous mode (arch-dependent)
 *       - adjustable rpu/dpp settings
 *       - adjustable pressure current
 *       - adjustable sample settle delay
 *       - 4 and 5 wire touchscreens (5 wire is WM9712 only)
 *       - pen down detection
 *       - battery monitor
 *       - sample AUX adc's
 *       - power management
 *
 *  TODO:
 *       - adjustable sample rate
 *       - AUX adc in coordinate / continous modes
 *       - Codec GPIO
 *
 *  Revision history
 *    7th May 2003   Initial version.
 *    6th June 2003  Added non module support and AC97 registration.
 *   18th June 2003  Added AUX adc sampling.
 *   23rd June 2003  Did some minimal reformatting, fixed a couple of
 *                   locking bugs and noted a race to fix.
 *   24th June 2003  Added power management and fixed race condition.
 *   10th July 2003  Changed to a misc device.
 *   31st July 2003  Moved TS_EVENT and TS_CAL to wm97xx.h
 *    8th Aug  2003  Added option for read() calling wm97xx_sample_touch()
 *                   because some ac97_read/ac_97_write call schedule()
 *    7th Nov  2003  Added Input touch event interface, stanley.cai@intel.com
 *   13th Nov  2003  Removed h3600 touch interface, added interrupt based
 *                   pen down notification and implemented continous mode
 *                   on XScale arch.
 *   16th Nov  2003  Ian Molton <spyro@f2s.com>
 *                   Modified so that it suits the new 2.6 driver model.
 *   25th Jan  2004  Andrew Zabolotny <zap@homelink.ru>
 *                   Implemented IRQ-driven pen down detection, implemented
 *                   the private API meant to be exposed to platform-specific
 *                   drivers, reorganized the driver so that it supports
 *                   an arbitrary number of devices.
 *    1st Feb  2004  Moved continuous mode handling to a separate
 *                   architecture-dependent file. For now only PXA
 *                   built-in AC97 controller is supported (pxa-ac97-wm97xx.c).
 *    11th Feb 2004  Reduced CPU usage by keeping a cached copy of both
 *                   digitizer registers instead of reading them every time.
 *                   A reorganization of the whole code for better
 *                   error handling.
 *    17th Apr 2004  Added BMON support.
 *    17th Nov 2004  Added codec GPIO, codec event handling (real and virtual
 *                   GPIOs) and 2.6 power management. 
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/pm.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/list.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#include "wm97xx.h"

#define TS_NAME			"wm97xx"
#define WM_TS_VERSION	"0.23"
#define DEFAULT_PRESSURE	0xb0c0

/*
 * Debug
 */
#if 0
#define dbg(format, arg...) printk(KERN_DEBUG TS_NAME ": " format "\n" , ## arg)
#else
#define dbg(format, arg...)
#endif
#define err(format, arg...) printk(KERN_ERR TS_NAME ": " format "\n" , ## arg)
#define info(format, arg...) printk(KERN_INFO TS_NAME ": " format "\n" , ## arg)
#define warn(format, arg...) printk(KERN_WARNING TS_NAME ": " format "\n" , ## arg)

#if defined(CONFIG_PROC_FS) && !defined(CONFIG_EMBEDDED)
#  define PROC_FS_SUPPORT
#endif

#define PROC_FS_SUPPORT

/*
 * Module parameters
 */

static int useless;

/*
 * Set the codec sample mode.
 *
 * The WM9712 can sample touchscreen data in 3 different operating
 * modes. i.e. polling, coordinate and continous.
 *
 * Polling:      The driver polls the codec and issues 3 seperate commands
 *               over the AC97 link to read X,Y and pressure.
 *
 * Coordinate:   The driver polls the codec and only issues 1 command over
 *               the AC97 link to read X,Y and pressure. This mode has
 *               strict timing requirements and may drop samples if
 *               interrupted. However, it is less demanding on the AC97
 *               link. Note: this mode requires a larger delay than polling
 *               mode.
 *
 * Continuous:   The codec automatically samples X,Y and pressure and then
 *               sends the data over the AC97 link in slots. This is the
 *               same method used by the codec when recording audio.
 *               This mode is not selectable by this parameter because
 *               it needs separate architecture-dependent support. The
 *               following drivers will add continuouse mode support:
 *
 *               pxa-wm97xx-touch.c - for Intel PXA built-in AC97 controller.
 */
static int mode;
module_param (mode, int, 0);
MODULE_PARM_DESC(mode, "WM97XX operation mode (0:polling 1:coordinate)");

/*
 * WM9712 - Set internal pull up for pen detect.
 *
 * Pull up is in the range 1.02k (least sensitive) to 64k (most sensitive)
 * i.e. pull up resistance = 64k Ohms / rpu.
 *
 * Adjust this value if you are having problems with pen detect not
 * detecting any down events.
 */
static int rpu;
module_param(rpu, int, 0);
MODULE_PARM_DESC(rpu, "Set internal pull up resitor for pen detect.");

/*
 * WM9705 - Pen detect comparator threshold.
 *
 * 0 to Vmid in 15 steps, 0 = use zero power comparator with Vmid threshold
 * i.e. 1 =  Vmid/15 threshold
 *      15 =  Vmid/1 threshold
 *
 * Adjust this value if you are having problems with pen detect not
 * detecting any down events.
 */
static int pdd = 8;
module_param(pdd, int, 0);
MODULE_PARM_DESC(pdd, "Set pen detect comparator threshold");

/*
 * Set current used for pressure measurement.
 *
 * Set pil = 2 to use 400uA
 *     pil = 1 to use 200uA and
 *     pil = 0 to disable pressure measurement.
 *
 * This is used to increase the range of values returned by the adc
 * when measureing touchpanel pressure.
 */
static int pil = 0;
module_param(pil, int, 0);
MODULE_PARM_DESC(pil, "Set current used for pressure measurement.");

/*
 * Set threshold for pressure measurement.
 *
 * Pen down pressure below threshold is ignored.
 */
static int pressure = DEFAULT_PRESSURE & 0xfff;
module_param(pressure, int, 0);
MODULE_PARM_DESC(pressure, "Set threshold for pressure measurement.");

/*
 * WM9712 - Set five_wire = 1 to use a 5 wire touchscreen.
 *
 * NOTE: Five wire mode does not allow for readback of pressure.
 */
static int five_wire;
module_param(five_wire, int, 0);
MODULE_PARM_DESC(five_wire, "Set to '1' to use 5-wire touchscreen.");

/*
 * Set adc sample delay.
 *
 * For accurate touchpanel measurements, some settling time may be
 * required between the switch matrix applying a voltage across the
 * touchpanel plate and the ADC sampling the signal.
 *
 * This delay can be set by setting delay = n, where n is the array
 * position of the delay in the array delay_table below.
 * Long delays > 1ms are supported for completeness, but are not
 * recommended.
 */
static int delay = 4;
module_param(delay, int, 0);
MODULE_PARM_DESC(delay, "Set adc sample delay.");

/*
 * Coordinate inversion control.
 *
 * On some hardware the coordinate system of the touchscreen and the
 * coordinate system of the graphics controller are different. A well-written
 * calibration tool won't depend on that; however for compatibility with
 * some old apps sometimes it is required to bring both coordinate
 * systems together. That's what this parameter is for.
 *
 * Bit 0: if 1, the X coordinate is inverted
 * Bit 1: if 1, the Y coordinate is inverted
 */
static int invert = 0;
module_param(invert, int, 0);
MODULE_PARM_DESC(invert, "Set bit 0 to invert X coordinate, set bit 1 to invert Y coordinate");

/*
 * Touchscreen absolute values
 *
 * These parameters are used to help the input layer discard out of
 * range readings and reduce jitter etc. 
 * 
 *   o min, max:- indicate the min and max values your touch screen returns
 *   o fuzz:- use a higher number to reduce jitter
 *  	
 * The default values correspond to Mainstone II in QVGA mode
 *	
 * Please read 
 * Documentation/input/input-programming.txt for more details.	
 */

static int abs_x[3] = {350,3900,5};
module_param_array(abs_x, int, useless, 0);
MODULE_PARM_DESC(abs_x, "Touchscreen absolute X min, max, fuzz");

static int abs_y[3] = {320,3750,40};
module_param_array(abs_y, int, useless, 0);
MODULE_PARM_DESC(abs_y, "Touchscreen absolute Y min, max, fuzz");

static int abs_p[3] = {0,150,4};
module_param_array(abs_p, int, useless, 0);
MODULE_PARM_DESC(abs_p, "Touchscreen absolute Pressure min, max, fuzz");

static u16 wm97xx_adcsel [2][4] =
{
	/* WM9705 */
	{ WM9705_ADCSEL_BMON, WM9705_ADCSEL_AUX, WM9705_ADCSEL_PHONE, WM9705_ADCSEL_PCBEEP },
	/* WM9712 */
	{ WM9712_ADCSEL_COMP1, WM9712_ADCSEL_COMP2, WM9712_ADCSEL_BMON, WM9712_ADCSEL_WIPER },
};

typedef struct {
	unsigned adcsel;		/* ADC select mask; !=0 -> enabled */
	unsigned next_jiffies;		/* Next time mark to collect data at */
	unsigned period;		/* Interval in jiffies to collect data */
	wm97xx_ac_prepare func;		/* Before/after conversion callback */
	int value;			/* Last value sampled (-1 = no data yet) */
} wm97xx_auxconv_data_t;

typedef struct {
	u16 dig1, dig2;						/* Cached digitizer registers */
	spinlock_t lock;					/* Hardware lock */
	struct ac97_codec *codec;			/* AC97 codec */
	struct semaphore sem;
	struct completion ts_init;
	struct completion ts_exit;
	volatile struct task_struct *ts_task;
	codec_read_t cont_read_sample;		/* continuous mode reader */
	wm97xx_auxconv_data_t aux_conv [4]; /* AUX data */
	struct completion aux_exit;
	struct completion aux_init;
	volatile struct task_struct *aux_task;
	int aux_task_ref;
	wait_queue_head_t aux_wait;			/* AUX pen-up wait queue */
	unsigned int pen_irq;				/* Pen IRQ number in use */
	volatile int pen_irq_count;
	unsigned int codec_irq;				/* Codec IRQ number in use */
	volatile int codec_irq_count;
	wait_queue_head_t pen_irq_wait;		/* Pen IRQ wait queue */
	int codec_irq_ref_count;			/* Number of gpio irq users */
	int adc_errs;						/* Sample read back errors */
	unsigned int pen_is_down;			/* Pen down time */
	unsigned int use_count;
	struct workqueue_struct *pen_irq_workq;
	struct work_struct pen_event_work;
	struct workqueue_struct *codec_event_workq;
	struct work_struct codec_event_work;
	codec_event_t codec_event[WM97XX_MAX_GPIO]; /* codec event handlers */
#if defined(PROC_FS_SUPPORT)
	struct proc_dir_entry *wm97xx_ts_ps;
#endif
	/* Bitfields grouped here to get some space gain */
	unsigned is_wm9712:1;			/* are we a WM9705/12 */
	unsigned pen_probably_down:1;	/* used in polling and coordinate modes */
	unsigned pen_is_up:1;			/* pen is up */
	unsigned invert_x:1;			/* invert X coords */
	unsigned invert_y:1;			/* invert Y coords */
	unsigned aux_waiting:1;			/* aux measurement waiting */
	unsigned ac97_link:1;			/* interrupt over AC link */
	unsigned probe:1;				/* probe complete */
#if defined(CONFIG_PM)
	unsigned phone_pga:5;			/* PGA settings for PM */
	unsigned line_pga:16;
	unsigned mic_pga:16;
#endif
} wm97xx_ts_t;

/* Shortcut since we have a lot of such references */
#define CODEC ts.codec

/* 
 * We only support a single codec, however we may need to provide 
 * similtanious access to the device e.g. X and detect-stylus (GPE)
 */
static wm97xx_ts_t ts;
static struct input_dev wm97xx_input;
static LIST_HEAD(wm97xx_drivers);

/*
 * Codec ID registers
 */
u32 wm97xx_id;
EXPORT_SYMBOL_GPL (wm97xx_id);

/*
 * ADC sample delay times in uS
 */
static const int delay_table[16] = {
	21,    // 1 AC97 Link frames
	42,    // 2
	84,    // 4
	167,   // 8
	333,   // 16
	667,   // 32
	1000,  // 48
	1333,  // 64
	2000,  // 96
	2667,  // 128
	3333,  // 160
	4000,  // 192
	4667,  // 224
	5333,  // 256
	6000,  // 288
	0      // No delay, switch matrix always on
};

/*
 * Delay after issuing a POLL command.
 *
 * The delay is 3 AC97 link frames + the touchpanel settling delay
 */
static inline void poll_delay(int d)
{
	udelay (3 * AC97_LINK_FRAME + delay_table [d]);
}

static inline int is_pden (void)
{
	u16 pden_mask;

	if (ts.is_wm9712)
		pden_mask = WM9712_PDEN;
	else
		pden_mask = WM9705_PDEN;

	return ts.dig2 & pden_mask;
}

/*
 * Read a sample from the adc in polling mode.
 */
static int wm97xx_poll_sample (int adcsel, int *sample)
{
	int timeout = 5 * delay;

	if (!ts.pen_probably_down) {
		u16 data = CODEC->codec_read(CODEC, AC97_WM97XX_DIGITISER_RD);
		if (!(data & WM97XX_PEN_DOWN))
			return RC_PENUP;
		ts.pen_probably_down = 1;
	}

	/* set up digitiser */
	CODEC->codec_write(CODEC, AC97_WM97XX_DIGITISER1, adcsel | WM97XX_POLL | WM97XX_DELAY(delay));

	/* wait 3 AC97 time slots + delay for conversion */
	poll_delay (delay);

	/* wait for POLL to go low */
	while ((CODEC->codec_read(CODEC, AC97_WM97XX_DIGITISER1) & WM97XX_POLL) && timeout) {
		udelay(AC97_LINK_FRAME);
		timeout--;
	}

	if (timeout <= 0) {
		/* If PDEN is set, we can get a timeout when pen goes up */
		if (is_pden())
			ts.pen_probably_down = 0;
		else {
			ts.adc_errs++;
			dbg ("adc sample timeout");
		}
		return 0;
	}

	*sample = CODEC->codec_read(CODEC, AC97_WM97XX_DIGITISER_RD);

	/* check we have correct sample */
	if ((*sample & WM97XX_ADCSEL_MASK) != adcsel) {
		ts.adc_errs++;
		dbg ("adc wrong sample, read %x got %x", adcsel,
		     *sample & WM97XX_ADCSEL_MASK);
		return 0;
	}

	if (!(*sample & WM97XX_PEN_DOWN)) {
		ts.pen_probably_down = 0;
		return RC_PENUP;
	}

	return RC_VALID;
}

/*
 * Read sample data (x,y,[p]) from the adc in coordinate mode.
 */
static int wm97xx_poll_coord_touch(wm97xx_data_t *data)
{
	u16 dig1;
	int timeout = 5 * delay;

	if (!ts.pen_probably_down) {
		u16 data = CODEC->codec_read(CODEC, AC97_WM97XX_DIGITISER_RD);
		if (!(data & WM97XX_PEN_DOWN))
			return RC_PENUP;
		ts.pen_probably_down = 1;
	}

	/* set up digitiser */
	dig1 = (ts.dig1 & ~WM97XX_ADCSEL_MASK) | WM97XX_ADCSEL_PRES;
	CODEC->codec_write(CODEC, AC97_WM97XX_DIGITISER1,
			   (ts.dig1 = dig1) | WM97XX_POLL);

	/* wait 3 AC97 time slots + delay for conversion */
	poll_delay(delay);

	/* read X then wait for 1 AC97 link frame + settling delay */
	data->x = CODEC->codec_read(CODEC, AC97_WM97XX_DIGITISER_RD);
	udelay (AC97_LINK_FRAME + delay_table[delay]);

	/* read Y */
	data->y = CODEC->codec_read(CODEC, AC97_WM97XX_DIGITISER_RD);

	/* wait for POLL to go low and then read pressure */
	while ((CODEC->codec_read(CODEC, AC97_WM97XX_DIGITISER1) & WM97XX_POLL)&& timeout) {
			udelay(AC97_LINK_FRAME);
			timeout--;
	}

	if (timeout <= 0) {
		if (is_pden())
			ts.pen_probably_down = 0;
		else {
			ts.adc_errs++;
			dbg ("adc sample timeout");
		}
		return 0;
	}

	/* read pressure */
	data->p = CODEC->codec_read(CODEC, AC97_WM97XX_DIGITISER_RD);

	/* check we have correct samples */
	if (((data->x & WM97XX_ADCSRC_MASK) != 0x1000) ||
	    ((data->y & WM97XX_ADCSRC_MASK) != 0x2000) ||
	    ((data->p & WM97XX_ADCSRC_MASK) != 0x3000)) {
		ts.adc_errs++;
		dbg ("wrong sample order");
		return 0;
	}

	if (!(data->x & WM97XX_PEN_DOWN)) {
		ts.pen_probably_down = 0;
		return RC_PENUP;
	}

	return RC_VALID;
}

/*
 * Sample the touchscreen in polling mode
 */
static int wm97xx_poll_touch(wm97xx_data_t *data)
{
	int rc;

	if ((rc = wm97xx_poll_sample(WM97XX_ADCSEL_X, &data->x)) != RC_VALID)
		return rc;
	if ((rc = wm97xx_poll_sample(WM97XX_ADCSEL_Y, &data->y)) != RC_VALID)
		return rc;
	if (pil && !five_wire) {
		if ((rc = wm97xx_poll_sample(WM97XX_ADCSEL_PRES, &data->p)) != RC_VALID)
			return rc;
	} else
		data->p = DEFAULT_PRESSURE;

	return RC_VALID;
}

/*
 * Read AUX adc
 */
static int wm97xx_check_aux (void)
{
	int i, restore_state = 0, min_delta = 0x7fffffff;
	u16 adcsel;
	unsigned jiff = jiffies;

	spin_lock(&ts.lock);
	if (ts.pen_is_down) {
		/* If pen is down, delay auxiliary sampling. The touchscreen
		 * thread will wake us up when the pen will be released.
		 */
		ts.aux_waiting = 1;
		spin_unlock(&ts.lock);
		return 10*HZ;
	}

	ts.aux_waiting = 0;
	for (i = 0; i < ARRAY_SIZE (ts.aux_conv); i++)
		if ((adcsel = ts.aux_conv [i].adcsel)) {
			int delta = ts.aux_conv [i].next_jiffies - jiff;

			/* Loose the check here to syncronize events that
			 * have close periods. This allows less timer
			 * interrupts to be generated, and better performance
			 * because of less codec writes.
			 */
			if (delta < (int)(ts.aux_conv [i].period >> 5)) {
				int auxval = 0;

				if (!restore_state) {
					/* This is the first AUX sample we
					 * read, prepare the chip.
					 */
					restore_state = 1;
					ts.dig1 = CODEC->codec_read(CODEC, AC97_WM97XX_DIGITISER1);
					ts.dig2 = CODEC->codec_read(CODEC, AC97_WM97XX_DIGITISER2);
					CODEC->codec_write (CODEC, AC97_WM97XX_DIGITISER1, 0);
					CODEC->codec_write (CODEC, AC97_WM97XX_DIGITISER2, WM97XX_PRP_DET_DIG);
				}

				/* Fool wm97xx_poll_read_adc() */
				ts.pen_probably_down = 1;

				if (ts.aux_conv [i].func)
					ts.aux_conv [i].func (i, 0);
				wm97xx_poll_sample (adcsel, &auxval);
				if (ts.aux_conv [i].func)
					ts.aux_conv [i].func (i, 1);
				if ((auxval & WM97XX_ADCSRC_MASK) == adcsel) {
					ts.aux_conv [i].value = auxval & 0xfff;
					ts.aux_conv [i].next_jiffies = jiff + ts.aux_conv [i].period;
				}
			}

			delta = ts.aux_conv [i].next_jiffies - jiff;
			if (delta < min_delta)
				min_delta = delta;
		}

	if (restore_state) {
		/* We know for sure pen is not down */
		ts.pen_probably_down = 0;
		/* Restore previous state of digitizer registers */
		CODEC->codec_write (CODEC, AC97_WM97XX_DIGITISER1, ts.dig1);
		CODEC->codec_write (CODEC, AC97_WM97XX_DIGITISER2, ts.dig2);
	}
	spin_unlock(&ts.lock);

	if (min_delta < 1)
		min_delta = 1;

	return min_delta;
}

/* 
 * Auxiliary ADC reading thread.
 */
static int wm97xx_aux_thread (void *data)
{
	/* set up thread context */
	ts.aux_task = current;
	daemonize("kwm97xx_aux");
	set_user_nice (current, 5);
	
	if(ts.codec == NULL) {
		ts.aux_task = NULL;
		printk(KERN_ERR "codec is NULL, bailing\n");
	}
	complete (&ts.aux_init);

	/* touch reader loop */
	while (ts.aux_task) {
		int sleep_time = wm97xx_check_aux ();

		if (ts.aux_waiting) {
			wait_event_interruptible_timeout (ts.aux_wait,
							  ts.aux_waiting == 0,
							  sleep_time);
		} else {
			set_task_state (current, TASK_INTERRUPTIBLE);
			schedule_timeout (sleep_time);
		}
	}
	complete_and_exit (&ts.aux_exit, 0);
}

/*---/ Public interface exposed to custom architecture-dependent drivers /---*/

int wm97xx_request_auxconv (struct wm97xx_device *dev, wm97xx_aux_conv_t ac,
				 int period, wm97xx_ac_prepare func)
{	
	int ret = 0;
	
	spin_lock(&ts.lock);
	if (ts.aux_conv[ac].func) {
		spin_unlock(&ts.lock);
		return -EBUSY;
	}
	
	ts.aux_conv [ac].value = -1;
	ts.aux_conv [ac].period = period;
	ts.aux_conv [ac].next_jiffies = jiffies + period;
	ts.aux_conv [ac].func = func;
	ts.aux_conv [ac].adcsel = ts.is_wm9712 ? wm97xx_adcsel[1][ac] : wm97xx_adcsel[0][ac];
	ts.aux_task_ref++;
	
	/* Start the aux thread, if not started already */
	if (ts.aux_task) {
		spin_unlock(&ts.lock);
		return 0;
	}
	spin_unlock(&ts.lock);
	init_completion (&ts.aux_init);
	init_completion (&ts.aux_exit);
	ret = kernel_thread (wm97xx_aux_thread, &ts, CLONE_KERNEL);
	if (ret >= 0) {
		wait_for_completion (&ts.aux_init);
		if(ts.aux_task == NULL)
			ret = -EINVAL;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(wm97xx_request_auxconv);

void wm97xx_free_auxconv (struct wm97xx_device *dev, wm97xx_aux_conv_t ac)
{	
	spin_lock(&ts.lock);
	if (ts.aux_conv == NULL)
		goto out;
	
	ts.aux_conv [ac].value = -1;
	ts.aux_conv [ac].func = NULL;
	ts.aux_task_ref--;

	if (!ts.aux_task_ref && ts.aux_task) {
		spin_unlock(&ts.lock);
		/* kill thread */
		ts.aux_task = NULL;
		ts.aux_waiting = 0;
		wait_for_completion (&ts.aux_exit);
		return;
	}
out:
	spin_unlock(&ts.lock);
}
EXPORT_SYMBOL_GPL(wm97xx_free_auxconv);

int wm97xx_get_auxconv (struct wm97xx_device *dev, wm97xx_aux_conv_t ac)
{
	return ts.aux_conv [ac].adcsel ? ts.aux_conv [ac].value : -1;
}
EXPORT_SYMBOL_GPL(wm97xx_get_auxconv);

/*
 * Arch specific wm97xx components can register here. This means they are
 * probed/removed and can have a PM suspend/resume.
 */
int wm97xx_driver_register (struct wm97xx_device *drv)
{	
	if (!drv->probe || !drv->remove)
		return -ENODEV;
	
	INIT_LIST_HEAD(&drv->devices);
	spin_lock(&ts.lock);
	list_add(&drv->devices, &wm97xx_drivers);
	if(ts.probe) {
		spin_unlock(&ts.lock);
		return drv->probe();
	}
	spin_unlock(&ts.lock);
	return 0;
}
EXPORT_SYMBOL_GPL(wm97xx_driver_register);

/*
 * Arch specific unregister.
 */
void wm97xx_driver_unregister (struct wm97xx_device *drv)
{
	spin_lock(&ts.lock);
	list_del_init(&drv->devices);
	if (ts.probe){
		spin_unlock(&ts.lock);
		drv->remove();
	} else
		spin_unlock(&ts.lock);
}
EXPORT_SYMBOL_GPL(wm97xx_driver_unregister);

/*
 * Get the status of a codec GPIO pin
 */
wm97xx_gpio_status_t wm97xx_get_codec_gpio (struct wm97xx_device *dev, u32 gpio)
{
	u16 status;
	
	status = CODEC->codec_read(CODEC, AC97_GPIO_STATUS);
	
	if (status & gpio)
		return WM97XX_GPIO_HIGH;
	else
		return WM97XX_GPIO_LOW;
}
EXPORT_SYMBOL_GPL(wm97xx_get_codec_gpio);

/*
 * Set the status of a codec GPIO pin
 */
void wm97xx_set_codec_gpio (struct wm97xx_device *dev, u32 gpio, wm97xx_gpio_status_t status)
{
	u16 reg;

	spin_lock(&ts.lock);
	reg = CODEC->codec_read(CODEC, AC97_GPIO_STATUS);
	if (status & WM97XX_GPIO_HIGH)
		reg |= gpio;
	else
		reg &= ~gpio;
	
	if (ts.is_wm9712) {
		/* small hack for wm9712 */
		CODEC->id = 1;
		CODEC->codec_write(CODEC, AC97_GPIO_STATUS, reg << 1);
		CODEC->id = 0;
	} else	
		CODEC->codec_write(CODEC, AC97_GPIO_STATUS, reg);
		
	spin_unlock(&ts.lock);
}
EXPORT_SYMBOL_GPL(wm97xx_set_codec_gpio);

/*
 * Codec GPIO pin configuration, this set's pin direction, polarity,
 * stickyness and wake up.
 */
void wm97xx_config_codec_gpio (struct wm97xx_device *dev, u32 gpio, 
									wm97xx_gpio_dir_t dir, wm97xx_gpio_pol_t pol,
	 								wm97xx_gpio_sticky_t sticky, wm97xx_gpio_wake_t wake)
{
	u16 reg;
	spin_lock(&ts.lock);
	
	reg = CODEC->codec_read(CODEC, AC97_GPIO_POLARITY);
	if (pol ==  WM97XX_GPIO_POL_HIGH)
		reg |= gpio;
	else
		reg &= ~gpio;
	CODEC->codec_write(CODEC, AC97_GPIO_POLARITY, reg);
	
	reg = CODEC->codec_read(CODEC, AC97_GPIO_STICKY);	
	if (sticky == WM97XX_GPIO_STICKY)
		reg |= gpio;
	else
		reg &= ~gpio;
	CODEC->codec_write(CODEC, AC97_GPIO_STICKY, reg);
	
	reg = CODEC->codec_read(CODEC, AC97_GPIO_WAKE_UP);	
	if (wake == WM97XX_GPIO_WAKE)
		reg |= gpio;
	else
		reg &= ~gpio;
	CODEC->codec_write(CODEC, AC97_GPIO_WAKE_UP, reg);
	
	reg = CODEC->codec_read(CODEC, AC97_GPIO_CONFIG);	
	if (dir == WM97XX_GPIO_IN)
		reg |= gpio;
	else
		reg &= ~gpio;
	CODEC->codec_write(CODEC, AC97_GPIO_CONFIG, reg);
	spin_unlock(&ts.lock);
}
EXPORT_SYMBOL_GPL(wm97xx_config_codec_gpio);

/*
 * Simple and safe codec register IO for external drivers
 */
u16 wm97xx_reg_read (struct wm97xx_device *dev, u16 reg)
{
	return CODEC->codec_read(CODEC, reg);
}
EXPORT_SYMBOL_GPL(wm97xx_reg_read);

void wm97xx_reg_write (struct wm97xx_device *dev, u16 reg, u16 val)
{
	CODEC->codec_write(CODEC, reg, val);
} 
EXPORT_SYMBOL_GPL(wm97xx_reg_write);

/*
 * Allow arch specific driver to use the codec PENDOWN gpio signal 
 * as a pen down interrupt source. The PENDOWN signal is usually 
 * routed via a cpu irq/gpio.
 * 
 * NOTE: The wm9712 also allows the pen down signal to be sent over
 * the AC97 link or over the codec irq pin. Use wm97xx_enable_codec_irq
 * in this case.
 */
int wm97xx_set_pen_irq (struct wm97xx_device *dev, int irq)
{	
	spin_lock(&ts.lock);
	if (!ts.pen_irq)
		ts.pen_irq = irq;
	
	spin_unlock(&ts.lock);
	return 0;
}
EXPORT_SYMBOL_GPL(wm97xx_set_pen_irq);

/*
 * Allow arch specific drivers to use the codec AC97 interrupt. This 
 * can either be over the AC97 link or using the codec irq pin.
 * This interrupt can be used to notify arch specific drivers of
 * numerous events i.e. battery level, ADC data, pen down, etc
 */
int wm97xx_set_codec_irq (struct wm97xx_device *dev, int irq, int ac97_link)
{	
	spin_lock(&ts.lock);
	ts.codec_irq_ref_count++;
	if (!ts.codec_irq) {
		ts.codec_irq = irq;
		ts.ac97_link = ac97_link;
	}
	spin_unlock(&ts.lock);
	return 0;
}
EXPORT_SYMBOL_GPL(wm97xx_set_codec_irq);

void wm97xx_clear_pen_irq (struct wm97xx_device *dev)
{	
	spin_lock(&ts.lock);
	if (ts.pen_irq)
		ts.pen_irq = 0;	

	spin_unlock(&ts.lock);
}
EXPORT_SYMBOL_GPL(wm97xx_clear_pen_irq);

int wm97xx_clear_codec_irq (struct wm97xx_device *dev)
{
	spin_lock(&ts.lock);
	if (ts.codec_irq_ref_count == 0)
		goto out;
	
	/* last one ? */
	if (--ts.codec_irq_ref_count == 0) {
		ts.codec_irq = 0;	
	}
	
out:	
	spin_unlock(&ts.lock);
	return ts.codec_irq_ref_count;
}
EXPORT_SYMBOL_GPL(wm97xx_clear_codec_irq);

/* 
 * Register arch specific codec event handler. One handler per event.
 */
int wm97xx_request_codec_event (struct wm97xx_device *dev, int gpio, codec_event_t work)
{
	u16 reg;
	int irq = generic_ffs(gpio) - 1;
		
	if (irq < 0 || irq > WM97XX_MAX_GPIO)
		return -ENODEV;
	
	spin_lock(&ts.lock);
	if (!ts.is_wm9712)
		goto err;
	
	/* cant use GPIO2 for irq and gpio at the same time */
	if (!ts.ac97_link && gpio == WM97XX_GPIO_2)
		goto err;
	
	if (ts.codec_event[irq])
		goto err;
	
	ts.codec_event[irq] = work;
	
	/* handle pen down internally */
	if (gpio == WM97XX_GPIO_13)
		ts.pen_irq = ts.codec_irq;
		
	/* set pin to GPIO function */
	reg= CODEC->codec_read(CODEC, AC97_MISC_MODEM_STAT);
	CODEC->codec_write(CODEC, AC97_MISC_MODEM_STAT, reg | gpio);
	spin_unlock(&ts.lock);
	wm97xx_config_codec_gpio (dev, gpio, WM97XX_GPIO_IN, WM97XX_GPIO_POL_HIGH,
		WM97XX_GPIO_STICKY, WM97XX_GPIO_WAKE);
	
	return 0;
	
err:
	spin_unlock(&ts.lock);
	return -ENODEV;	
}
EXPORT_SYMBOL_GPL(wm97xx_request_codec_event);

/*
 * Free codec event handler
 */
void wm97xx_free_codec_event (struct wm97xx_device *dev, int gpio)
{
	int irq = generic_ffs(gpio) - 1;
	
	spin_lock(&ts.lock);
	if (!ts.is_wm9712)
		goto out;
	
	if (irq >= 0 && irq < WM97XX_MAX_GPIO)
		ts.codec_event[irq] = NULL;
	
	/* handle pen down internally */	
	if (gpio == WM97XX_GPIO_13)
		ts.pen_irq = 0;

out:
	spin_unlock(&ts.lock);
}
EXPORT_SYMBOL_GPL(wm97xx_free_codec_event);

/*
 * Enable continuous mode, i.e. touch data is streamed across an AC97 slot
 */
void wm97xx_set_continuous_mode (struct wm97xx_device *dev, int enable, int slot, int rate)
{
	u16 dig1, dig2;

	spin_lock(&ts.lock);

	dig1 = ts.dig1;
	dig2 = ts.dig2;

	if (enable) {
		/* continous mode */
		dbg("Enabling continous mode");
		ts.cont_read_sample = dev->cont_read;
		dig1 &= ~(WM97XX_CM_RATE_MASK | WM97XX_ADCSEL_MASK |
			  WM97XX_DELAY_MASK | WM97XX_SLT_MASK);
                dig1 |= WM97XX_CTC | WM97XX_COO | WM97XX_SLEN |
			WM97XX_DELAY (delay) |
			WM97XX_SLT (slot) |
			WM97XX_RATE (rate);
			if (pil)
				dig1 |= WM97XX_ADCSEL_PRES;
			if (ts.is_wm9712)
				dig2 |= WM9712_PDEN;
			else
				dig2 |= WM9705_PDEN;
	} else {
		ts.cont_read_sample = NULL;
		dig1 &= ~(WM97XX_CTC | WM97XX_COO | WM97XX_SLEN);
		if (mode == 1)
			/* coordinate mode */
			dig1 |= WM97XX_COO;
		
		if (ts.is_wm9712)
				dig2 &= ~WM9712_PDEN;
			else
				dig2 &= ~WM9705_PDEN;
	}

	CODEC->codec_write(CODEC, AC97_WM97XX_DIGITISER1, ts.dig1 = dig1);
	CODEC->codec_write(CODEC, AC97_WM97XX_DIGITISER2, ts.dig2 = dig2);

	spin_unlock(&ts.lock);
}
EXPORT_SYMBOL_GPL(wm97xx_set_continuous_mode);
/*---------------------------------------------------------------------------*/

#if defined(CONFIG_PM)
/* save state of wm9712 PGA's */
static void wm9712_pga_save(void)
{
	ts.phone_pga = CODEC->codec_read(CODEC, AC97_PHONE_VOL) & 0x001f;
	ts.line_pga = CODEC->codec_read(CODEC, AC97_LINEIN_VOL) & 0x1f1f;
	ts.mic_pga = CODEC->codec_read(CODEC, AC97_MIC_VOL) & 0x1f1f;
}

/* restore state of wm9712 PGA's */
static void wm9712_pga_restore(void)
{
	u16 reg;
	
	reg = CODEC->codec_read(CODEC, AC97_PHONE_VOL);
	CODEC->codec_write(CODEC, AC97_PHONE_VOL, (reg & ~0x1f) | ts.phone_pga);
	reg = CODEC->codec_read(CODEC, AC97_LINEIN_VOL);
	CODEC->codec_write(CODEC, AC97_LINEIN_VOL, (reg & ~0x1f1f) | ts.line_pga);
	reg = CODEC->codec_read(CODEC, AC97_MIC_VOL);
	CODEC->codec_write(CODEC, AC97_MIC_VOL, (reg & ~0x1f1f) | ts.mic_pga);
}

/*
 * Power down the codec
 */
static int wm97xx_suspend(struct ac97_codec *codec, int state)
{
	u16 reg;
	struct list_head *l;

	/* save digitiser settings */
	ts.dig1 = CODEC->codec_read(CODEC, AC97_WM97XX_DIGITISER1);
	ts.dig2 = CODEC->codec_read(CODEC, AC97_WM97XX_DIGITISER2);
	
	/* suspend any arch specific drivers */
	list_for_each(l, &wm97xx_drivers) {
		struct wm97xx_device *c = list_entry(l, struct wm97xx_device, devices);
		if (c->suspend)
			c->suspend(state, 0);
	}		
	
	/* wm9705 does not have extra PM */
	if (!ts.is_wm9712)
		return 0;

	/* save and mute the PGA's */
	wm9712_pga_save();

	reg = CODEC->codec_read(CODEC, AC97_PHONE_VOL);
	CODEC->codec_write(CODEC, AC97_PHONE_VOL, reg | 0x001f);
	reg = CODEC->codec_read(CODEC, AC97_MIC_VOL);
	CODEC->codec_write(CODEC, AC97_MIC_VOL, reg | 0x1f1f);
	reg = CODEC->codec_read(CODEC, AC97_LINEIN_VOL);
	CODEC->codec_write(CODEC, AC97_LINEIN_VOL, reg | 0x1f1f);

	/* power down, dont disable the AC link */
	CODEC->codec_write(CODEC, AC97_WM9712_POWER, WM9712_PD(14) |
			       WM9712_PD(13) | WM9712_PD(12) | WM9712_PD(11) |
			       WM9712_PD(10) | WM9712_PD(9) | WM9712_PD(8) |
			       WM9712_PD(7) | WM9712_PD(6) | WM9712_PD(5) |
			       WM9712_PD(4) | WM9712_PD(3) | WM9712_PD(2) |
			       WM9712_PD(1) | WM9712_PD(0));

	return 0;
}

/*
 * Power up the Codec
 */
static void wm97xx_resume(struct ac97_codec* codec)
{
	struct list_head *l;

	/* resume any arch specific drivers */
	list_for_each(l, &wm97xx_drivers) {
		struct wm97xx_device *c = list_entry(l, struct wm97xx_device, devices);
		if (c->resume)
			c->resume(0);
	}
	
	/* restore digitiser settings */
	CODEC->codec_write(CODEC, AC97_WM97XX_DIGITISER1, ts.dig1);
	CODEC->codec_write(CODEC, AC97_WM97XX_DIGITISER2, ts.dig2);
	
	/* wm9705 does not have extra PM other than muting AUX and VID */
	if (!ts.is_wm9712) {
		CODEC->codec_write(CODEC, AC97_AUX_VOL, 0x8000);
		CODEC->codec_write(CODEC, AC97_VIDEO_VOL, 0x8000);
		return;
	}

	/* power up */
	CODEC->codec_write(CODEC, AC97_WM9712_POWER, 0x0);

	/* restore PGA state */
	wm9712_pga_restore();
}

#else
#define wm97xx_suspend NULL
#define wm97xx_resume NULL
#endif

/*
 * Handle a pen down interrupt.
 */
static void wm97xx_pen_irq_worker (void *ptr)
{
	u32 status = 0;

	/* do we need to enable the touch panel reader */
	if (!ts.is_wm9712) {
		ts.pen_is_down++;
		wake_up_interruptible (&ts.pen_irq_wait);
	} else {
		status = CODEC->codec_read(CODEC, AC97_GPIO_STATUS);
		if (status & WM97XX_GPIO_13) {
			ts.pen_is_down++;
			wake_up_interruptible (&ts.pen_irq_wait);
		}
	}
}

/*
 * Handle a codec event interrupt. 
 * Can also include pen down depending upon configuration.
 */
static void wm97xx_codec_irq_worker (void *ptr)
{
	int i = WM97XX_MAX_GPIO;
	u16 status, sticky = 0;
	
	status = CODEC->codec_read(CODEC, AC97_GPIO_STATUS);
	
	/* check for pen down as we handle it internally
	 * MASK pen down the unmask at pen up */
	if (status & WM97XX_GPIO_13)
		wm97xx_pen_irq_worker(ptr);
	
	/* reset codec slot 12 sticky bits and disable active interrupt 
	 * sources by clearing the sticky status.
	 */
	spin_lock(&ts.lock);
	if (ts.is_wm9712) {
		sticky = CODEC->codec_read(CODEC, AC97_GPIO_STICKY);
		CODEC->id = 1;
		CODEC->codec_write(CODEC, AC97_GPIO_STATUS, status << 1);
		CODEC->id = 0;
		CODEC->codec_write(CODEC, AC97_GPIO_STICKY, status ^ sticky);
	}
	
	/* Iterate through archictecture specific codec workers (if they exist) in 
	 * priority order.
	 */
	while(--i >= 0) {
		if (ts.codec_event[i] && status & 1 << i)
			ts.codec_event[i](status);
	}

	/* turn sticky bits back on, except for pen down */
	if (ts.is_wm9712)
		CODEC->codec_write(CODEC, AC97_GPIO_STICKY, sticky & ~WM97XX_GPIO_13);
	
	spin_unlock(&ts.lock);
}

/*
 * Codec PENDOWN signal irq handler
 */
static irqreturn_t wm97xx_pen_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	ts.pen_irq_count++;
	queue_work(ts.pen_irq_workq, &ts.pen_event_work);
		
	return IRQ_HANDLED;
}

/*
 * Codec PENDOWN/Power/GPIO/status signal irq handler
 */
static irqreturn_t wm97xx_codec_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{	
	ts.codec_irq_count++;
	queue_work(ts.codec_event_workq, &ts.codec_event_work);
		
	return IRQ_HANDLED;
}

/*
 * initialise codec IRQ handler and workqueue
 */
static int wm97xx_init_codec_irq (void)
{
	u16 reg;

	INIT_WORK(&ts.codec_event_work, wm97xx_codec_irq_worker, &ts);
	if ((ts.codec_event_workq = create_singlethread_workqueue("kwm97ev")) == NULL) {
		err("could not create codec event work queue");
		ts.codec_irq = 0;
		return -1;
	}
	
	ts.codec_irq_count = 0;
	if (request_irq(ts.codec_irq, wm97xx_codec_interrupt, SA_SHIRQ, "wm97xx-ac97", &ts)) {
		err("could not register codec interrupt");
		destroy_workqueue(ts.codec_event_workq);
		ts.codec_irq = 0;
		return -1;
	}
		
	/* wm9712 can interrupt using it's IRQ pin or over the AC97 link */
	if (ts.is_wm9712) {
		if (ts.ac97_link) { /* use link */
			reg = CODEC->codec_read(CODEC, AC97_RESERVED_58);
			CODEC->codec_write(CODEC, AC97_RESERVED_58, reg |= 0x2);
		} else { /* use pin */
			reg = CODEC->codec_read(CODEC, AC97_MISC_MODEM_STAT);
			CODEC->codec_write(CODEC, AC97_MISC_MODEM_STAT, reg &= 0xfffb);
			reg = CODEC->codec_read(CODEC, AC97_GPIO_CONFIG);
			CODEC->codec_write(CODEC, AC97_GPIO_CONFIG, reg &= 0xfffb);
		}
	}
	
	return 0;
}

/*
 * initialise pen IRQ handler and workqueue
 */
static int wm97xx_init_pen_irq (void)
{
	u16 reg;

	INIT_WORK(&ts.pen_event_work, wm97xx_pen_irq_worker, &ts);
	if ((ts.pen_irq_workq = create_singlethread_workqueue("kwm97pen")) == NULL) {
		err("could not create pen irq work queue");
		ts.pen_irq = 0;
		return -1;
	}

	ts.pen_irq_count = 0;
	if (request_irq(ts.pen_irq, wm97xx_pen_interrupt, SA_SHIRQ, "wm97xx-pen", &ts)) {
		err("could not register codec pen down interrupt, will poll for pen down");
		destroy_workqueue(ts.pen_irq_workq);
		ts.pen_irq = 0;
		return -1;
	}
	
	/* enable PEN down on wm9712 */
	if (ts.is_wm9712) {
			reg = CODEC->codec_read(CODEC, AC97_MISC_MODEM_STAT);
	 		CODEC->codec_write(CODEC, AC97_MISC_MODEM_STAT, reg & 0xfff7);
	}

	return 0;
}

/*
 * set up the physical settings of the device
 */
static void init_wm97xx_phy(void)
{
	ts.dig1 = 0;

	if (ts.is_wm9712)
		ts.dig2 = WM97XX_RPR | WM9712_RPU(1);
	else {
		ts.dig2 = WM97XX_RPR;

		/*
		 * mute VIDEO and AUX as they share X and Y touchscreen
		 * inputs on the WM9705 
		 */
		CODEC->codec_write(CODEC, AC97_AUX_VOL, 0x8000);
		CODEC->codec_write(CODEC, AC97_VIDEO_VOL, 0x8000);
	}
		
	/* WM9712 rpu */
	if (ts.is_wm9712 && rpu) {
		ts.dig2 &= 0xffc0;
		ts.dig2 |= WM9712_RPU(rpu);
		dbg("setting pen detect pull-up to %d Ohms",64000 / rpu);
	}
      
	/* touchpanel pressure current*/
	if  (pil == 2) {
		if (ts.is_wm9712)
			ts.dig2 |= WM9712_PIL;
		else
			ts.dig2 |= WM9705_PIL;
		dbg("setting pressure measurement current to 400uA.");
	} else if (pil) 
		dbg("setting pressure measurement current to 200uA.");
	if(!pil)
		pressure = 0;
      
	/* WM9712 five wire */
	if (ts.is_wm9712 && five_wire) {
		ts.dig2 |= WM9712_45W;
		dbg("setting 5-wire touchscreen mode.");
	}
      
	/* polling mode sample settling delay */
	if (delay!=4) {
		if (delay < 0 || delay > 15) {
			dbg("supplied delay out of range.");
			delay = 4;
		}
	}
	ts.dig1 &= 0xff0f;
	ts.dig1 |= WM97XX_DELAY(delay);
	dbg("setting adc sample delay to %d u Secs.", delay_table[delay]);
      
	/* coordinate mode */
	if (mode == 1) {
		ts.dig1 |= WM97XX_COO;
		dbg("using coordinate mode");
	}
      
	/* WM9705 pdd */
	if (pdd && !ts.is_wm9712) {
		ts.dig2 |= (pdd & 0x000f);
		dbg("setting pdd to Vmid/%d", 1 - (pdd & 0x000f));
	}
      
	CODEC->codec_write(CODEC, AC97_WM97XX_DIGITISER1, ts.dig1);
	CODEC->codec_write(CODEC, AC97_WM97XX_DIGITISER2, ts.dig2); 	
}

/* Private struct for communication between wm97xx_ts_thread and wm97xx_read_samples */
struct ts_state {
	int sleep_time;
	int min_sleep_time;
};

/*
 * When we use an irq to detect a pen down event, we must manually check
 * for a pen up by checking the PDEN bit. If the pen goes up, we have to
 * re-enable the codec pen down detection.
 */
static int wm97xx_irq_pen_check(int rc)
{
	u16 pen_status, reg;

	pen_status = CODEC->codec_read(CODEC, AC97_WM97XX_DIGITISER_RD);
	if (!(pen_status & WM97XX_PEN_DOWN)) {
		rc = RC_PENUP;

		/* Pen is now going to the up position */
		if (ts.pen_is_down && (ts.pen_irq == ts.codec_irq)) {
			reg = CODEC->codec_read(CODEC, AC97_GPIO_STICKY);
			CODEC->codec_write(CODEC, AC97_GPIO_STICKY, reg | WM97XX_GPIO_13);
		}
	}
	return rc;
}

static int wm97xx_read_samples(struct ts_state *state)
{
	wm97xx_data_t data;
	int rc = RC_PENUP;
		
	spin_lock(&ts.lock);

	if (ts.cont_read_sample) {
		rc = ts.cont_read_sample (&wm97xx_input, ts.pen_is_down, pressure);
		rc = wm97xx_irq_pen_check (rc);
	} else {
		switch (mode) {
		case 0:
			rc = wm97xx_poll_touch (&data);
			break;
		case 1:
			rc = wm97xx_poll_coord_touch (&data);
			break;
		}
	}
	
	if (rc & RC_PENUP) {
		if (ts.pen_is_down) {
			ts.pen_is_down = 0;
			dbg("pen up");
			input_report_abs(&wm97xx_input, ABS_PRESSURE, 0);
			input_sync(&wm97xx_input);
			if (ts.aux_waiting) {
				ts.aux_waiting = 0;
				wake_up_interruptible (&ts.aux_wait);
			}
		} else if (!(rc & RC_AGAIN)) {
			/* We need high frequency updates only while pen is down,
			 * the user never will be able to touch screen faster than
			 * a few times per second... On the other hand, when the
			 * user is actively working with the touchscreen we don't
			 * want to lose the quick response. So we will slowly
			 * increase sleep time after the pen is up and quicky
			 * restore it to ~one task switch when pen is down again.
			 */
			if (state->sleep_time < HZ/10)
				state->sleep_time++;
		}
	} else if (rc & RC_VALID) {
		dbg("pen down: x=%x:%d, y=%x:%d, pressure=%x:%d\n",
		    data.x >> 12, data.x & 0xfff,
		    data.y >> 12, data.y & 0xfff,
		    data.p >> 12, data.p & 0xfff);
		if (ts.invert_x)
			data.x ^= 0xfff;
		if (ts.invert_y)
			data.y ^= 0xfff;
		input_report_abs (&wm97xx_input, ABS_X, data.x & 0xfff);
		input_report_abs (&wm97xx_input, ABS_Y, data.y & 0xfff);
		input_report_abs (&wm97xx_input, ABS_PRESSURE, data.p & 0xfff);
		input_sync (&wm97xx_input);
		ts.pen_is_down++;
		state->sleep_time = state->min_sleep_time;
	} else if (rc & RC_PENDOWN) {
		dbg("pen down");
		ts.pen_is_down++;
		state->sleep_time = state->min_sleep_time;
	}

	spin_unlock(&ts.lock);
	return rc;
}

/*
 * The touchscreen sample reader thread.
 * Also once in 30 seconds we'll update the battery status.
 */
static int wm97xx_ts_thread(void *data)
{
	int rc;
	struct ts_state state;

	/* set up thread context */
	ts.ts_task = current;
	daemonize("wm97xx_ts");

	if(ts.codec == NULL) {
		ts.ts_task = NULL;
		printk(KERN_ERR "codec is NULL, bailing\n");
	}
	complete(&ts.ts_init);

	ts.pen_is_down = 0;
	state.min_sleep_time = HZ >= 100 ? HZ/100 : 1;
	if (state.min_sleep_time < 1)
		state.min_sleep_time = 1;
	state.sleep_time = state.min_sleep_time;

	/* touch reader loop */
	while (ts.ts_task) {
		do {
			rc = wm97xx_read_samples (&state);
		} while (rc & RC_AGAIN);
		if (!ts.pen_is_down && ts.pen_irq) {	
			/* Nice, we don't have to poll for pen down event */
			wait_event_interruptible (ts.pen_irq_wait, ts.pen_is_down);
		} else {
			set_task_state(current, TASK_INTERRUPTIBLE);
			schedule_timeout(state.sleep_time);
		}
	}
	complete_and_exit(&ts.ts_exit, 0);
}

/*
 * Start the touchscreen thread and 
 * the touch digitiser.
 */
static int wm97xx_ts_input_open(struct input_dev *idev)
{
	int ret = 0;
	u16 dig2;

	if (ts.codec == NULL) {
		printk("error: ts.codec == NULL\n");
		return -EINVAL;
	}
	
	if (ts.codec->codec_read == NULL) {
		printk("error: codec_read == NULL\n");
		return -EINVAL;
	}

	if (ts.codec->codec_write == NULL) {
		printk("error: codec_write == NULL\n");
		return -EINVAL;
	}
	
	down(&ts.sem);
	/* first time opened ? */
	if (ts.use_count++ == 0) {
		/* start touchscreen thread */
		init_completion(&ts.ts_init);
		init_completion(&ts.ts_exit);

		ret = kernel_thread(wm97xx_ts_thread, &ts, CLONE_KERNEL);
		if (ret >= 0) {
			wait_for_completion(&ts.ts_init);
			if (ts.ts_task == NULL)
				ret = -EINVAL;
		}
		if (ret < 0)
			goto out;

		/* start digitiser */
		spin_lock(&ts.lock);

		dig2 = ts.dig2 | WM97XX_PRP_DET_DIG;
		CODEC->codec_write(CODEC, AC97_WM97XX_DIGITISER2, ts.dig2 = dig2);
		CODEC->codec_read(CODEC, AC97_WM97XX_DIGITISER_RD); /* dummy read */
		
		/* init pen down/up irq handling */
		if (ts.codec_irq || ts.pen_irq) {
			if (ts.codec_irq == ts.pen_irq) {
				u16 reg = CODEC->codec_read(CODEC, AC97_GPIO_STICKY);
				CODEC->codec_write(CODEC, AC97_GPIO_STICKY, reg | WM97XX_GPIO_13);
				wm97xx_init_codec_irq();
			} else {
				wm97xx_init_pen_irq();
			}
		}
		spin_unlock(&ts.lock);
	}

out:
	up(&ts.sem);
	return ret;
}

/*
 * Kill the touchscreen thread and stop
 * the touch digitiser.
 */
static void wm97xx_ts_input_close(struct input_dev *idev)
{
	int val;

	down(&ts.sem);
	if (--ts.use_count == 0) {
		/* destroy workqueue and free irq's */
		if (ts.codec_irq || ts.pen_irq) {
			if (ts.pen_irq == ts.codec_irq) {
				free_irq(ts.codec_irq, &ts);
				destroy_workqueue(ts.codec_event_workq);
			} else {
				free_irq(ts.pen_irq, &ts);
				destroy_workqueue(ts.pen_irq_workq);
			}
		}

		/* kill thread */
		ts.ts_task = NULL;
		ts.pen_is_down = 1;
		wake_up_interruptible (&ts.pen_irq_wait);
		wait_for_completion(&ts.ts_exit);

		/* stop digitiser */
		val = ts.codec->codec_read(ts.codec, AC97_WM97XX_DIGITISER2);
		ts.codec->codec_write(ts.codec, AC97_WM97XX_DIGITISER2,
				       val & ~WM97XX_PRP_DET_DIG);
		
	}
	up(&ts.sem);
}

/*
 * Called by the audio codec initialisation to register
 * the touchscreen driver.
 */
static int wm97xx_probe(struct ac97_codec *codec, struct ac97_driver *driver)
{
	u16 id1, id2;
	struct list_head *l;

	spin_lock(&ts.lock);
	/* stop multiple probes */
	if (ts.probe) {
		spin_unlock(&ts.lock);
		return 0;
	}
	
	/* set up AC97 codec interface */
	CODEC = codec;
	codec->driver_private = &ts;
	
	/*
	 * We can only use a WM9705 or WM9712 that has been *first* initialised
	 * by the AC97 audio driver. This is because we have to use the audio
	 * drivers codec read() and write() functions to sample the touchscreen
	 *
	 * If an initialsed WM97xx is found then get the codec read and write
	 * functions.
	 */

	/* test for a WM9712 or a WM9705 */
	id1 = codec->codec_read(codec, AC97_VENDOR_ID1);
	id2 = codec->codec_read(codec, AC97_VENDOR_ID2);
	if (id1 == WM97XX_ID1 && id2 == WM9712_ID2)
		ts.is_wm9712 = 1;
	else if (id1 == WM97XX_ID1 && id2 == WM9705_ID2)
		ts.is_wm9712 = 0;
	else {
		err("could not find a WM9705/12 codec. Found a 0x%4x:0x%4x instead",
		    id1, id2);
		spin_unlock(&ts.lock);
		return -ENODEV;
	}
	info("Detected a WM97%s codec connected to AC97 controller %d", ts.is_wm9712 ? "12" : "05", codec->id);
	wm97xx_id = (id1 << 16) | id2;
	
	/* tell input system what we events we accept and register */
	memset (&wm97xx_input, 0, sizeof (struct input_dev));
	wm97xx_input.name = ts.is_wm9712 ? "wm9712 touchscreen" : "wm9705 touchscreen";
	wm97xx_input.open = wm97xx_ts_input_open;
	wm97xx_input.close = wm97xx_ts_input_close;
	set_bit(EV_ABS, wm97xx_input.evbit);
	set_bit(ABS_X, wm97xx_input.absbit);
	set_bit(ABS_Y, wm97xx_input.absbit);
	set_bit(ABS_PRESSURE, wm97xx_input.absbit);
	wm97xx_input.absmax[ABS_X] = abs_x[1];
	wm97xx_input.absmax[ABS_Y] = abs_y[1];
	wm97xx_input.absmax[ABS_PRESSURE] = abs_p[1];
	wm97xx_input.absmin[ABS_X] = abs_x[0];
	wm97xx_input.absmin[ABS_Y] = abs_y[0];
	wm97xx_input.absmin[ABS_PRESSURE] = abs_p[0];
	wm97xx_input.absfuzz[ABS_X] = abs_x[2];
	wm97xx_input.absfuzz[ABS_Y] = abs_y[2];
	wm97xx_input.absfuzz[ABS_PRESSURE] = abs_p[2];
	wm97xx_input.private = &ts;
	input_register_device(&wm97xx_input);

	/* set up physical characteristics */
	init_wm97xx_phy();

	/* probe any arch specific drivers */
	list_for_each(l, &wm97xx_drivers){
		struct wm97xx_device *c = list_entry(l, struct wm97xx_device, devices);
			c->probe();
	}	
	
	ts.probe = 1;
	spin_unlock(&ts.lock);

	return 0;
}

/*
 * Called by ac97_codec when it is unloaded.
 */
static void wm97xx_remove(struct ac97_codec *codec, struct ac97_driver *driver)
{
	u16 dig1, dig2;
	struct list_head *l;
		
	dbg("Unloading AC97 codec %d", codec->id);

	input_unregister_device(&wm97xx_input);
	
	/* remove any arch specific drivers */
	list_for_each(l, &wm97xx_drivers) {
		struct wm97xx_device *c = list_entry(l, struct wm97xx_device, devices);
			c->remove();
	}	

	/* Stop touch read thread */
	if (ts.ts_task) {
		ts.ts_task = NULL;
		ts.pen_is_down = 1;
		wake_up_interruptible (&ts.pen_irq_wait);
		wait_for_completion(&ts.ts_exit);
	}
	
	/* Stop AUX sampling thread */
	if (ts.aux_task) {
		ts.aux_task = NULL;
		ts.aux_waiting = 0;
		wake_up_interruptible (&ts.aux_wait);
		wait_for_completion (&ts.aux_exit);
	}
	
	spin_lock(&ts.lock);
	codec->driver_private = NULL;

	/* restore default digitiser values */
	dig1 = WM97XX_DELAY(4) | WM97XX_SLT(6);
	if (ts.is_wm9712)
		dig2 = WM9712_RPU(1);
	else
		dig2 = 0x0;

	codec->codec_write(codec, AC97_WM97XX_DIGITISER1, dig1);
	codec->codec_write(codec, AC97_WM97XX_DIGITISER2, dig2);
	ts.probe = 0;
	
	spin_unlock(&ts.lock);	
}

/* AC97 registration info */
static struct ac97_driver wm9705_driver = {
	.codec_id 	= (WM97XX_ID1 << 16) | WM9705_ID2,
	.codec_mask = 0xFFFFFFFF,
	.name		= "Wolfson WM9705 Touchscreen/Battery Monitor",
	.probe		= wm97xx_probe,
	.remove		= __devexit_p(wm97xx_remove),
	.suspend	= wm97xx_suspend,
	.resume		= wm97xx_resume,
};

static struct ac97_driver wm9712_driver = {
	.codec_id	= (WM97XX_ID1 << 16) | WM9712_ID2,
	.codec_mask	= 0xFFFFFFFF,
	.name		= "Wolfson WM9712 Touchscreen/Battery Monitor",
	.probe		= wm97xx_probe,
	.remove		= __devexit_p(wm97xx_remove),
	.suspend	= wm97xx_suspend,
	.resume		= wm97xx_resume,
};

static int __init wm97xx_plugin_init(void)
{
	info("Wolfson WM9705/WM9712 Touchscreen Controller driver");
	info("Version %s  liam.girdwood@wolfsonmicro.com", WM_TS_VERSION);
	
	memset(&ts, 0, sizeof(ts));
	init_MUTEX(&ts.sem);
	spin_lock_init (&ts.lock);
	init_waitqueue_head (&ts.pen_irq_wait);
	init_waitqueue_head (&ts.aux_wait);
	
	/* register with the AC97 layer */
	ac97_register_driver(&wm9705_driver);
	ac97_register_driver(&wm9712_driver);

	return 0;
}

static void __exit wm97xx_plugin_exit(void)
{
	/* Unregister from the AC97 layer */
	ac97_unregister_driver(&wm9712_driver);
	ac97_unregister_driver(&wm9705_driver);
}

/* Module information */
MODULE_AUTHOR("Liam Girdwood, liam.girdwood@wolfsonmicro.com, www.wolfsonmicro.com");
MODULE_DESCRIPTION("WM9705/WM9712 Touch Screen / BMON Driver");
MODULE_LICENSE("GPL");

module_init(wm97xx_plugin_init);
module_exit(wm97xx_plugin_exit);
