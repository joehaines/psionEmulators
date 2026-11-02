/*
 * OHCI HCD (Host Controller Driver) for USB.
 *
 * (C) Copyright 1999 Roman Weissgaerber <weissg@vienna.at>
 * (C) Copyright 2000-2002 David Brownell <dbrownell@users.sourceforge.net>
 * (C) Copyright 2002 Hewlett-Packard Company
 *
 * SL811 Bus Glue
 *
 * Written by Christopher Hoover <ch@hpl.hp.com>
 * Based on fragments of previous driver by Rusell King et al.
 *
 * This file is licenced under the GPL.
 */

#include <linux/device.h>
#include <linux/timer.h>

#include <asm/hardware.h>
#include <asm/arch/dma.h>
#include <asm/mach-types.h>

#include <asm/arch/netbookpro-pcon.h>

#define SOF_B

#ifdef USE_TST_GPIOS
static inline void SET_GPIO(int n)
{
	BUG_ON(n >= ARRAY_SIZE(tst_gpios));
	gpio_set_bit(tst_gpios[n]);
}

static inline void CLR_GPIO(int n)
{
	BUG_ON(n >= ARRAY_SIZE(tst_gpios));
	gpio_clr_bit(tst_gpios[n]);
}

static inline void PULSE_GPIO(int n)
{
	SET_GPIO(n);
	CLR_GPIO(n);
}
#else
#define SET_GPIO(n) do {} while (0)
#define CLR_GPIO(n) do {} while (0)
#define PULSE_GPIO(n) do {} while (0)
#endif
#include <linux/kallsyms.h>



//static int hc_sl811_usb_reset(struct usb_hcd *hcd);

//static void hcd_sl811_release(struct usb_bus *bus);

#include "ohci-emu.c"

extern int usb_disabled(void);
#ifdef KERNEL_THREAD
static int ohci_sl811_thread(void *data);
#endif

static inline void hc_sl811_set_drvdata(struct hc_sl811_dev *dev, struct usb_hcd *data)
{
	dev_set_drvdata(&dev->dev, data);
}

static inline struct usb_hcd* hc_sl811_get_drvdata(struct hc_sl811_dev *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(&dev->dev);

	return hcd;
}

#if 0
static int usb_hcd_sl811_remove(struct hc_sl811_dev *dev);
#else
static int usb_hcd_sl811_remove(struct usb_hcd *hcd, struct hc_sl811_dev *dev);
#endif
/*-------------------------------------------------------------------------*/

//#define TEST_DMA
#ifdef TEST_DMA
static inline void _dma_move_data(int chan, u32 src, u32 dst, u32 dma_len)
{
	const unsigned int DMA_FLAGS = DCMD_INCTRGADDR | DCMD_INCSRCADDR | DCMD_BURST32;

	if (chan < 0) {
		return;
	}

	DDPRINTK("%s: Moving %d byte from %08x to %08x\n", __FUNCTION__,
		dma_len, src, dst);
	DCSR(chan) = DCSR_NODESC;
	DSADR(chan) = src;
	DTADR(chan) = dst;
	DCMD(chan) = DMA_FLAGS | (DCMD_LENGTH & dma_len);
	DDPRINTK("%s: DMA started: DCSR=%08x, DSADR=%08x, DTADR=%08x, DCMD=%08x\n", __FUNCTION__,
		 DCSR(chan), DSADR(chan), DTADR(chan), DCMD(chan));
	DCSR(chan) |= DCSR_RUN;
	while (!(DCSR(chan) & DCSR_STOPSTATE));
	DDPRINTK("%s: DMA stopped: DCSR=%08x, DSADR=%08x, DTADR=%08x, DCMD=%08x\n", __FUNCTION__,
		DCSR(chan), DSADR(chan), DTADR(chan), DCMD(chan));
	DCSR(chan) = DCSR_NODESC;
}

static inline void _dma_copy_data(int chan, u32 src, u32 dst, u32 dma_len)
{
	void *dma_buf;
	int src_align = src & 7;
	int dst_align = dst & 7;

	DDPRINTK("%s: Copying %d byte from %08x(%d) to %08x(%d)\n", __FUNCTION__,
		dma_len, src, src_align, dst, dst_align);
	if ((src_align | dst_align) == 0) {
		_dma_move_data(chan, src, dst, dma_len);
	} else {
		dma_addr_t dma_addr;

		dma_buf = pci_alloc_consistent(NULL, dma_len + 8, &dma_addr);
		DDPRINTK("%s: temporary buffer allocated @ %08x:%08x\n", __FUNCTION__,
			 (u32)dma_buf, dma_addr);
		_dma_move_data(chan, src - src_align, dma_addr, dma_len + src_align);
		consistent_sync(dma_buf, SL811_BUF_SIZE, PCI_DMA_FROMDEVICE);
		if (src_align != dst_align) {
			DDPRINTK("%s: Shifting %d byte from %08x to %08x\n", __FUNCTION__,
				 dma_len, (u32)dma_buf + src_align, (u32)dma_buf + dst_align);
			_memdmp(dma_buf, dma_len, dma_addr);
			memmove(dma_buf + dst_align, dma_buf + src_align, dma_len);
			_memdmp(dma_buf, dma_len + dst_align, dma_addr);
			consistent_sync(dma_buf, SL811_BUF_SIZE, PCI_DMA_TODEVICE);
		}
		_dma_move_data(chan, dst - dst_align, dma_addr, dst_align);
		_memdmp(dma_buf, dst_align, dma_addr);
		_dma_move_data(chan, dma_addr, dst - dst_align, dma_len + dst_align);
		_memdmp(dma_buf, dma_len + dst_align, dma_addr);
		consistent_sync(dma_buf, SL811_BUF_SIZE, PCI_DMA_FROMDEVICE);
		pci_free_consistent(NULL, dma_len + 8, dma_buf, dma_addr);
	}
}
#endif

static int sl811_start_hc(struct hc_sl811_dev *dev)
{
	int i;
	int ret = 0;
#ifdef TEST_DMA
	struct usb_hcd *hcd = hc_sl811_get_drvdata(dev);
	u8 *src_buf = kmalloc(SL811_BUF_SIZE, GFP_DMA);
	u8 *dst_buf = kmalloc(SL811_BUF_SIZE, GFP_DMA);
	u32 src;
	u32 dst;
#endif

	DDPRINTK("%s: Starting SL811 OHCI USB Controller\n", __FUNCTION__);

#if 0
	for (i = 0; i < ARRAY_SIZE(tst_gpios); i++) {
		CLR_GPIO(i);
		pxa_gpio_mode(tst_gpios[i] | GPIO_OUT);
	}
#endif
#if 0
	for (i = 0; i < (1 << (ARRAY_SIZE(tst_gpios) - 1)); i++) {
		int bit;
		int carry = 1;

		for (bit = 0; carry && (bit < ARRAY_SIZE(tst_gpios)); bit++) {
			if (gpio_tst_bit(tst_gpios[bit])) {
				CLR_GPIO(bit);
				carry = 1;
			} else {
				SET_GPIO(bit);
				carry = 0;
			}
		}
	}
#endif
	/*
	 * Configure the power sense and control lines.  Place the USB
	 * host controller in reset.
	 */
#ifdef CONFIG_ARCH_KARO
	karo_pcf8574_clr(FMsk(KARO_PCF8574_USBRST));
#endif
#ifdef CONFIG_MACH_NETBOOKPRO
	GPCR(14) = GPIO_bit(14);
#endif

	/*
	 * Now, carefully enable the USB clock, and take
	 * the USB host controller out of reset.
	 */
#ifdef CONFIG_ARCH_KARO
	karo_pcf8574_set(FMsk(KARO_PCF8574_USBRST));
#endif
#ifdef CONFIG_MACH_NETBOOKPRO
	GPSR(14) = GPIO_bit(14);
#endif


	DDPRINTK("%s: Testing SL811 register access\n", __FUNCTION__);
#if DEBUG_BUF_SIZE > 0
	debug_on = 0;
#endif
	for (i = 0; i < SL811_BUF_SIZE; i++) {
		hc_sl811_write_reg(dev, SL11H_DATA_START + i, ~i);
	}

	HC_SL811_WRITE_ADDR(dev, SL11H_DATA_START);
	for (i = 0; i < SL811_BUF_SIZE; i++) {
		u8 val = HC_SL811_READ_DATA(dev);
		u8 ref = ~i;
		if (val != ref) {
			DPRINTK("%s: SL811 register test failed @ %02x: wr: %02x, rd %02x\n",
				__FUNCTION__, i, ref, val);
			ret = 1;
			break;
		}
	}

	DDPRINTK("%s: Clearing SL811 buffer\n", __FUNCTION__);
	for (i = 0; i < SL811_BUF_SIZE; i++) {
		hc_sl811_write_reg(dev, SL11H_DATA_START + i, 0);
	}
	ENABLE_DEBUG;

	dev->hw_rev = hc_sl811_read_reg(dev, SL11H_HWREVREG) >> 4;
	if (dev->hw_rev == 0) {
		DPRINTK("%s: Bad HW rev\n", __FUNCTION__);
		return -ENODEV;
	}
	// enable Host mode
	hc_sl811_write_reg(dev, SL11H_CTLREG2, SL11H_CTL2MASK_HOSTMODE);

#ifdef TEST_DMA
	DDPRINTK("%s: Testing DMA capabilities\n", __FUNCTION__);
	src = dma_map_single(hcd->self.controller, src_buf, SL811_BUF_SIZE, DMA_TO_DEVICE);
	dst = dma_map_single(hcd->self.controller, dst_buf, SL811_BUF_SIZE, DMA_FROM_DEVICE);
	if (src == 0 || dst == 0) {
		DDPRINTK("%s: Failed to map DMA buffers\n", __FUNCTION__);
	} else {
		for (i = 0; i < SL811_BUF_SIZE; i++) {
			src_buf[i] = i;
		}
		memset(dst_buf, 0xee, SL811_BUF_SIZE);
		dma_sync_single(hcd->self.controller, src, SL811_BUF_SIZE, DMA_TO_DEVICE);
		_dma_copy_data(0, src, dst, SL811_BUF_SIZE);
		if (memcmp(src_buf, dst_buf, SL811_BUF_SIZE) == 0) {
			DDPRINTK("%s: DMA move succeeded\n", __FUNCTION__);
		} else {
			DPRINTK("%s: DMA move failed:\n", __FUNCTION__);
			_memdmp(dst_buf, SL811_BUF_SIZE, dst);
			_memdmp(src_buf, SL811_BUF_SIZE, src);
		}
	}
	dma_unmap_single(hcd->self.controller, src, SL811_BUF_SIZE, DMA_TO_DEVICE);
	dma_unmap_single(hcd->self.controller, dst, SL811_BUF_SIZE, DMA_FROM_DEVICE);
	kfree(src_buf);
	kfree(dst_buf);
	return -ENODEV;
#endif
		
#ifdef KERNEL_THREAD_
	DDPRINTK("%s: Starting kernel thread\n", __FUNCTION__);
	ret = kernel_thread(ohci_sl811_thread, dev, 0);
	if (ret <= 0) {
		printk("%s: Failed to start kernel thread %d\n", __FUNCTION__, ret);
		if (ret == 0) {
			ret = -ENOMEM;
		}
	} else {
		wait_for_completion(&dev->complete);
		DDPRINTK("%s: Kernel thread started\n", __FUNCTION__);
		ret = 0;
	}
#endif
	return ret;
}

static void sl811_stop_hc(struct hc_sl811_dev *dev)
{
	DPRINTK("%s: stopping SL811 OHCI USB Controller\n",  __FUNCTION__);

	/*
	 * Put the USB host controller into reset.
	 */
#ifdef CONFIG_ARCH_KARO
	karo_pcf8574_clr(FMsk(KARO_PCF8574_USBRST));
#endif
#ifdef CONFIG_MACH_NETBOOKRP
	GPCR(14) = GPIO_bit(14);
#endif

#if DEBUG_BUF_SIZE > 0
	print_debug_buffer();
#endif
	/*
	 * Stop the USB clock.
	 */
	// No way to do it on KARO boards
#ifdef KERNEL_THREAD
	if (dev->task) {
		dev->flags.b.shutdown = 1;
		if (waitqueue_active(&dev->task_wait)) {
			DDPRINTK("%s: Waking up kernel thread\n", __FUNCTION__);
			wake_up_interruptible(&dev->task_wait);
		} else {
			DDPRINTK("%s: Wait queue not active\n", __FUNCTION__);
		}
		DDPRINTK("%s: Waiting for kernel thread to terminate\n", __FUNCTION__);
		wait_for_completion(&dev->complete);
		DDPRINTK("%s: kernel thread terminated\n", __FUNCTION__);
	} else {
		DDPRINTK("%s: Kernel thread not yet started\n", __FUNCTION__);
	}
#endif
}

/*
 * initialize the emulated OHCI registers
 */
static void sl811_reset_hc(struct usb_hcd *hcd, int cold)
{
	struct ohci_hcd *ohci = hcd_to_ohci(hcd);
	struct ohci_regs *regs = hcd->regs;
	struct hc_sl811_dev *dev = hcd_to_sl811_dev(hcd);
	const u8 nr_ports = 1;
	const u8 pwr_on_delay = 100;

	DDPRINTK("%s: Performing %s reset of HC SL811, regs=%08x->%08x\n", __FUNCTION__,
		 cold ? "cold" : "warm", (u32)ohci->regs, (u32)regs);
	ohci->regs = regs;

	DDPRINTK("%s: Disabling SL811 interrupts\n", __FUNCTION__);
	hc_sl811_write_reg(dev, SL11H_INTENBLREG, 0);
	DDPRINTK("%s: Resetting SL811 chip\n", __FUNCTION__);
	hc_sl811_write_reg(dev, SL11H_CTLREG1, SL11H_CTL1VAL_RESET);

	if (cold) {
		memzero(regs, sizeof(struct ohci_regs));
		regs->revision = 0x10; // OHCI Rev.
		regs->control = OHCI_USB_RESET;
		regs->roothub.a = RH_A_PSM | RH_A_NOCP | nr_ports | (pwr_on_delay << 24);
		regs->roothub.status = 0;
		regs->roothub.portstatus[0] = RH_PS_PPS;
	} else {
		regs->control = OHCI_USB_SUSPEND;
		regs->ed_controlcurrent = 0;
		regs->ed_controlhead = 0;
		regs->ed_bulkcurrent = 0;
		regs->ed_bulkhead = 0;
	}
}

static void sl811_dma_irq(int dma, void *dummy, struct pt_regs *regs)
{
	printk(KERN_WARNING "%s: Ooops! How did we get **HERE**?\n", __FUNCTION__);
	DPRINTK("%s: Stopping DMA channel %d\n", __FUNCTION__, dma);
	DCSR(dma) = 0;
}

#define RESET_WIDTH 50

/*
  INSRMV interrupt
  enable SL11H_INTMASK_USBRESET
*/
static void hc_sl811_start_sof(struct usb_hcd *hcd, int connected)
{
	struct hc_sl811_dev *dev = hcd_to_sl811_dev(hcd);
	struct ohci_hcd *ohci = hcd_to_ohci(hcd);
	u16 sof_ctr = (ohci->regs->fminterval & OHCI_FR) + 1;
	u8 ctrl2 = SL11H_CTL2MASK_HOSTMODE | (sof_ctr >> 8);
	u8 ctrl1 = SL11H_CTL1MASK_SOFENA;

	DPRINTK( "%s: connected=%d\n", __func__, connected );

	switch (connected) {
	case 0:
		DPRINTK("%s: Disabling SOF\n", __FUNCTION__);
		hc_sl811_clr_mask(dev, SL11H_CTLREG1, SL11H_CTL1MASK_SOFENA);
		break;
	case 1:
		ctrl1 |= SL11H_CTL1MASK_NSPD;
		ctrl2 |= SL11H_CTL2MASK_DSWAP;
		//hc_sl811_write_reg(dev, SL11H_CTLREG1, SL11H_CTL1VAL_FORCE_J);
		// fall through
	case 2:
		DPRINTK("%s: Enabling SOF %02x\n", __FUNCTION__, ctrl2);
#ifdef SOF_B
		hc_sl811_write_reg(dev, SL11H_BUFLNTHREG_B, 0);	//zero lenth
		hc_sl811_write_reg(dev, SL11H_PIDEPREG_B, 0x50);	//send SOF to EP0       
		hc_sl811_write_reg(dev, SL11H_DEVADDRREG_B, 0x01);	//address0
#else
		hc_sl811_write_reg(dev, SL11H_BUFLNTHREG, 0);	//zero lenth
		hc_sl811_write_reg(dev, SL11H_PIDEPREG, 0x50);	//send SOF to EP0       
		hc_sl811_write_reg(dev, SL11H_DEVADDRREG, 0x01);	//address0
#endif
		hc_sl811_write_reg(dev, SL11H_SOFLOWREG, (u8)sof_ctr);	// start sending SOF tokens

#ifdef SOF_B
		hc_sl811_set_mask(dev, SL11H_INTENBLREG, SL11H_INTMASK_XFERDONE_B);
#else
		hc_sl811_set_mask(dev, SL11H_INTENBLREG, SL11H_INTMASK_XFERDONE);
#endif
		hc_sl811_write_reg(dev, SL11H_CTLREG2, ctrl2);
		hc_sl811_write_reg(dev, SL11H_CTLREG1, ctrl1);
#ifdef SOF_B
		hc_sl811_write_reg(dev, SL11H_HOSTCTLREG_B, SL11H_HCTLMASK_ARM);
#else
		hc_sl811_write_reg(dev, SL11H_HOSTCTLREG, SL11H_HCTLMASK_ARM);
#endif
	}
	//hc_sl811_write_reg(dev, SL11H_INTSTATREG, 0xff);
}

static int hc_sl811_handle_insrmv(struct usb_hcd *hcd)
{
	int ret = 0;
	struct ohci_hcd *ohci = hcd_to_ohci(hcd);
	struct hc_sl811_dev *dev = hcd_to_sl811_dev(hcd);
	u8 sl811_stat;
	u8 int_mask;

	DDPRINTK("%s: \n", __FUNCTION__);

	hc_sl811_set_mask(dev, SL11H_INTENBLREG, SL11H_INTMASK_USBRESET);
	hc_sl811_write_reg(dev, SL11H_INTSTATREG, SL11H_INTMASK_USBRESET | SL11H_INTMASK_INSRMV);
	mdelay(10);
	sl811_stat = hc_sl811_read_reg(dev, SL11H_INTSTATREG);

	if (sl811_stat & SL11H_INTMASK_USBRESET) {
		DDPRINTK("%s: Device removed: %02x\n", __FUNCTION__, sl811_stat);
		ohci_sl811_update_portstatus(ohci, 0, 0, RH_PS_CCS | RH_PS_PES);
		int_mask = SL11H_INTMASK_INSRMV;
	} else {
		int_mask = SL11H_INTMASK_INSRMV | SL11H_INTMASK_SOFINTR | SL11H_INTMASK_XFERDONE;
		if (sl811_stat & SL11H_INTMASK_DSTATE) {
			DDPRINTK("%s: Full speed device attached: %02x\n", __FUNCTION__, sl811_stat);
			ret = 2;
			ohci_sl811_update_portstatus(ohci, 0, RH_PS_CCS | RH_PS_PES, RH_PS_PRS);
		} else {
			DDPRINTK("%s: Low speed device attached: %02x\n", __FUNCTION__, sl811_stat);
			ret = 1;
			ohci_sl811_update_portstatus(ohci, 0, RH_PS_CCS | RH_PS_PES | RH_PS_LSDA, RH_PS_PRS);
		}
	}
	DDPRINTK("%s: Enabling interrupts: %02x\n", __FUNCTION__, int_mask);
	hc_sl811_write_reg(dev, SL11H_INTENBLREG, int_mask);

	return ret;
}

static void hc_sl811_usb_reset(struct usb_hcd *hcd, int assert)
{
	struct hc_sl811_dev *dev = hcd_to_sl811_dev(hcd);

#if DEBUG_BUF_SIZE > 0
	print_debug_buffer();
#endif
	SET_GPIO(0);
	if (assert) {
		DDPRINTK("%s: %sing reset @ %12u\n", __FUNCTION__,
			 assert > 0 ? "assert" : "puls", OSCR);
		SET_GPIO(1);
		hc_sl811_write_reg(dev, SL11H_INTENBLREG, 0);
		hc_sl811_write_reg(dev, SL11H_CTLREG1, SL11H_CTL1VAL_RESET);
		DDPRINTK("%s: Clearing interrupt status %02x\n", __FUNCTION__,
			 hc_sl811_read_reg(dev, SL11H_INTSTATREG));
		hc_sl811_write_reg(dev, SL11H_INTSTATREG, 0xff);
	}

	if (assert < 0) {
		if (in_interrupt()) {
			mdelay(RESET_WIDTH);
		} else {
			schedule_timeout((RESET_WIDTH * HZ) / 1000);
		}
	}

	if (assert <= 0) {
		DDPRINTK("%s: deasserting reset @ %12u\n", __FUNCTION__, OSCR);
		CLR_GPIO(1);
		DDPRINTK("%s: Clearing interrupt status %02x\n", __FUNCTION__,
			 hc_sl811_read_reg(dev, SL11H_INTSTATREG));
		hc_sl811_write_reg(dev, SL11H_INTSTATREG, 0xff);
		hc_sl811_write_reg(dev, SL11H_CTLREG1, 0);

		hc_sl811_write_reg(dev, SL11H_INTENBLREG, SL11H_INTMASK_INSRMV);
	}
	CLR_GPIO(0);
	print_debug_buffer();
}

#ifdef KERNEL_THREAD
static int hc_sl811_handle_resume(struct usb_hcd *hcd)
{
	return 0;
}

static int hc_sl811_do_suspend(struct usb_hcd *hcd)
{
	return 0;
}

static void timer_event(unsigned long data)
{
	struct hc_sl811_dev *dev = (void*)data;

	dev->flags.b.timer = 1;
	if (waitqueue_active(&dev->task_wait)) {
		wake_up_interruptible(&dev->task_wait);
	}
}

/*
 * state machine to keep track of device insertion/removal events
 * and guarantee the required reset timing
 *
 */
typedef enum {
	ST_INIT,
	ST_IDLE,
	ST_RESET,
	ST_TIMER,
	ST_CONNECT,
	ST_INSRMV,
	ST_RESUME,
	ST_SUSPEND,
} st_t;

/*
  State		Event		Action					NewState
  ST_INIT	Reset		Assert Reset, Activate Timer		ST_TIMER
  ST_TIMER	Timer		Deassert Reset				ST_IDLE
  ST_IDLE
*/

static int ohci_sl811_thread(void *data)
{
	int ret = 0;
	struct task_struct *tsk = current;
	struct hc_sl811_dev *dev = data;
	struct usb_hcd *hcd = hc_sl811_get_drvdata(dev);
	//struct ohci_hcd *ohci = hcd_to_ohci(hcd);
	int running = 1;
	st_t state = ST_INIT;
	st_t new_state = ST_INIT;
	st_t last_state;
	static struct timer_list timer;

	dev->task = current;

	current->policy = SCHED_FIFO;
	current->rt_priority = 1;
	daemonize("kSL811d");

	/* only want to receive SIGKILL */
	spin_lock_irq(&tsk->sighand->siglock);
	siginitsetinv(&tsk->blocked, sigmask(SIGKILL));
	recalc_sigpending();
	spin_unlock_irq(&tsk->sighand->siglock);

	sl811_reset_hc(hcd, 1);

	complete(&dev->complete);

	while (running) {
		state = new_state;
		switch (state) {
		case ST_INIT:
			init_timer(&timer);
			timer.function = timer_event;
			timer.data = (unsigned long)dev;
			new_state = ST_IDLE;
			state = new_state;
			// fall through
		case ST_IDLE:
			wait_event_interruptible(dev->task_wait, dev->flags.i);
			break;
		case ST_TIMER:
			del_timer(&timer);
			new_state = ST_IDLE;
			break;
		case ST_INSRMV:
			if (hc_sl811_handle_insrmv(hcd)) {
				new_state = ST_CONNECT;
			} else {
				break;
			}
			// fall through if connected
		case ST_CONNECT:
			hc_sl811_start_sof(hcd, 1);
			new_state = ST_IDLE;
			break;
		case ST_RESET:
			new_state = ST_TIMER;
			break;
		case ST_RESUME:
			hc_sl811_handle_resume(hcd);
			break;
		case ST_SUSPEND:
			hc_sl811_do_suspend(hcd);
			break;
		default:
			DPRINTK("%s: Invalid state %d\n", __FUNCTION__, state);
		}
		last_state = state;

		if (signal_pending(tsk)) {
			DDPRINTK("%s: Taking emergency exit\n", __FUNCTION__);
			break;
		}

		if (dev->flags.b.shutdown) {
			DDPRINTK("%s: Shutting down kernel thread\n", __FUNCTION__);
			running = 0;
			break;
		} else if (dev->flags.b.portreset) {
			dev->flags.b.portreset = 0;

			DDPRINTK("%s: Resetting port 1\n", __FUNCTION__);
			hc_sl811_usb_reset(hcd, -1);
			add_timer(&timer);
			timer.expires = jiffies + ((RESET_DELAY * HZ) / 1000);
			new_state = ST_TIMER;
		} else if (dev->flags.b.timer) {
			dev->flags.b.timer = 0;
			DDPRINTK("%s: Timer expired\n", __FUNCTION__);
			new_state = ST_TIMER;
		}
		if (dev->flags.b.roothub_portstatus) {
			dev->flags.b.roothub_portstatus = 0;
		}
		if (dev->flags.b.roothub_status) {
			u32 status = ohci->regs->roothub.status;

			DPRINTK("%s: rh_status changed to %08x\n", __FUNCTION__,
				status);
			if (status & RH_HS_CRWE) {
				status &= ~(RH_HS_CRWE | RH_HS_DRWE);
				hc_sl811_clr(dev, SL11H_CTLREG1, SL11H_CTL1MASK_SUSPEND);
			}
			if (status & RH_HS_DRWE) {
				hc_sl811_set(dev, SL11H_CTLREG1, SL11H_CTL1MASK_SUSPEND);
			}
			if (status & RH_HS_OCIC) {
				status &= ~RH_HS_OCIC;
			}
			regs->roothub.status = status;
			ohci->regs->roothub.status = status;
			dev->flags.b.roothub_status = 0;
		}
		if (dev->flags.b.roothub_a) {
			dev->flags.b.roothub_a = 0;
		}
		if (dev->flags.b.roothub_b) {
			dev->flags.b.roothub_b = 0;
		}
	}

	dev->task = NULL;
	DDPRINTK("%s: Finishing\n", __FUNCTION__);
	complete_and_exit(&dev->complete, ret);

	return ret;
}
#endif

/*-------------------------------------------------------------------------*/

/* configure so an HC device and id are always provided */
/* always called with process context; sleeping is OK */


/**
 * usb_hcd_sl811_probe - initialize HC-SL811-based HCDs
 * Context: !in_interrupt()
 *
 * Allocates basic resources for this USB host controller, and
 * then invokes the start() method for the HCD associated with it
 * through the hotplug entry's driver_data.
 *
 * Store this function in the HCD's struct pci_driver as probe().
 */
static int usb_hcd_sl811_probe(struct hc_driver *driver, struct usb_hcd **hcd_out, struct hc_sl811_dev *dev)
{
	int retval;
	struct usb_hcd *hcd = NULL;
	struct device *parent = dev->dev.parent;
	//struct platform_device *pdev = to_platform_device(parent);
	const char *chip_names[] = {
		"SL11H",
		"SL811HS Rev 1.2",
		"SL811HS Rev 1.5",
		"Unknown"};

#ifdef KERNEL_THREAD
	init_completion(&dev->complete);
	init_waitqueue_head(&dev->task_wait);

	//init_waitqueue_head(&dev->update);
	//init_MUTEX(&dev->hw_lock);
#endif

	DDPRINTK("%s: Allocating hcd struct\n", __FUNCTION__);
	hcd = driver->hcd_alloc();
	if (hcd == NULL) {
		DPRINTK("hcd_alloc failed\n");
		retval = -ENOMEM;
		goto out;
	}

	DDPRINTK("%s: Allocating hcd register struct\n", __FUNCTION__);
	hcd->regs = kmalloc(sizeof(struct ohci_regs), GFP_KERNEL);
	if (hcd->regs == NULL) {
		retval = -ENOMEM;
		goto err1;
	}
	hc_sl811_set_drvdata(dev, hcd);

	if (sl811_start_hc(dev) != 0) {
		DPRINTK("%s: Failed to start SL811 USB host controller\n", __FUNCTION__);
		retval = -ENODEV;
		goto err2;
	}

	hcd->driver = driver;
	hcd->description = driver->description;
	hcd->self.bus_name = "sl811"; // ??
	if (hcd->product_desc == NULL) {
		hcd->product_desc = "HC-SL811 OHCI";
	}
	hcd->self.controller = parent;
	DDPRINTK("%s: controller=%08x(%08x)\n", __FUNCTION__, (u32)parent, (u32)&dev->dev);

	DDPRINTK("%s: Setting dma mask of device %08x\n", __FUNCTION__, (u32)hcd->self.controller);
	retval = dma_set_mask(hcd->self.controller, (u64)0x0fffffff8);
	if (retval) {
		printk("%s: Failed to set dma_mask to %08llx\n", __FUNCTION__, 
		       (u64)0x0fffffff8);
		goto err2;
	}		
	//hcd->self.controller->dma_mask = &(u64)0x0fffffff8;
	DDPRINTK("%s: dma_mask=%08llx\n", __FUNCTION__, *hcd->self.controller->dma_mask);

	// done by sl811_reset_hc: memzero(hcd->regs, sizeof(struct ohci_regs));
#if 1
#ifdef KERNEL_THREAD
	DDPRINTK("%s: Starting kernel thread\n", __FUNCTION__);
	retval = kernel_thread(ohci_sl811_thread, dev, 0);
	if (retval <= 0) {
		printk("%s: Failed to start kernel thread %d\n", __FUNCTION__, retval);
		if (retval == 0) {
			retval = -ENOMEM;
			goto err2;
		}
	} else {
		wait_for_completion(&dev->complete);
		DDPRINTK("%s: Kernel thread started\n", __FUNCTION__);
		retval = 0;
	}
#else
	sl811_reset_hc(hcd, 1);
#endif
#endif
	DDPRINTK("%s: Creating hcd buffer pool\n", __FUNCTION__);
	retval = hcd_buffer_create(hcd);
	if (retval != 0) {
		DPRINTK("pool alloc failed\n");
		goto err2;
	}

	DDPRINTK("%s: Resetting USB controller\n", __FUNCTION__);
	/* till now HC has been in an indeterminate state ... */
	if (driver->reset && (retval = driver->reset(hcd)) < 0) {
		dev_err(hcd->self.controller, "can't reset\n");
		goto err3;
	}
	hcd->state = USB_STATE_HALT;

	DDPRINTK("%s: Requesting IRQ %d (GPIO%d)\n", __FUNCTION__, dev->irq, IRQ_TO_GPIO(dev->irq));
	retval = request_irq(dev->irq, ohci_sl811_interrupt, SA_INTERRUPT, hcd->description, hcd);
	if (retval != 0) {
		DPRINTK("request_irq failed\n");
		retval = -EBUSY;
		goto err3;
	}
	hcd->irq = dev->irq;
	set_irq_type(hcd->irq, IRQT_RISING);

	DDPRINTK("%s: Requesting DMA channel\n", __FUNCTION__);
	retval = pxa_request_dma((char*)hcd_name, DMA_PRIO_HIGH, sl811_dma_irq, NULL);
	if (retval < 0) {
		DPRINTK("request_dma failed: %d\n", retval);
		goto err4;
	}
	dev->dma = retval;

#ifdef CONSISTENT_ALLOC
	dev->dma_buf = consistent_alloc(GFP_DMA, SL811_BUF_SIZE, &dev->dma_bus_addr, 0);
	if (dev->dma_buf == NULL) {
		DPRINTK("%s: Failed to allocate DMA buffer\n", __FUNCTION__);
		retval = -ENOMEM;
		goto err4;
	}
#endif
	info("%s (HC-%s) at 0x%p, irq %d DMA %d\n", hcd->description,
	     chip_names[dev->hw_rev & 0x0f], hcd->regs, hcd->irq, dev->dma);

	usb_bus_init(&hcd->self);
	hcd->self.op = &usb_hcd_operations;
	hcd->self.hcpriv = hcd;
	//hcd->self.release = hcd_sl811_release;

	INIT_LIST_HEAD(&hcd->dev_list);

	DDPRINTK("adding hdc->self=%p, hcd->self->controller=%p\n",
		 &hcd->self, hcd->self.controller);

	usb_register_bus(&hcd->self);

	DDPRINTK("calling driver->start(%p)\n", hcd);

	if ((retval = driver->start(hcd)) < 0) {
		usb_hcd_sl811_remove(hcd, dev);
		return retval;
	}

	DDPRINTK("setting hcd_out (%p) to %p\n", hcd_out, hcd);
	*hcd_out = hcd;

	return 0;

 err4:
	if (dev->dma >= 0) {
		pxa_free_dma(dev->dma);
		dev->dma = -1;
	}
	free_irq(dev->irq, hcd);
 err3:
	hcd_buffer_destroy(hcd);
 err2:
	sl811_stop_hc(dev);
 err1:
	kfree(hcd->regs);
	driver->hcd_free(hcd);
 out:

	return retval;
}

/* may be called without controller electrically present */
/* may be called with controller, bus, and devices active */

/**
 * usb_hcd_sl811_remove - shutdown processing for HC-SL811-based HCDs
 * @dev: USB Host Controller being removed
 * Context: !in_interrupt()
 *
 * Reverses the effect of usb_hcd_sl811_probe(), first invoking
 * the HCD's stop() method.  It is always called from a thread
 * context, normally "rmmod", "apmd", or something similar.
 *
 */
static int usb_hcd_sl811_remove(struct usb_hcd *hcd, struct hc_sl811_dev *dev)
{
	struct usb_device	*hub;

	dev_info(hcd->self.controller, "remove, state %x\n", hcd->state);

	BUG_ON(in_interrupt());

	hub = hcd->self.root_hub;
	if (HCD_IS_RUNNING (hcd->state)) {
		hcd->state = USB_STATE_QUIESCING;
	}

	DDPRINTK("%s: roothub graceful disconnect\n", hcd->self.bus_name);
	usb_disconnect(&hub);

	DDPRINTK("%s: Stopping driver\n", __FUNCTION__);
	hcd->driver->stop(hcd);
	DDPRINTK("%s: Destroying buffers\n", __FUNCTION__);
	hcd_buffer_destroy(hcd);
	hcd->state = USB_STATE_HALT;

	hc_sl811_set_drvdata(dev, NULL);

	if (dev->dma >= 0) {
		DDPRINTK("%s: Freeing DMA channel %d\n", __FUNCTION__, dev->dma);
		pxa_free_dma(dev->dma);
	}

	DDPRINTK("%s: Freeing IRQ %d\n", __FUNCTION__, hcd->irq);
	free_irq(hcd->irq, hcd);

	DPRINTK("%s: Deregistering bus\n", __FUNCTION__);
	usb_deregister_bus(&hcd->self);

	hcd->driver->hcd_free(hcd); //done in hcd_sl811_release

	sl811_stop_hc(dev);

	return 0;
}

/*
 * perform OHCI (PCI) bus reset
 */
static int ohci_sl811_reset(struct usb_hcd *hcd)
{
	int ret;
	struct ohci_hcd *ohci = hcd_to_ohci(hcd);

	DDPRINTK("%s: Resetting HC SL811 regs=%08x->%08x\n", __FUNCTION__,
		(u32)ohci->regs, (u32)hcd->regs);
	ohci->regs = hcd->regs;

	ret = hc_reset(ohci);

	return ret;
}

static int __devinit ohci_sl811_start(struct usb_hcd *hcd)
{
	struct ohci_hcd	*ohci = hcd_to_ohci(hcd);
	int ret;

	DPRINTK("%s: hcd=%08x\n", __FUNCTION__, (u32)hcd);

	ohci->hcca = pci_alloc_consistent(NULL, sizeof(*ohci->hcca), &ohci->hcca_dma);
	if (!ohci->hcca) {
		return -ENOMEM;
	}

	DDPRINTK("%s: Clearing HCCA @ %08x\n", __FUNCTION__, (u32)ohci->hcca);
	memzero(ohci->hcca, sizeof(struct ohci_hcca));

	DDPRINTK("%s: Initializing ohci memory\n", __FUNCTION__);
	ret = ohci_mem_init(ohci);
	if (ret < 0) {
		ohci_stop(hcd);
		return ret;
	}

	DDPRINTK("%s: Starting OHCI host controller\n", __FUNCTION__);
	if (hc_start(ohci) < 0) {
		err("can't start %s", ohci->hcd.self.bus_name);
		ohci_stop(hcd);
		return -EBUSY;
	}
	create_debug_files(ohci);

//#ifdef	DEBUG
	ohci_dump(ohci, 1);
//#endif
	return 0;
}

/*-------------------------------------------------------------------------*/
#ifdef CONFIG_PM
static int ohci_sl811_suspend(struct usb_hcd *hcd, u32 state)
{
	DDPRINTK("%s: \n", __FUNCTION__);
	return 0;
}

static int ohci_sl811_resume(struct usb_hcd *hcd)
{
	DDPRINTK("%s: \n", __FUNCTION__);

	hc_restart(hcd_to_ohci(hcd));

	return 0;
}
#endif

static struct hc_driver ohci_sl811_hc_driver = {
	.description		= hcd_name,

	/*
	 * generic hardware linkage
	 */
	.irq			= ohci_irq,
	.flags			= HCD_MEMORY | HCD_USB11,

	/*
	 * basic lifecycle operations
	 */
	.reset			= ohci_sl811_reset,
	.start			= ohci_sl811_start,
#ifdef	CONFIG_PM
	.suspend		= ohci_sl811_suspend,
	.resume			= ohci_sl811_resume,
#endif
	.stop			= ohci_stop,

	/*
	 * memory lifecycle (except per-request)
	 */
	.hcd_alloc		= ohci_hcd_alloc,
	.hcd_free		= ohci_hcd_free,

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue		= ohci_urb_enqueue,
	.urb_dequeue		= ohci_urb_dequeue,
	.endpoint_disable	= ohci_endpoint_disable,

	/*
	 * scheduling support
	 */
	.get_frame_number	= ohci_get_frame,

	/*
	 * root hub support
	 */
	.hub_status_data	= ohci_hub_status_data,
	.hub_control		= ohci_hub_control,
};

/*-------------------------------------------------------------------------*/
#define res_len(p) ((p)->end - (p)->start + 1)
static int ohci_hcd_sl811_drv_probe(struct hc_sl811_dev *dev)
{
	struct usb_hcd *hcd = NULL;
	struct platform_device *pdev = to_platform_device(dev->dev.parent);
	int req = -1;
	int ret;

	if (usb_disabled()) {
		DDPRINTK("%s: USB is disabled\n", __FUNCTION__);
		return -ENODEV;
	}

	DDPRINTK("%s: Got platform dev for %s @ %08x with %d resources\n", __FUNCTION__,
		pdev->name, (u32)pdev, pdev->num_resources);
	if (pdev->num_resources < 2) {
		return -ENODEV;
	}

	if (pdev->num_resources > 2) {
		dev->dma_addr = pdev->resource[1].start;
	} else {
		dev->dma_addr = pdev->resource[0].start + res_len(&pdev->resource[0] - 1);
	}

	for (req = 0; req < pdev->num_resources - 1; req++) {
		struct resource *res = &pdev->resource[req];

		DDPRINTK("%s: Requesting memory region %08lx..%08lx\n", __FUNCTION__,
			 res->start, res->start + res_len(res) - 1);
		if (!request_mem_region(res->start, res_len(res), hcd_name)) {
			DDPRINTK("%s: request_mem_region %08lx:%08lx failed\n", __FUNCTION__,
				 res->start, res->start + res_len(res) - 1);
			ret = -EBUSY;
			goto out;
		}
	}
	dev->irq = pdev->resource[req].start;

	DDPRINTK("%s: Remapping memory region %08lx\n", __FUNCTION__, pdev->resource[0].start);
	dev->addr_reg = ioremap(pdev->resource[0].start, res_len(&pdev->resource[0]));
	if (dev->addr_reg == NULL) {
		DDPRINTK("%s: Remapping memory region %08lx failed\n", __FUNCTION__,
			 pdev->resource[0].start);
		ret = -ENOMEM;
		goto out;
	}

	if (pdev->num_resources > 2) {
		DDPRINTK("%s: Remapping memory region %08lx\n", __FUNCTION__, pdev->resource[1].start);
		dev->data_reg = ioremap(pdev->resource[1].start, res_len(&pdev->resource[1]));
		if (dev->data_reg == NULL) {
			DDPRINTK("%s: Remapping memory region %08lx failed\n", __FUNCTION__,
				 pdev->resource[1].start);
			ret = -ENOMEM;
			goto out;
		}
	} else {
		dev->data_reg = dev->addr_reg + res_len(&pdev->resource[0]);
	}

	DDPRINTK("%s: Probing for HC_SL811 @ %08x\n", __FUNCTION__, (u32)&ohci_sl811_hc_driver);
	ret = usb_hcd_sl811_probe(&ohci_sl811_hc_driver, &hcd, dev);

	DDPRINTK("%s: usb_hcd_sl811_probe returned %d\n", __FUNCTION__, ret);
	if (ret == 0) {
		hc_sl811_set_drvdata(dev, hcd);
	}

	return 0;

 out:
	if (dev->addr_reg) {
		DDPRINTK("%s: Unmapping memory @ %08x\n", __FUNCTION__, (u32)dev->addr_reg);
		iounmap(dev->addr_reg);
		dev->addr_reg = NULL;
		if ((pdev->num_resources > 2) && (dev->data_reg)) {
			DDPRINTK("%s: Unmapping memory @ %08x\n", __FUNCTION__, (u32)dev->data_reg);
			iounmap(dev->data_reg);
			dev->data_reg = NULL;
		}
	}
	for (; req >= 0; req--) {
		struct resource *res = &pdev->resource[req];

		DDPRINTK("%s: Releasing memory region %08lx..%08lx\n", __FUNCTION__,
			 res->start, res->start + res_len(res) - 1);
		release_mem_region(res->start, res_len(res));
	}

	return ret;
}

static int ohci_hcd_sl811_drv_remove(struct hc_sl811_dev *dev)
{
	struct usb_hcd *hcd = hc_sl811_get_drvdata(dev);
	struct platform_device *pdev = to_platform_device(dev->dev.parent);
	int i;

	DDPRINTK("%s: Removing HC-SL811 driver for hc_sl811_dev @ %08x\n", __FUNCTION__, (u32)dev);
	usb_hcd_sl811_remove(hcd, dev);

	hc_sl811_set_drvdata(dev, NULL);

	DDPRINTK("%s: Unmapping memory @ %08x\n", __FUNCTION__, (u32)dev->addr_reg);
	iounmap(dev->addr_reg);

	if (pdev->num_resources > 2) {
		DDPRINTK("%s: Unmapping memory @ %08x\n", __FUNCTION__, (u32)dev->data_reg);
		iounmap(dev->data_reg);
	}

	for (i = 0; i < pdev->num_resources - 1; i++) {
		struct resource *res = &pdev->resource[i];

		DDPRINTK("%s: Releasing memory region %08lx..%08lx\n", __FUNCTION__,
			res->start, res->start + res_len(res) - 1);
		release_mem_region(res->start, res_len(res));
	}

	return 0;
}
#if 0
static void hcd_sl811_release(struct usb_bus *bus)
{
	struct usb_hcd *hcd = bus->hcpriv;

	DDPRINTK("%s: bus=%08x\n", __FUNCTION__, (u32)bus);
	if (hcd != NULL) {
		DDPRINTK("%s: Freeing hcd @ %08x\n", __FUNCTION__, (u32)hcd);
		hcd->driver->hcd_free(hcd);
	}
}
#endif

static void __sl811_release(struct device *dev)
{
	struct hc_sl811_dev *sl811_dev = dev_get_drvdata(dev);

	DDPRINTK("%s: dev=%08x\n", __FUNCTION__, (u32)dev);
	if (sl811_dev != NULL) {
		DDPRINTK("%s: Freeing hc_sl811_dev @ %08x\n", __FUNCTION__, (u32)sl811_dev);
#ifdef CONSISTENT_ALLOC
		consistent_free(sl811_dev->dma_buf, SL811_MAP_SIZE, sl811_dev->dma_bus_addr);
#endif
		kfree(sl811_dev);
	}
}

static int __sl811_probe(struct device *dev, struct resource *mem, struct resource *irq)
{
	int ret;
	struct hc_sl811_dev *sl811_dev;

	sl811_dev = kmalloc(sizeof(struct hc_sl811_dev), GFP_KERNEL);
	if (sl811_dev == NULL) {
		return -ENOMEM;
	}
	memzero(sl811_dev, sizeof(struct hc_sl811_dev));
	DDPRINTK("%s: sl811_dev allocated @ %08x\n", __FUNCTION__, (u32)sl811_dev);

	sl811_dev->dma = -1;
	sl811_dev->irq = NO_IRQ;

	sl811_dev->dev.parent = dev;
	sl811_dev->dev.bus = &platform_bus_type;
	sl811_dev->dev.release = __sl811_release;
	sl811_dev->res.start = mem->start;
	sl811_dev->res.end = mem->end;
	sl811_dev->res.name = sl811_dev->dev.bus_id;
	sl811_dev->res.flags = IORESOURCE_MEM;

	dev_set_drvdata(dev, sl811_dev);

	ret = ohci_hcd_sl811_drv_probe(sl811_dev);

	if (ret != 0) {
		dev_set_drvdata(dev, NULL);
	}

	if (machine_is_netbookpro()) {
		printk("NetbookPro: enabling USB Power\n");
		nbp_pcon_command(I2C_SETUSBHOSTPOWER, 1, PCON_FLG_ASYNC, NULL);
		printk("USB Power Enabled\n");
	}

	return ret;
}

static struct hc_sl811_driver ohci_hcd_sl811_driver = {
	.drv = {
		.name	= "sl811-ohci",
		.bus	= &platform_bus_type,
	},
	.devid		= 0,
#if 0
	.probe	= ohci_hcd_sl811_drv_probe,
	.remove	= usb_hcd_sl811_remove,
#else
	.probe	= ohci_hcd_sl811_drv_probe,
	.remove	= ohci_hcd_sl811_drv_remove,
#endif
#ifdef CONFIG_PM
	.suspend = NULL,
	.resume = NULL,
#endif
};

static int sl811_bus_probe(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct resource *mem, *irq = NULL;
	int ret = -ENODEV;

	DPRINTK("%s: platform_device=%08x\n", __FUNCTION__, (u32)pdev);

	mem = &pdev->resource[0];
	if (pdev->num_resources == 2) {
		irq = &pdev->resource[1];
	}
	ret = __sl811_probe(dev, mem, irq);

	return ret;
}

static int sl811_bus_remove(struct device *dev)
{
	int ret = 0;
	struct hc_sl811_dev *sl811_dev = dev_get_drvdata(dev);
	struct hc_sl811_driver *drv = SL811_DRV(dev->driver);

	DDPRINTK("%s: removing driver %08x\n", __FUNCTION__, (u32)drv);
	if (drv->remove) {
		DPRINTK("%s: Removing driver @ %08x from dev %08x\n", __FUNCTION__, (u32)drv,
			(u32)sl811_dev);
		ret = drv->remove(sl811_dev);
	}
	return ret;
}

static int hc_sl811_driver_register(struct hc_sl811_driver *driver)
{
	int ret;

	WARN_ON(driver->drv.suspend || driver->drv.resume || driver->drv.probe || driver->drv.remove);
	DDPRINTK("%s: Registering hc_sl811_driver @ %08x\n", __FUNCTION__, (u32)driver);
	driver->drv.probe = sl811_bus_probe;
	driver->drv.remove = sl811_bus_remove;

	DDPRINTK("%s: Registering platform bus driver @ %08x\n", __FUNCTION__, (u32)&driver->drv);
	ret = driver_register(&driver->drv);
	DDPRINTK("%s: driver_register returned %d\n", __FUNCTION__, ret);

	return ret;
}

static void hc_sl811_driver_unregister(struct hc_sl811_driver *driver)
{
	DDPRINTK("%s: Unregistering hc_sl811_driver @ %08x\n", __FUNCTION__, (u32)driver);
	driver_unregister(&driver->drv);
}

static int __init ohci_hcd_sl811_init(void)
{
	int ret = 0;

#if DEBUG_BUF_SIZE > 0
	debug_buffer = kmalloc(DEBUG_BUF_SIZE * sizeof(struct debug_buffer), GFP_KERNEL);
#endif
	printk(DRIVER_INFO " (HC-SL811)\n");

	DDPRINTK("%s: Registering hc_driver @ %08x\n", __FUNCTION__,
		(u32)&ohci_hcd_sl811_driver);
	ret = hc_sl811_driver_register(&ohci_hcd_sl811_driver);
	DDPRINTK("%s: driver_register returned %d\n", __FUNCTION__, ret);

	return ret;
}

static void __exit ohci_hcd_sl811_cleanup(void)
{
#if DEBUG_BUF_SIZE > 0
	print_debug_buffer();
	kfree(debug_buffer);
	debug_buffer = NULL;
#endif
	DDPRINTK("%s: Unregistering hc_driver @ %08x\n", __FUNCTION__,
		(u32)&ohci_hcd_sl811_driver);
	hc_sl811_driver_unregister(&ohci_hcd_sl811_driver);
}

module_init(ohci_hcd_sl811_init);
module_exit(ohci_hcd_sl811_cleanup);
