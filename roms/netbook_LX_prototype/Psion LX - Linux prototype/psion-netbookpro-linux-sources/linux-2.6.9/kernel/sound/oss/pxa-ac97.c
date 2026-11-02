/*
 *  linux/drivers/sound/pxa-ac97.c -- AC97 interface for the Cotula chip
 *
 *  Author:	Nicolas Pitre
 *  Created:	Aug 15, 2001
 *  Copyright:	MontaVista Software Inc.
 *
 *  Forward ported to 2.6 by Ian Molton 15/09/2003
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 * 
 *  Sep 1st 2004: Liam Girdwood <liam.girdwood@wolfsonmicro.com>
 *                Changed codec initialisation sequence to perform a cold reset 
 *                followed by a warm reset. This allows a greater range of 
 *                codecs to be used on the XScale.
 *                Added codec init failure error detection, fast codec GPIO
 *                for PXA25x and PXA27x, new PXA27x AC97 read/write and
 *                shared the AC97 interrupt for use with codec specifig GPIO's.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/sound.h>
#include <linux/soundcard.h>
#include <linux/ac97_codec.h>

#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/semaphore.h>
#include <asm/dma.h>
#include <asm/arch/mainstone.h>

#include "pxa-audio.h"

#if defined(CONFIG_PREEMPT) && defined(CONFIG_MACH_MAINSTONE)
#error this driver is buggy with preempt enabled
#endif

/* We need a dummy irq tag to request/free our shared irq handler */
#define AC97_IRQ_TAG	(void*)0xac97c0de

static DECLARE_MUTEX(CAR_mutex);

/*
 * PXA27x has a different AC97 IO flow than PXA25x.
 */
#ifdef CONFIG_PXA27x

static int pxa27x_read_wait(void)
{
        unsigned long timeout = jiffies + 2;
        int ret = 1;
        u32 gsr;

        do {
                if (GSR & GSR_SDONE)
                        break;

                msleep(1);
        } while (time_before(jiffies, timeout));

        gsr = GSR;
        GSR = gsr | GSR_SDONE | GSR_CDONE;

        if (!(gsr & GSR_SDONE)) {
		printk(KERN_CRIT "%s: read codec register timeout.\n", __FUNCTION__);
                ret = 0;
        }

        return ret;
}

static int pxa27x_write_wait(void)
{
        unsigned long timeout = jiffies + 2;
        int ret = 1;
        u32 gsr;

        do {
                if (GSR & GSR_CDONE)
                        break;

                msleep(1);
        } while (time_before(jiffies, timeout));

        gsr = GSR;
        GSR = gsr | GSR_CDONE;

        if (!(gsr & GSR_CDONE)) {
		printk(KERN_CRIT "%s: write codec register timeout.\n", __FUNCTION__);
                ret = 0;
        }

        return ret;
}

static u16 pxa_ac97_read(struct ac97_codec *codec, u8 reg)
{
	u16 val =-1;
	volatile u32 *reg_addr;
	
	down(&CAR_mutex);

	/* set up primary or secondary space */
	if (codec->id & 0x1)
		reg_addr = (u32 *)&SAC_REG_BASE + (reg >> 1);
	else
		reg_addr = (u32 *)&PAC_REG_BASE + (reg >> 1);
		
	/* No fast codec GPIO reads on PXA27x */
	(void)*reg_addr;			// dummy read
	if (pxa27x_read_wait()) {
		val = *reg_addr;
		if (!pxa27x_read_wait())
			val = -1;
	}
		
	up(&CAR_mutex);
	return val;
}

static void pxa_ac97_write(struct ac97_codec *codec, u8 reg, u16 val)
{
	volatile u32 *reg_addr;
	down(&CAR_mutex);
		
	/* Status reg (0x54) writes are fast, others need to wait */ 
	if (reg == AC97_GPIO_STATUS) {
		
		/* set up primary or secondary modem space */
		if (codec->id & 0x1)
			reg_addr = (u32 *)&SMC_REG_BASE + (reg >> 1);
		else
			reg_addr = (u32 *)&PMC_REG_BASE + (reg >> 1);
		
		*reg_addr = val;
		udelay(50);
	} else {
		/* set up primary or secondary codec space */
		if (codec->id & 0x1)
			reg_addr = (u32 *)&SAC_REG_BASE + (reg >> 1);
		else
			reg_addr = (u32 *)&PAC_REG_BASE + (reg >> 1);
			
		*reg_addr = val;
		pxa27x_write_wait();
	}
	up(&CAR_mutex);
}

#else
static struct completion CAR_completion;
static int waitingForMask;

static u16 pxa_ac97_read(struct ac97_codec *codec, u8 reg)
{
	u16 val = -1;

	down(&CAR_mutex);
	if (!(CAR & CAR_CAIP)) {
		volatile u32 *reg_addr;

		/* Status reg (0x54) reads are fast, others need to wait */ 
		if (reg == AC97_GPIO_STATUS) {
			/* set up primary or secondary modem space */
			if (codec->id & 0x1)
				reg_addr = (u32 *)&SMC_REG_BASE + (reg >> 1);
			else
				reg_addr = (u32 *)&PMC_REG_BASE + (reg >> 1);
				
			val = *reg_addr;
			up(&CAR_mutex);
			return val;
		}

		/* set up primary or secondary codec space */
		if (codec->id & 0x1)
			reg_addr = (u32 *)&SAC_REG_BASE + (reg >> 1);
		else
			reg_addr = (u32 *)&PAC_REG_BASE + (reg >> 1);
			
		waitingForMask=GSR_SDONE;
		init_completion(&CAR_completion);
		(void)*reg_addr;			//start read access across the ac97 link
		wait_for_completion(&CAR_completion);

		if (GSR & GSR_RDCS) {
			GSR |= GSR_RDCS;		//write a 1 to clear
			printk(KERN_CRIT "%s: read codec register timeout.\n", __FUNCTION__);
		}

		init_completion(&CAR_completion);
		val = *reg_addr;			//valid data now but we've just started another cycle...
		wait_for_completion(&CAR_completion);

	} else {
		printk(KERN_CRIT"%s: CAR_CAIP already set\n", __FUNCTION__);
	}
	up(&CAR_mutex);

	return val;
}

static void pxa_ac97_write(struct ac97_codec *codec, u8 reg, u16 val)
{
	down(&CAR_mutex);
	if (!(CAR & CAR_CAIP)) {
		volatile u32 *reg_addr;
		
		/* Status reg (0x54) writes are fast, others need to wait  */ 
		if (reg == AC97_GPIO_STATUS) {
			/* set up primary or secondary modem space */
			if (codec->id & 0x1)
				reg_addr = (u32 *)&SMC_REG_BASE + (reg >> 1);
			else
				reg_addr = (u32 *)&PMC_REG_BASE + (reg >> 1);
				
			*reg_addr = val;
			up(&CAR_mutex);
			return;
		}
		
		reg_addr = (u32 *)&PAC_REG_BASE + (reg >> 1);

		/* set up primary or secondary codec space */
		if (codec->id & 0x1)
			reg_addr = (u32 *)&SAC_REG_BASE + (reg >> 1);
		else
			reg_addr = (u32 *)&PAC_REG_BASE + (reg >> 1);
			
		waitingForMask=GSR_CDONE;
		init_completion(&CAR_completion);
		*reg_addr = val;
		wait_for_completion(&CAR_completion);
	} else {
		printk(KERN_CRIT "%s: CAR_CAIP already set\n", __FUNCTION__);
	}
	up(&CAR_mutex);

	if (0)
		printk("%s(0x%02x, 0x%04x)\n", __FUNCTION__, reg, val);
}

static irqreturn_t pxa_ac97_irq(int irq, void *dev_id, struct pt_regs *regs)
{
	int gsr = GSR;
	
	/* we only handle CDONE and SDONE */
	if (gsr & (GSR_SDONE|GSR_CDONE)) {
		GSR = gsr & (GSR_SDONE|GSR_CDONE);		//write a 1 to clear
		if (gsr & waitingForMask)
			complete(&CAR_completion);
		return IRQ_HANDLED;
	} else
		return IRQ_NONE;
}
#endif

static struct ac97_codec pxa_ac97_codec = {
	codec_read:	pxa_ac97_read,
	codec_write:	pxa_ac97_write,
};

static int pxa_ac97_cold_reset(void)
{
	int timeout = 5;

	if (machine_is_netbookpro()) {
		/* remove codec reset */
		GPSR(63) = GPIO_bit(63);	
	}
	GCR = 0;
	udelay(10);
#ifdef CONFIG_PXA27x
	GCR = GCR_COLD_RST;
#else
	GCR = GCR_COLD_RST|GCR_CDONE_IE|GCR_SDONE_IE;
#endif
	
	/* link should be up after 10 uS */
	udelay(10);
	while (!(GSR & GSR_PCR) && timeout--) {
		msleep(1);
	}

	return timeout;
}

static int pxa_ac97_warm_reset(void)
{
	int timeout = 5;

#ifdef CONFIG_PXA27x
	/* warm reset broken on Bulverde, so manually keep AC97 reset high */
	GAFR3_U &= ~0xC;
	GPSR3 = 0x20000 | GPLR3;
#endif		
	
	GCR &= ~GCR_ACLINK_OFF;
#ifdef CONFIG_PXA27x
	GCR |= GCR_WARM_RST;
#else
	GCR |= GCR_WARM_RST|GCR_CDONE_IE|GCR_SDONE_IE;
#endif
	udelay(500);
	
	/* link should be up after 500 uS */
	while (!(GSR & GSR_PCR) && timeout--) {
		msleep(1);
	}
	
	return timeout;
}

static DECLARE_MUTEX(pxa_ac97_mutex);
static int pxa_ac97_refcount;

int pxa_ac97_get(struct ac97_codec **codec)
{
	int ret;

	*codec = NULL;
	down(&pxa_ac97_mutex);

	if (!pxa_ac97_refcount) {	
#ifndef CONFIG_PXA27x
		ret = request_irq(IRQ_AC97, pxa_ac97_irq, SA_SHIRQ, "AC97", AC97_IRQ_TAG);
		if (ret)
			return ret;
#endif			
		pxa_set_cken(CKEN2_AC97, 1);

		/*
		 * On Mainstone, we enable the on board audio amp and
		 * route AC97_SYSCLK via GPIO45 to the audio daughter card
		 */
		if(machine_is_mainstone()) {
			MST_MSCWR2 &= 0xffff000b;
			pxa_gpio_mode(GPIO45_SYSCLK_AC97_MD);
		}

		pxa_gpio_mode(GPIO31_SYNC_AC97_MD);
		pxa_gpio_mode(GPIO30_SDATA_OUT_AC97_MD);
		pxa_gpio_mode(GPIO28_BITCLK_AC97_MD);
		pxa_gpio_mode(GPIO29_SDATA_IN_AC97_MD);

		/*
		 * Use GPIO 113 as AC97 Reset on Bulverde
		 */
#ifdef CONFIG_PXA27x
		pxa_gpio_mode(113 | GPIO_ALT_FN_2_OUT);
#endif
		/* 
		 * We try a cold reset followed by a warm reset. If the codec doesn't
		 * respond after this then something is wrong.
		 */
		if (pxa_ac97_cold_reset() < 0)
		 	if (pxa_ac97_warm_reset() < 0)
		 		goto ac97_fail;
		 		
		ret = ac97_probe_codec(&pxa_ac97_codec);
		if (ret != 1) 
			goto ac97_fail;
	}

	pxa_ac97_refcount++;
	up(&pxa_ac97_mutex);
	*codec = &pxa_ac97_codec;
	return 0;
	
ac97_fail:
	if(machine_is_mainstone()) {
		MST_MSCWR2 &= ~0xc0;
		pxa_gpio_mode(GPIO45_BTRTS_MD);
	}
	free_irq(IRQ_AC97, AC97_IRQ_TAG);
	GCR = GCR_ACLINK_OFF;
	pxa_set_cken(CKEN2_AC97, 0);
	printk(KERN_ERR "could not initialise AC97 codec.");
	return -ENODEV;
}

void pxa_ac97_put(void)
{
	down(&pxa_ac97_mutex);
	pxa_ac97_refcount--;
	if (!pxa_ac97_refcount) {
		if(machine_is_mainstone()) {
			MST_MSCWR2 &= ~0xc0;
			pxa_gpio_mode(GPIO45_BTRTS_MD);
		}
		GCR = GCR_ACLINK_OFF;
		pxa_set_cken(CKEN2_AC97, 0);
#ifndef CONFIG_PXA27x 
		free_irq(IRQ_AC97, AC97_IRQ_TAG);
#endif
	}
	up(&pxa_ac97_mutex);
}

EXPORT_SYMBOL(pxa_ac97_get);
EXPORT_SYMBOL(pxa_ac97_put);


/*
 * Audio Mixer stuff
 */

static audio_state_t ac97_audio_state;
static audio_stream_t ac97_audio_in;

static int mixer_ioctl( struct inode *inode, struct file *file,
			unsigned int cmd, unsigned long arg)
{
	int ret;
	
	/* we need to disable/intercept some ioctls that interfere with the nbp setup */
	if (machine_is_netbookpro()) {
		if (cmd == MIXER_WRITE(SOUND_MIXER_LINE1)|| 
			cmd == MIXER_WRITE(SOUND_MIXER_VIDEO)||
			cmd == MIXER_WRITE(SOUND_MIXER_CD) ||
			cmd == MIXER_WRITE(SOUND_MIXER_ALTPCM) ||
			cmd == MIXER_WRITE(SOUND_MIXER_PHONEOUT)) {
			return 0;
		}
		if (cmd == MIXER_READ(SOUND_MIXER_ALTPCM) ||
			cmd == MIXER_READ(SOUND_MIXER_PHONEOUT))
			cmd = MIXER_READ(SOUND_MIXER_VOLUME);
	}
	
	ret = pxa_ac97_codec.mixer_ioctl(&pxa_ac97_codec, cmd, arg);
	if (ret)
		return ret;

	/* We must snoop for some commands to provide our own extra processing */
	switch (cmd) {
	case SOUND_MIXER_WRITE_RECSRC:
		/*
		 * According to the PXA250 spec, mic-in should use different
		 * DRCMR and different AC97 FIFO.
		 * Unfortunately current UCB1400 versions (up to ver 2A) don't
		 * produce slot 6 for the audio input frame, therefore the PXA
		 * AC97 mic-in FIFO is always starved.
		 */
#if 0
		ret = get_user(val, (int *)arg);
		if (ret)
			return ret;
		pxa_audio_clear_buf(&ac97_audio_in);
		*ac97_audio_in.drcmr = 0;
		if (val & (1 << SOUND_MIXER_MIC)) {
			ac97_audio_in.dcmd = DCMD_RXMCDR;
			ac97_audio_in.drcmr = &DRCMRRXMCDR;
			ac97_audio_in.dev_addr = __PREG(MCDR);
		} else {
			ac97_audio_in.dcmd = DCMD_RXPCDR;
			ac97_audio_in.drcmr = &DRCMRRXPCDR;
			ac97_audio_in.dev_addr = __PREG(PCDR);
		}
		if (ac97_audio_state.rd_ref)
			*ac97_audio_in.drcmr =
				ac97_audio_in.dma_ch | DRCMR_MAPVLD;
#endif
		break;
		if (machine_is_netbookpro()) {
			u16 vol;
			case SOUND_MIXER_WRITE_VOLUME:
				/* 
		 		* nbp does not use line out, so we must also change mono 
		 		* and hp volumes.
		 		*/
				vol = pxa_ac97_read(&pxa_ac97_codec, AC97_MASTER_VOL_STEREO);
				pxa_ac97_write(&pxa_ac97_codec, AC97_HEADPHONE_VOL, vol);
				pxa_ac97_write(&pxa_ac97_codec, AC97_MASTER_VOL_MONO, vol);
				break;
		}
		
	}
	return 0;
}

static struct file_operations mixer_fops = {
	ioctl:		mixer_ioctl,
	llseek:		no_llseek,
	owner:		THIS_MODULE
};

/*
 * AC97 codec ioctls
 */

static int codec_adc_rate = 48000;
static int codec_dac_rate = 48000;

static int ac97_ioctl(struct inode *inode, struct file *file,
		      unsigned int cmd, unsigned long arg)
{
	int ret;
	long val = 0;

	switch(cmd) {
	case SNDCTL_DSP_STEREO: /* Only stereo is supported */
		return put_user(1, (int *) arg);

	case SNDCTL_DSP_CHANNELS:
	case SOUND_PCM_READ_CHANNELS: /* Only stereo is supported */
		return put_user(2, (long *) arg);

	case SNDCTL_DSP_SPEED:
		ret = get_user(val, (long *) arg);
		if (ret)
			return ret;
		if (file->f_mode & FMODE_READ)
			codec_adc_rate = ac97_set_adc_rate(&pxa_ac97_codec, val);
		if (file->f_mode & FMODE_WRITE) {
			codec_dac_rate = ac97_set_dac_rate(&pxa_ac97_codec, val);
			printk("SNDCTL_DSP_SPEED: codec_dac_rate=%d\n",
			       codec_dac_rate);
		}

		/* fall through */
	case SOUND_PCM_READ_RATE:
		if (file->f_mode & FMODE_READ)
			val = codec_adc_rate;
		if (file->f_mode & FMODE_WRITE)
			val = codec_dac_rate;
		return put_user(val, (long *) arg);

	case SNDCTL_DSP_SETFMT:
	case SNDCTL_DSP_GETFMTS: /* Only one format is supported */
		return put_user(AFMT_S16_LE, (long *) arg);

	default:
		/* Maybe this is meant for the mixer (As per OSS Docs) */
		return mixer_ioctl(inode, file, cmd, arg);
	}
	return 0;
}


/*
 * Audio stuff
 */

static audio_stream_t ac97_audio_out = {
	name:			"AC97 audio out",
	dcmd:			DCMD_TXPCDR,
	drcmr:			&DRCMRTXPCDR,
	dev_addr:		__PREG(PCDR),
};

static audio_stream_t ac97_audio_in = {
	name:			"AC97 audio in",
	dcmd:			DCMD_RXPCDR,
	drcmr:			&DRCMRRXPCDR,
	dev_addr:		__PREG(PCDR),
};

static audio_state_t ac97_audio_state = {
	output_stream:		&ac97_audio_out,
	input_stream:		&ac97_audio_in,
	client_ioctl:		ac97_ioctl,
	sem:			__MUTEX_INITIALIZER(ac97_audio_state.sem),
};

static int ac97_audio_open(struct inode *inode, struct file *file)
{
	return pxa_audio_attach(inode, file, &ac97_audio_state);
}

static int pxa_ac97_suspend(struct device * dev, u32 state, u32 level)
{
	u16 reg, pwrmode;
	int ret;

	if ( level != SUSPEND_POWER_DOWN )
		return 0;


	switch (state) {
	case 1: pwrmode = AC97_PWR_D1; break;
	case 2: pwrmode = AC97_PWR_D2; break;
	case 3: pwrmode = AC97_PWR_D3; break;
	default: return 1;
	}

	ret = ac97_suspend(&pxa_ac97_codec, state);
	if (ret)
		return ret;

	reg = pxa_ac97_read(&pxa_ac97_codec, AC97_POWER_CONTROL);
	reg |= (pwrmode & ~(AC97_PWR_PR6|AC97_PWR_PR5|AC97_PWR_PR4));
	pxa_ac97_write(&pxa_ac97_codec, AC97_POWER_CONTROL, reg);
	udelay(500);

	reg = pxa_ac97_read(&pxa_ac97_codec, AC97_POWER_CONTROL);
	reg |= pwrmode;
	pxa_ac97_write(&pxa_ac97_codec, AC97_POWER_CONTROL, reg);

	GCR |= GCR_ACLINK_OFF;
	pxa_set_cken(CKEN2_AC97, 0);

	return 0;
}

static int pxa_ac97_resume(struct device * dev, u32 level)
{
	if ( level != RESUME_POWER_ON ) 
		return 0;

	pxa_set_cken(CKEN2_AC97, 1);

	if (machine_is_netbookpro()) {
		/* it seems this may well be the only way to
		 * get the ac97 setup on the netbookpro to play
		 * ball nicely...
		 */
		pxa_ac97_cold_reset();

		ac97_probe_codec(&pxa_ac97_codec);

	} else {
		pxa_ac97_warm_reset();
	}

	pxa_ac97_write(&pxa_ac97_codec, AC97_POWER_CONTROL, AC97_PWR_D0);

	ac97_resume(&pxa_ac97_codec);

	return 0;
}

/*
 * Missing fields of this structure will be patched with the call
 * to pxa_audio_attach().
 */

static struct file_operations ac97_audio_fops = {
	open:		ac97_audio_open,
	owner:		THIS_MODULE
};

#ifdef CONFIG_MACH_NETBOOKPRO
static void nbp_hp_remove(void)
{
	u16 reg;

	/* power down and mute headphones, unmute spk */
	reg = pxa_ac97_read(&pxa_ac97_codec, 0x5c);
	pxa_ac97_write(&pxa_ac97_codec, 0x5c, reg & ~0x0200);
	reg = pxa_ac97_read(&pxa_ac97_codec, AC97_HEADPHONE_VOL);
	pxa_ac97_write(&pxa_ac97_codec, AC97_HEADPHONE_VOL, reg | AC97_MUTE);
	reg = pxa_ac97_read(&pxa_ac97_codec, AC97_POWER_CONTROL);
	pxa_ac97_write(&pxa_ac97_codec, AC97_POWER_CONTROL, reg | AC97_PWR_PR6);
	reg = pxa_ac97_read(&pxa_ac97_codec, AC97_MASTER_VOL_MONO);
	pxa_ac97_write(&pxa_ac97_codec, AC97_MASTER_VOL_MONO, reg & ~AC97_MUTE);
	GPCR(65) = GPIO_bit(65);
}

static void nbp_hp_insert(void)
{
	u16 reg;
	
	/* power up headphones and mute spk, unmute hp */
	GPSR(65) = GPIO_bit(65);
	reg = pxa_ac97_read(&pxa_ac97_codec, AC97_POWER_CONTROL);
	pxa_ac97_write(&pxa_ac97_codec, AC97_POWER_CONTROL, reg & ~AC97_PWR_PR6);
	reg = pxa_ac97_read(&pxa_ac97_codec, AC97_HEADPHONE_VOL);
	pxa_ac97_write(&pxa_ac97_codec, AC97_HEADPHONE_VOL, reg & ~AC97_MUTE);
	reg = pxa_ac97_read(&pxa_ac97_codec, AC97_MASTER_VOL_MONO);
	pxa_ac97_write(&pxa_ac97_codec, AC97_MASTER_VOL_MONO, reg | AC97_MUTE);

}

static void nbp_hp_status_detect (void *ptr)
{
	/* do we have a headphone/MIC insertion or removal event */
	if (GPLR0 & 0x8) {
		nbp_hp_remove();
	} else {
		nbp_hp_insert();
	}
}

static struct workqueue_struct *nbp_hp_workq;
DECLARE_WORK(nbp_hp_status_work, nbp_hp_status_detect, NULL);

static void nbp_hp_debounce(unsigned long data)
{
	/* debounce should now be complete */
	queue_work(nbp_hp_workq, &nbp_hp_status_work);
}

static struct timer_list nbp_hp_timer = 
	TIMER_INITIALIZER(nbp_hp_debounce, 0, 0);

static irqreturn_t nbp_hp_irq(int irq, void *dev_id, struct pt_regs *regs)
{	
	/* debounce the jack insertion */
	mod_timer(&nbp_hp_timer, jiffies + HZ);

	return IRQ_HANDLED;
}

#endif

static int __devinit pxa_ac97_probe(struct device *dev)
{
	int ret;
	struct ac97_codec *dummy;

	ret = pxa_ac97_get(&dummy);
	if (ret)
		return ret;
	
	if (machine_is_netbookpro()) {
		if ((nbp_hp_workq = create_singlethread_workqueue("nbp-hp")) == NULL) {
			pxa_ac97_put();
			return -ENODEV;
		}
		
		if ((ret = request_irq(IRQ_GPIO(3), nbp_hp_irq, 0, "Headphones/MIC", NULL))) {
			pxa_ac97_put();
			destroy_workqueue(nbp_hp_workq);
			return -ENODEV;
		}
		set_irq_type(IRQ_GPIO(3), IRQT_BOTHEDGE);
		
		if (GPLR0 & 0x8)
			nbp_hp_remove();
		else
			nbp_hp_insert();
		
		/* not used on nbp */
		pxa_ac97_write(&pxa_ac97_codec, AC97_MASTER_VOL_STEREO, AC97_MUTE);
		pxa_ac97_write(&pxa_ac97_codec, AC97_CD_VOL, AC97_MUTE);
		
		/* keep startup quite */
		pxa_ac97_write(&pxa_ac97_codec, AC97_GENERAL_PURPOSE, 0x0);
		pxa_ac97_write(&pxa_ac97_codec, AC97_LINEIN_VOL, AC97_MUTE);
		pxa_ac97_write(&pxa_ac97_codec, AC97_MIC_VOL, AC97_MUTE);
	}

	ac97_audio_state.dev_dsp = register_sound_dsp(&ac97_audio_fops, -1);
	pxa_ac97_codec.dev_mixer = register_sound_mixer(&mixer_fops, -1);
	ac97_audio_state.output_stream->dev = dev;
	ac97_audio_state.input_stream->dev = dev;
	
	return 0;
}

static int __devexit pxa_ac97_remove(struct device *dev)
{
	unregister_sound_dsp(ac97_audio_state.dev_dsp);
	unregister_sound_mixer(pxa_ac97_codec.dev_mixer);
	if (machine_is_netbookpro()) {
		free_irq(IRQ_GPIO(3), NULL);
		destroy_workqueue(nbp_hp_workq);
	}
	pxa_ac97_put();
	ac97_audio_state.output_stream->dev = NULL;
	ac97_audio_state.input_stream->dev = NULL;
	return 0;
}

static struct device_driver pxa_ac97_driver = {
        .name           = "pxa-audio",
        .bus            = &platform_bus_type,
        .probe          = pxa_ac97_probe,
        .remove         = pxa_ac97_remove,
	.suspend	= pxa_ac97_suspend,
	.resume		= pxa_ac97_resume,
};
 
static int __init pxa_ac97_init(void)
{
        return driver_register(&pxa_ac97_driver);
}
  
static void __exit pxa_ac97_exit(void)
{
        driver_unregister(&pxa_ac97_driver);
}

module_init(pxa_ac97_init);
module_exit(pxa_ac97_exit);

MODULE_AUTHOR("Nicolas Pitre");
MODULE_DESCRIPTION("AC97 interface for the Cotula chip");
MODULE_LICENSE("GPL");
