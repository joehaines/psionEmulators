/*
 * drivers/usb/host/ohci-regs.h
 *
 * Macros for OHCI register access to facilitate OHCI emulation
 * on non-OHCI compliant hardware
 *
 */

#ifdef CONFIG_PCI
#define ohci_read_reg(hc,n)			readl(&hc->regs->n)
#define ohci_read_control(hc)			ohci_read_reg(hc,control)
#define ohci_read_cmdstatus(hc)			ohci_read_reg(hc,cmdstatus)
#define ohci_read_intstatus(hc)			readl(&hc->regs->intrstatus)
#define ohci_read_intrenable(hc)		readl(&hc->regs->intrenable)
#define ohci_read_ed_bulkhead(hc)		ohci_read_reg(hc,ed_bulkhead)
#define ohci_read_ed_bulkcurrent(hc)		ohci_read_reg(hc,ed_bulkcurrent)
#define ohci_read_ed_controlhead(hc)		ohci_read_reg(hc,ed_controlhead)
#define ohci_read_ed_controlcurrent(hc)		ohci_read_reg(hc,ed_controlcurrent)
#define ohci_read_ed_periodcurrent(hc)		ohci_read_reg(hc,ed_periodcurrent)
#define ohci_read_ed_(hc)			ohci_read_reg(hc,ed_)
#define ohci_write_control(v,hc)		writel(v, &hc->regs->control)
#define ohci_write_cmdstatus(cmd,hc)		writel(cmd, &hc->regs->cmdstatus)
#define ohci_write_intrstatus(mask,hc)		writel(mask, &hc->regs->intrstatus)
#define ohci_write_intrenable(mask,hc)		writel(mask, &hc->regs->intrenable)
#define ohci_write_intrdisable(mask,hc)		writel(mask, &hc->regs->intrdisable)
#define ohci_clr_ed_controlhead(hc)		writel(0, &hc->regs->ed_controlhead)
#define ohci_write_ed_controlhead(hc,ed)	writel(ed->dma, &hc->regs->ed_controlhead)
#define ohci_write_ed_controlhead_next(hc,ed)	writel(le32_to_cpup(&ed->hwNextED), &hc->regs->ed_controlhead)
#define ohci_clr_ed_bulkhead(hc)		writel(0, &hc->regs->ed_bulkhead)
#define ohci_write_ed_bulkhead(hc,ed)		writel(ed->dma, &hc->regs->ed_bulkhead)
#define ohci_write_ed_bulkhead_next(hc,ed)	writel(le32_to_cpup(&ed->hwNextED), &hc->regs->ed_bulkhead)

#else
#include "ohci-sl811.h"

#define hcd_to_sl811_dev(hcd)		((struct hc_sl811_dev *)dev_get_drvdata(hcd->self.controller))
#define ohci_to_sl811_dev(ohci)		((struct hc_sl811_dev *)dev_get_drvdata(ohci->hcd.self.controller))

/*
 * OHCI emulation
 */
static void sl811_reset_hc(struct usb_hcd *hcd, int cold);
//static void ohci_sl811_do_port_reset(struct ohci_hcd *ohci, int port);

#define ohci_sl811_update_reg(hc,n,s,c)				\
{								\
	u32 *reg = &ohci->regs->n;				\
	DPRINTK("%s: Updating %s @ %08x: %08x->%08x\n",		\
		__FUNCTION__, #n, (u32)reg,			\
		*reg, (*reg & ~(c)) | (s));			\
								\
	*reg = (*reg & ~(c)) | (s);				\
}

#define ohci_sl811_update_control(hc,s,c) ohci_sl811_update_reg(hc,control,s,c);
#define ohci_sl811_update_cmdstatus(hc,s,c) ohci_sl811_update_reg(hc,cmdstatus,s,c);
#define ohci_sl811_update_intrstatus(hc,s,c) ohci_sl811_update_reg(hc,intrstatus,s,c);
#define ohci_sl811_update_ed_controlhead(hc,s,c) ohci_sl811_update_reg(hc,ed_controlhead,s,c);
#define ohci_sl811_update_ed_controlcurrent(hc,s,c) ohci_sl811_update_reg(hc,ed_controlcurrent,s,c);
#define ohci_sl811_update_ed_bulkhead(hc,s,c) ohci_sl811_update_reg(hc,ed_bulkhead,s,c);
#define ohci_sl811_update_ed_bulkcurrent(hc,s,c) ohci_sl811_update_reg(hc,ed_bulkcurrent,s,c);

static void ohci_sl811_update_portstatus(struct ohci_hcd *ohci, int port, u32 set, u32 clr)
{
	unsigned long flags;
	u32 stat;
	u32 new;
	u32 chg;

	if (port < 0 || port >= MAX_ROOT_PORTS) {
		printk("%s: Invalid port number %d\n", __FUNCTION__, port);
		return;
	}
	local_irq_save(flags);
	stat = ohci->regs->roothub.portstatus[port];
	new = ((stat & 0xffff) | set) & ~clr;
	chg = (stat ^ new) & 0xffff;
	DPRINTK("%s: old: %08x new: %08x chg: %04x\n", __FUNCTION__, stat, new, chg);
	if (chg) {
		chg |= stat >> 16;
		DDPRINTK("%s: Changed port_status from %08x to: %08x\n", __FUNCTION__,
			ohci->regs->roothub.portstatus[port], new | (chg << 16));
		ohci->regs->roothub.portstatus[port] = new | (chg << 16);
		ohci->regs->intrstatus |= OHCI_INTR_RHSC;
	} else {
		DPRINTK("%s: Leaving portstatus unchanged: %08x\n", __FUNCTION__,
			ohci->regs->roothub.portstatus[port]);
	}
	local_irq_restore(flags);
}

/*
 * Software API
 */
#define ohci_read_reg(hc,n)						\
({									\
	u32 *reg = &hc->regs->n;					\
	u32 val;							\
									\
	val = *reg;							\
	DPRINTK("%s: Read %08x from %s reg @ %08x\n", __FUNCTION__,	\
		val, #n, (u32)reg);					\
	val;								\
})

#define ohci_sl811_wake_thread()						\
	if (changed) {								\
		if (waitqueue_active(&dev->task_wait)) {			\
			DDPRINTK("%s: Waking up kernel thread\n", __FUNCTION__);	\
			wake_up_interruptible(&dev->task_wait);			\
		} else {							\
			DDPRINTK("%s: Waitqueue is not active\n", __FUNCTION__);	\
		}								\
	}

#define _ohci_write_reg(hc,n,v)						\
({									\
	u32 *reg = &ohci->regs->n;					\
									\
	DPRINTK("%s: Writing %08x to %s reg @ %08x\n", __FUNCTION__,	\
		v, #n, (u32)reg);					\
	*reg = v;							\
})

#define ohci_write_reg(hc,n,v)						\
({									\
	unsigned long flags;						\
	u32 *reg = &ohci->regs->n;					\
									\
	DPRINTK("%s: Writing %08x to %s reg @ %08x\n", __FUNCTION__,	\
		v, #n, (u32)reg);					\
	local_irq_save(flags);						\
	*reg = v;							\
	local_irq_restore(flags);					\
})

#define ohci_read_roothub_reg(hc,n)					\
	ohci_read_reg(hc,roothub.n)

#define ohci_write_roothub_reg(hc,n,v)						\
({										\
	unsigned long flags;							\
	u32 *reg = &ohci->regs->roothub.n;					\
										\
	DPRINTK("%s: Writing %08x to roothub.%s reg @ %08x\n", __FUNCTION__,	\
		v, #n, (u32)reg);						\
	local_irq_save(flags);							\
	*reg = v;								\
	local_irq_restore(flags);						\
})

#define ohci_read_control(hc)	ohci_read_reg(hc,control)
#define ohci_read_cmdstatus(hc)	ohci_read_reg(hc,cmdstatus)

#define ohci_read_roothub_a(hc) ohci_read_roothub_reg(hc,a)
#define ohci_read_roothub_b(hc) ohci_read_roothub_reg(hc,b)
//#define ohci_read_roothub_status(hc) ohci_read_roothub_reg(hc,status)

#define ohci_write_roothub_a(hc,v) ohci_write_roothub_reg(hc,a,v)
#define ohci_write_roothub_b(hc,v) ohci_write_roothub_reg(hc,b,v)

static inline void ohci_write_control(struct ohci_hcd *ohci, u32 ctrl)
{
	unsigned long flags;
	struct usb_hcd *hcd = &ohci->hcd;
	struct hc_sl811_dev *dev = hcd_to_sl811_dev(hcd);
	int old_state;
	int new_state = ctrl & OHCI_CTRL_HCFS;

	local_irq_save(flags);
	old_state = ohci->regs->control & OHCI_CTRL_HCFS;
	if (old_state != new_state) {
		DDPRINTK("%s: Changing state from %08x to %08x\n", __FUNCTION__, old_state, new_state);
		switch (new_state) {
		case OHCI_USB_RESET:
			DDPRINTK("%s: Asserting USB reset\n", __FUNCTION__);
			hc_sl811_usb_reset(hcd, 1);
			break;
		case OHCI_USB_RESUME:
			if (old_state == OHCI_USB_SUSPEND) {
				DDPRINTK("%s: Deactivating SUSPEND mode\n", __FUNCTION__);
				hc_sl811_set_mask(dev, SL11H_CTLREG1, SL11H_CTL1MASK_SOFENA);
			}
			break;
		case OHCI_USB_SUSPEND:
			if (old_state == OHCI_USB_OPER) {
				DDPRINTK("%s: Activating SUSPEND mode\n", __FUNCTION__);
				hc_sl811_clr_mask(dev, SL11H_CTLREG1, SL11H_CTL1MASK_SOFENA);
			}
			break;
		case OHCI_USB_OPER:
			switch (old_state) {
			case OHCI_USB_RESET:
				DDPRINTK("%s: Deasserting USB reset\n", __FUNCTION__);
				hc_sl811_usb_reset(hcd, 0);
				break;
			case OHCI_USB_RESUME:
				DDPRINTK("%s: Disabling USB Resume Interrupt\n", __FUNCTION__);
				hc_sl811_clr_mask(dev, SL11H_INTENBLREG, SL11H_INTMASK_USBRESET);
				hc_sl811_clr_mask(dev, SL11H_CTLREG1, SL11H_CTL1MASK_SUSPEND);
				break;
			case OHCI_USB_SUSPEND:
				hc_sl811_set_mask(dev, SL11H_CTLREG1, SL11H_CTL1MASK_SUSPEND);
				DDPRINTK("%s: Enabling USB Resume Interrupt\n", __FUNCTION__);
				hc_sl811_set_mask(dev, SL11H_INTENBLREG, SL11H_INTMASK_USBRESET);
				break;
			}
			break;
		}
	}
	ohci->regs->control = ctrl;
	local_irq_restore(flags);
}

static inline void ohci_write_cmdstatus(struct ohci_hcd *ohci, u32 cmd)
{
	unsigned long flags;
	struct usb_hcd *hcd = &ohci->hcd;

	if (cmd & OHCI_HCR) {
		sl811_reset_hc(hcd, 0);
		cmd &= ~OHCI_HCR;
	}

	local_irq_save(flags);
	if ((ohci->regs->ed_controlcurrent == 0) && (cmd & OHCI_CLF)) {
		DPRINTK("%s: Got CLF command\n", __FUNCTION__);

		ohci->regs->ed_controlcurrent = ohci->regs->ed_controlhead;
	}
	if ((ohci->regs->ed_bulkcurrent == 0) && (cmd & OHCI_BLF)) {
		DPRINTK("%s: Got BLF command\n", __FUNCTION__);

		ohci->regs->ed_bulkcurrent = ohci->regs->ed_bulkhead;
	}
	ohci->regs->cmdstatus = cmd;
	local_irq_restore(flags);
}

#if 0
static inline u32 ohci_read_roothub_a(struct ohci_hcd *ohci)
{
	u32 status = ohci->regs->roothub.a;
	return status;
}

static inline u32 ohci_read_roothub_b(struct ohci_hcd *ohci)
{
	u32 status = ohci->regs->roothub.b;
	return status;
}
#endif

static inline u32 ohci_read_roothub_status(struct ohci_hcd *ohci)
{
	int oci = 0;
	u32 status = ohci->regs->roothub.status;

#ifdef CONFIG_ARCH_KARO_
	oci = karo_pcf8574_tst(FMsk(KARO_PCF8574_USBOVL));
	DDPRINTK("%s: Read %08x from PCF8574\n", __FUNCTION__, oci);

	if (oci < 0) {
		DDPRINTK("%s: Failed to read Overcurrent status from PCF8574: %d\n", __FUNCTION__,
			 oci);
	} else {
		oci = !oci;
	}
#endif
	if ((status & RH_HS_OCI) ^ ((oci > 0) ? RH_HS_OCI : 0)) {
		status |= RH_HS_OCIC;
		status = (status & ~RH_HS_OCI) | ((oci > 0) ? RH_HS_OCI : 0);
	}

	return status;
}

static inline void ohci_write_roothub_status(struct ohci_hcd *ohci, u32 mask)
{
	struct usb_hcd *hcd = ohci_to_hcd(ohci);
	struct hc_sl811_dev *dev = hcd_to_sl811_dev(hcd);
	unsigned long flags;
	u32 *reg = &ohci->regs->roothub.status;
	u32 status = 0;

	DPRINTK("%s: Writing %08x to HcRhStatus\n", __FUNCTION__, mask);
	local_irq_save(flags);
	if (mask & RH_HS_LPS) {
		DPRINTK("%s: Power control not supported on SL811\n", __FUNCTION__);
	}
	if (mask & RH_HS_DRWE) {
		status |= RH_HS_DRWE;
		hc_sl811_set_mask(dev, SL11H_CTLREG1, SL11H_CTL1MASK_SUSPEND);
	}
	if (mask & RH_HS_CRWE) {
		status &= ~RH_HS_DRWE;
		hc_sl811_clr_mask(dev, SL11H_CTLREG1, SL11H_CTL1MASK_SUSPEND);
	}
	if (mask & RH_HS_LPSC) {
		DPRINTK("%s: Power control not supported on SL811\n", __FUNCTION__);
		status &= ~RH_HS_LPSC;
	}
	if (mask & RH_HS_OCIC) {
		status &= ~RH_HS_OCIC;
	}

	if (*reg != status) {
		ohci->regs->intrstatus |= OHCI_INTR_RHSC;
	}
	*reg = status;

	local_irq_restore(flags);
}

static inline u32 ohci_read_roothub_portstatus(struct ohci_hcd *ohci, int port)
{
	struct ohci_roothub_regs *rh_regs = &ohci->regs->roothub;
	u32 status = RH_PS_PPS;

	if (port < 0 || port >= MAX_ROOT_PORTS) {
		DPRINTK("%s: Invalid port number %d\n", __FUNCTION__, port);
		return -1;
	}
	status |= rh_regs->portstatus[port];

	return status;
}

static inline void ohci_write_roothub_portstatus(struct ohci_hcd *ohci, int port, u32 mask)
{
	unsigned long flags;
	struct ohci_roothub_regs *rh_regs = &ohci->regs->roothub;
	u32 status;

	if (port < 0 || port >= MAX_ROOT_PORTS) {
		printk("%s: Invalid port number %d\n", __FUNCTION__, port);
		return;
	}
#ifdef DEBUG
	status = rh_regs->portstatus[port];
	DDPRINTK("%s: Writing %08x to portstatus[%d]: %08x\n", __FUNCTION__, mask, port, status);
#endif
	local_irq_save(flags);
	status = rh_regs->portstatus[port];
	if (mask & RH_PS_PESC) {
		status &= ~RH_PS_PESC;
	}
	if (mask & RH_PS_PSSC) {
		status &= ~RH_PS_PSSC;
	}
	if (mask & RH_PS_OCIC) {
		status &= ~RH_PS_OCIC;
	}
	if (mask & RH_PS_PRSC) {
		status &= ~RH_PS_PRSC;
	}
	if (mask & RH_PS_CSC) {
		status &= ~RH_PS_CSC;
	}
	if (status & RH_PS_CCS) {
		if (mask & RH_PS_CCS) {
			// clear PES, if device is removable
			if (rh_regs->b & (1 << port)) {
				status &= ~RH_PS_PES;
			}
		}
		if (mask & RH_PS_PES) {
			status |= RH_PS_PES;
		}
		if (mask & RH_PS_PSS) {
			status |= RH_PS_PSS;
		}
		if (mask & RH_PS_POCI) {
			status &= ~RH_PS_PSS;
		}
		if (mask & RH_PS_PRS) {
			status |= RH_PS_PRS;
		}
		if (mask & RH_PS_PPS) {
			// PPS enabled: set PPS
		}
		if (mask & RH_PS_LSDA) {
			// PPS enabled: clear PPS
		}
	} else {
		if (mask & (RH_PS_PRS | RH_PS_PES | RH_PS_PSS)) {
			status |= RH_PS_CSC;
		}
	}
	if (status != rh_regs->portstatus[port]) {
		DDPRINTK("%s: status of port %d changed from %08x to %08x\n", __FUNCTION__,
			port, rh_regs->portstatus[port], status);
	}
	rh_regs->portstatus[port] = status;

	if (status & RH_PS_PRS) {
		hc_sl811_usb_reset(ohci_to_hcd(ohci), -1);
	}
	local_irq_restore(flags);
}

static inline void ohci_write_intrstatus(struct ohci_hcd *ohci, u32 ohci_mask)
{
	unsigned long flags;

	DPRINTK("%s: Acknowledging Interrupts: %08x\n", __FUNCTION__, ohci_mask);
	local_irq_save(flags);
	ohci->regs->intrstatus &= ~ohci_mask;
	local_irq_restore(flags);
}

static inline void ohci_write_intrenable(struct ohci_hcd *ohci, u32 ohci_mask)
{
	struct usb_hcd *hcd = ohci_to_hcd(ohci);
	struct hc_sl811_dev *dev = hcd_to_sl811_dev(hcd);
	unsigned long flags;
	u32 chg = ohci->regs->intrenable ^ ohci_mask;

	DPRINTK("%s: Enabling OHCI Interrupts: %08x\n", __FUNCTION__, ohci_mask);

	local_irq_save(flags);
	ohci->regs->intrenable |= ohci_mask;
	if (chg & OHCI_INTR_MIE) {
		if (ohci->regs->intrenable & OHCI_INTR_MIE) {
			DPRINTK("%s: Enabling SL811 SOF interrupts\n", __FUNCTION__);
			hc_sl811_set_mask(dev, SL11H_INTENBLREG, SL11H_INTMASK_SOFINTR);
		} else {
			DPRINTK("%s: Disabling SL811 SOF interrupts\n", __FUNCTION__);
			hc_sl811_clr_mask(dev, SL11H_INTENBLREG, SL11H_INTMASK_SOFINTR);
		}
	}
	local_irq_restore(flags);
}

static inline void ohci_write_intrdisable(struct ohci_hcd *ohci, u32 ohci_mask)
{
	unsigned long flags;

	DPRINTK("%s: Disabling OHCI Interrupts: %08x\n", __FUNCTION__, ohci_mask);

	local_irq_save(flags);
	ohci->regs->intrenable &= ~ohci_mask;
	local_irq_restore(flags);
}

static inline u32 ohci_read_intrstatus(struct ohci_hcd *ohci)
{
	u32 ohci_status = ohci->regs->intrstatus;

	DPRINTK("%s: Int status: %02x\n", __FUNCTION__, ohci_status);

	return ohci_status;
}

static inline u32 ohci_read_intrenable(struct ohci_hcd *ohci)
{
	u32 ohci_int_mask = ohci->regs->intrenable;

	return ohci_int_mask;
}
#define ohci_read_intrdisable ohci_read_intrenable

static inline void ohci_clr_ed_controlhead(struct ohci_hcd *ohci)
{
	_ohci_write_reg(ohci, ed_controlhead, 0);
}

static inline void ohci_clr_ed_controlcurrent(struct ohci_hcd *ohci)
{
	_ohci_write_reg(ohci, ed_controlcurrent, 0);
}

static inline void ohci_write_ed_controlhead(struct ohci_hcd *ohci, struct ed *ed)
{
	_ohci_write_reg(ohci, ed_controlhead, (u32)ed);
}

static inline void ohci_write_ed_controlhead_next(struct ohci_hcd *ohci, struct ed *ed)
{
	_ohci_write_reg(ohci, ed_controlhead, (u32)ed->ed_next);
}

static inline void ohci_clr_ed_bulkhead(struct ohci_hcd *ohci)
{
	_ohci_write_reg(ohci, ed_bulkhead, 0);
}

static inline void ohci_clr_ed_bulkcurrent(struct ohci_hcd *ohci)
{
	_ohci_write_reg(ohci, ed_bulkcurrent, 0);
}

static inline void ohci_write_ed_bulkhead(struct ohci_hcd *ohci, struct ed *ed)
{
	_ohci_write_reg(ohci, ed_bulkhead, (u32)ed);
}

static inline void ohci_write_ed_bulkhead_next(struct ohci_hcd *ohci, struct ed *ed)
{
	_ohci_write_reg(ohci, ed_bulkhead, (u32)ed->ed_next);
}
#endif
