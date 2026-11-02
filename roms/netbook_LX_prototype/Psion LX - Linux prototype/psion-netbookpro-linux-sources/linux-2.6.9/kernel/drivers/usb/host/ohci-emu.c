#include <asm/arch/dma.h>

#define HcControl		(ohci->regs->control)
#define HcCmdStatus		(ohci->regs->cmdstatus)
#define HcFmRemaining		(ohci->regs->fmremaining)
#define HcFmNumber		(ohci->regs->fmnumber)
#define HcFmInterval		(ohci->regs->fminterval)
#define HcPeriodicStart		(ohci->regs->periodicstart)
#define HcLsThreshold		(ohci->regs->lsthresh)
#define HcRhStatus		(ohci->regs->roothub.status)
#define HcRhPortStatus		(ohci->regs->roothub.portstatus)
#define HcIntrEnable		(ohci->regs->intrenable)
#define HcIntrStatus		(ohci->regs->intrstatus)
#define HcControlHeadED		(ohci->regs->ed_controlhead)
#define HcBulkHeadED		(ohci->regs->ed_bulkhead)
#define HcDoneHead		(ohci->regs->donehead)
#define HcControlCurrentED	(ohci->regs->ed_controlcurrent)
#define HcBulkCurrentED		(ohci->regs->ed_bulkcurrent)
#define HcPeriodCurrentED	(ohci->regs->ed_periodcurrent)
#define HccaDoneHead		(ohci->hcca->done_head)
#define HccaFrameNumber		(ohci->hcca->frame_no)
#define HccaInterruptTable	(ohci->hcca->int_table)

static inline void hc_sl811_write_regs(struct hc_sl811_dev *dev, u8 regno, const void *buf, u8 count)
{
	int i;
	const u8 *data = buf;

	HC_SL811_WRITE_ADDR(dev, regno);
	for (i = 0; i < count; i++) {
		HC_SL811_WRITE_DATA(dev, data[i]);
	}
}

static inline void hc_sl811_read_regs(struct hc_sl811_dev *dev, u8 regno, void *buf, u8 count)
{
	int i;
	u8 *data = buf;

	HC_SL811_WRITE_ADDR(dev, regno);
	for (i = 0; i < count; i++) {
		data[i] = HC_SL811_READ_DATA(dev);
	}
}

#if 0
static inline u32 ohci_read_fm_remaining(struct ohci_hcd *ohci)
{
	u32 val;
	struct usb_hcd *hcd = ohci_to_hcd(ohci);
	struct hc_sl811_dev *dev = hcd_to_sl811_dev(hcd);

	val = hc_sl811_read_reg(dev, SL11H_CTLREG2) * 64;

	return val;
}
#endif

static inline u32 TD_BUF_LEN(struct td *td)
{
	u32 len = 0;
	u32 cbp = td->hwCBP;

	if (cbp != 0) {
		u32 be = td->hwBE;

		if (unlikely((cbp ^ be) & PAGE_MASK)) {
			// CBP and BE in different pages
			// length till end of first page  +  length in second page
			len = (PAGE_SIZE - (cbp & ~PAGE_MASK)) + (be & ~PAGE_MASK) + 1;
			DPRINTK("%s: CBP=%08x, BE=%08x, len=%08x\n", __FUNCTION__,
				cbp, be, len);
			print_debug_buffer();
		} else {
			len = be - cbp + 1;
		}
	}

	return len;
}

#if DEBUG_BUF_SIZE > 0
static inline int TD_BUF_OFFSET(struct td *td)
{
	int offset = 0;
	u32 cbp = td->hwCBP;
	u32 be = td->hwBE;

	if (cbp != 0) {
		if (unlikely((cbp ^ td->data_dma) & PAGE_MASK)) {
			offset = PAGE_SIZE + (be & ~PAGE_MASK);
			DPRINTK("%s: cbp=%08x, be=%08x, td->data_dma=%08x, offset=%08x\n",
				 __FUNCTION__, cbp, be, td->data_dma, offset);
			print_debug_buffer();
		} else {
			offset = cbp - td->data_dma;
		}
	}

	return offset;
}
#endif

#define ED_MPS		(((1 << 10) - 1) << 16)
#define ed_pid(info)	(((info) & (ED_IN | ED_OUT)) >> 11)
#define td_pid(info)	(((info) & TD_DP) >> 19)
#define ed_mps(i)	(((i) & ED_MPS) >> 16)
#define ed_fa(info)	((info) & ((1 << 7) - 1))
#define ed_en(info)	(((info) >> 7) & ((1 << 4) - 1))

static const u8 ed_td_to_pid[4][4] = {
	{ PID_SETUP, PID_OUT, PID_IN, 0 },	// PID from TD
	{ PID_OUT, PID_OUT, PID_OUT, PID_OUT },	// PID from ED
	{ PID_IN, PID_IN, PID_IN, PID_IN },	// PID from ED
	{ PID_SETUP, PID_OUT, PID_IN, 0 },	// PID from TD
};

static inline u32 DMA_XFER_SIZE(u32 addr, u32 len)
{
	u32 count = len;

	if (unlikely(((addr & ~PAGE_MASK) + len) > PAGE_SIZE)) {
		count = PAGE_ALIGN(addr + len) - addr;
		DPRINTK("%s: addr: %08x, len: %08x count: %08x\n", __FUNCTION__,
			 addr, len, count);
		print_debug_buffer();
	}

	return count;
}

#define DO_DMA(ch,len)						\
	DCMD(ch) = DMA_FLAGS | (DCMD_LENGTH & len);		\
	DCSR(ch) |= DCSR_RUN;					\
	while (!(DCSR(ch) & DCSR_STOPSTATE));			\
	DCSR(ch) = DCSR_NODESC

#ifdef USE_DMA
static inline void sl811_dma_recv_data(struct usb_hcd *hcd, struct td *td,
				       struct hc_sl811_dev *dev, int count)
{
	const unsigned int DMA_FLAGS = DCMD_INCTRGADDR | DCMD_WIDTH1 | DCMD_BURST32;
	u32 dma_buf = td->hwCBP;
	int align = dma_buf & 7;
	int dma_ch = dev->dma;
	int len = count;

	WARN_ON(dma_buf == 0);

	if (dma_ch < 0) {
		printk(KERN_ERR "%s: DMA unavailable\n", __FUNCTION__);
		return;
	}

	DCSR(dma_ch) = DCSR_NODESC;

	HC_SL811_WRITE_ADDR(dev, SL11H_DATA_START);
	while (len > 0) {
		u32 dma_len = DMA_XFER_SIZE(dma_buf, len);

		DSADR(dma_ch) = dev->dma_addr;
		DTADR(dma_ch) = dma_buf; // FIXME: work around DMA buffer alignment issue

		DO_DMA(dma_ch, dma_len);

		len -= dma_len;
		if (likely(len == 0)) {
			break;
		} else {
			dma_buf = td->hwBE & ~PAGE_MASK;
		}
	}
	dma_sync_single(hcd->self.controller, dma_buf, count, DMA_FROM_DEVICE);
}

static inline void sl811_dma_send_data(struct usb_hcd *hcd, struct td *td,
				       struct hc_sl811_dev *dev, int count)
{
	const unsigned int DMA_FLAGS = DCMD_INCSRCADDR | DCMD_WIDTH1 | DCMD_BURST32;
	u32 dma_buf = td->hwCBP;
	int align = dma_buf & 7;
	int dma_ch = dev->dma;
	int len = count;

	if (dma_ch < 0) {
		printk(KERN_ERR "%s: DMA unavailable\n", __FUNCTION__);
		return;
	}

	dma_sync_single(hcd->self.controller, dma_buf, count, DMA_TO_DEVICE);
	DCSR(dma_ch) = DCSR_NODESC;

	HC_SL811_WRITE_ADDR(dev, SL11H_DATA_START);
	while (len > 0) {
		u32 dma_len = DMA_XFER_SIZE(dma_buf, len);

		DSADR(dma_ch) = dma_buf; // FIXME: work around DMA buffer alignment issue
		DTADR(dma_ch) = dev->dma_addr;

		DO_DMA(dma_ch, dma_len);

		len -= dma_len;
		if (likely(len == 0)) {
			break;
		} else {
			dma_buf = td->hwBE & ~PAGE_MASK;
		}
	}
}
#else
#if 0
static inline void dma_move_data(int chan, u32 src, u32 dst, u32 dma_len)
{
	const unsigned int DMA_FLAGS = DCMD_INCTRGADDR | DCMD_INCSRCADDR | DCMD_BURST32;

	if (chan < 0) {
		return;
	}

	DPRINTK("%s: Moving %d byte from %08x to %08x\n", __FUNCTION__,
		dma_len, src, dst);
	DCSR(chan) = DCSR_NODESC;
	DSADR(chan) = src;
	DTADR(chan) = dst;
	DCMD(chan) = DMA_FLAGS | (DCMD_LENGTH & dma_len);
	DPRINTK("%s: DMA started: DCSR=%08x, DSADR=%08x, DTADR=%08x, DCMD=%08x\n", __FUNCTION__,
		DCSR(chan), DSADR(chan), DTADR(chan), DCMD(chan));
	DCSR(chan) |= DCSR_RUN;
	while (!(DCSR(chan) & DCSR_STOPSTATE));
	DPRINTK("%s: DMA stopped: DCSR=%08x, DSADR=%08x, DTADR=%08x, DCMD=%08x\n", __FUNCTION__,
		DCSR(chan), DSADR(chan), DTADR(chan), DCMD(chan));
	DCSR(chan) = DCSR_NODESC;
}

static inline void sl811_dma_recv_data(struct usb_hcd *hcd, struct td *td,
					 struct hc_sl811_dev *dev, int count)
{
	int dma_ch = dev->dma;
	char *buf = dev->dma_buf;
	dma_addr_t dma_buf = td->hwCBP;
	int align = dma_buf & 7;
	int len = TD_BUF_LEN(td);
#if DEBUG_BUF_SIZE > 0
	int offset = TD_BUF_OFFSET(td);
#endif
	u8 sl811_start;
	int i;

	DPRINTK("%s: data_dma=%08x, cbp=%08x\n", __FUNCTION__, td->data_dma, td->hwCBP);

	BUG_ON(dma_ch < 0);

	if (len == 0) {
		// nothing to to, if no transfer buffer given
		DEBUG_STORE(0, 0x0d);
		return;
	}

	sl811_start = hc_sl811_read_reg(dev, SL11H_BUFADDRREG);

	if (count > len || count > (SL811_MAP_SIZE - sl811_start)) {
		DPRINTK("%s: count=%d, len=%d, start=0x%02x\n", __FUNCTION__,
			count, len, sl811_start);
		BUG();
	}

	hc_sl811_read_regs(dev, sl811_start, buf + align, count);
	consistent_sync(buf, count + align, PCI_DMA_TODEVICE);
	DEBUG_STORE((offset << 8) | count, 0x0d);
	for (i = 0; i < count; i++) {
		DEBUG_STORE(buf[i], 0x0c);
	}

	if (align) {
		DPRINTK("%s: Copying %d byte from %08x to %08x\n", __FUNCTION__,
			align, dma_buf - align, dev->dma_bus_addr);
		dma_move_data(dma_ch, dma_buf - align, dev->dma_bus_addr, align);
	}
	dma_move_data(dma_ch, dev->dma_bus_addr, dma_buf, count + align);
}

static inline void sl811_dma_send_data(struct usb_hcd *hcd, struct td *td,
				       struct hc_sl811_dev *dev, int count)
{
	int dma_ch = dev->dma;
	char *buf = dev->dma_buf;
	dma_addr_t dma_buf = td->hwCBP;
	int align = dma_buf & 7;
	int len = TD_BUF_LEN(td);
#if DEBUG_BUF_SIZE > 0
	int offset = TD_BUF_OFFSET(td);
#endif
	int i;

	BUG_ON(dma_ch < 0);

	if (len == 0) {
		// nothing to to, if no transfer buffer given
		DEBUG_STORE(0, 0x0e);
		return;
	}

	BUG_ON(count > len || count > SL811_BUF_SIZE);
	dma_move_data(dma_ch, dma_buf - align, dev->dma_bus_addr, count + align);
	consistent_sync(buf, count + align, PCI_DMA_FROMDEVICE);

	DEBUG_STORE((offset << 8) | count, 0x0e);
	for (i = 0; i < count; i++) {
		DEBUG_STORE(buf[i + align], 0x0c);
	}
	hc_sl811_write_regs(dev, SL11H_DATA_START, buf + align, count);
}
#else
static inline void sl811_dma_recv_data(struct usb_hcd *hcd, struct td *td,
				       struct hc_sl811_dev *dev, int count)
{
	char *buf = bus_to_virt(le32_to_cpu(td->hwCBP));
	int len = TD_BUF_LEN(td);
#if DEBUG_BUF_SIZE > 0
	int offset = TD_BUF_OFFSET(td);
#endif
	u8 sl811_start = hc_sl811_read_reg(dev, SL11H_BUFADDRREG);
	int i;

	if (len == 0) {
		// nothing to to, if no transfer buffer given
		DEBUG_STORE(0, 0x0d);
		return;
	}

	BUG_ON(count > len || count > (SL811_MAP_SIZE - sl811_start));
	BUG_ON(buf == NULL);

	DEBUG_STORE((offset << 8) | count, 0x0d);
	hc_sl811_read_regs(dev, sl811_start, buf, count);
	dma_sync_single(hcd->self.controller, td->hwCBP, count, DMA_TO_DEVICE);
	for (i = 0; i < count; i++) {
		DEBUG_STORE(buf[i], 0x0c);
	}
}

static inline void sl811_dma_send_data(struct usb_hcd *hcd, struct td *td,
				       struct hc_sl811_dev *dev, int count)
{
	char *buf = bus_to_virt(le32_to_cpu(td->hwCBP));
	int len = TD_BUF_LEN(td);
#if DEBUG_BUF_SIZE > 0
	int offset = TD_BUF_OFFSET(td);
#endif
	int i;

	if (len == 0) {
		// nothing to to, if no transfer buffer given
		DEBUG_STORE(0, 0x0e);
		return;
	}

	BUG_ON(count > len || count > SL811_BUF_SIZE);

	DEBUG_STORE((offset << 8) | count, 0x0e);
	for (i = 0; i < count; i++) {
		DEBUG_STORE(buf[i], 0x0c);
	}
	hc_sl811_write_regs(dev, SL11H_DATA_START, buf, count);
}
#endif // 1
#endif // USE_DMA

static inline void start_xfer(struct usb_hcd *hcd, struct hc_sl811_dev *dev,
		       struct xfer_buf *buf, struct td *td)
{
	if (buf->hc & SL11H_HCTLMASK_WRITE) {
		// transfer data from TD buffer to SL811 buffer memory
		sl811_dma_send_data(hcd, td, dev, buf->len);
	}
	hc_sl811_write_reg(dev, SL11H_BUFADDRREG, SL11H_DATA_START);
	hc_sl811_write_regs(dev, SL11H_BUFLNTHREG, &buf->len, 3);
	//hc_sl811_set_mask(dev, SL11H_INTENBLREG, SL11H_INTMASK_XFERDONE);
	hc_sl811_write_reg(dev, SL11H_HOSTCTLREG, buf->hc);

	DEBUG_STORE((((((buf->len << 8) | buf->pid_ep) << 8) | buf->dev_addr) << 8) | buf->hc, 0xd0);
	dev->last_td = td;
}

static inline int process_td(struct ohci_hcd *ohci, struct td *td)
{
	struct hc_sl811_dev *dev = ohci_to_sl811_dev(ohci);
	struct ed *ed = td->ed;
	u8 pid;
	struct xfer_buf *buf;
	u32 ed_flags = ed->hwINFO;
	u32 td_flags = td->hwINFO;
	u8 hc_flags = SL11H_HCTLMASK_ARM | SL11H_HCTLMASK_ENBLEP;
	u32 max_packet_size = ed_mps(ed->hwINFO);
	u32 buf_len = TD_BUF_LEN(td);
	int len;
#if DEBUG_BUF_SIZE > 0
	struct usb_hcd *hcd = ohci_to_hcd(ohci);
#endif

	buf = &dev->xfer_buf;
	pid = ed_td_to_pid[ed_pid(ed_flags)][td_pid(td_flags)];
	//DEBUG_STORE((((ed_pid(ed_flags) << 8) | td_pid(td_flags)) << 8) | pid, 0xcc);
	WARN_ON(pid == 0);
	len = max_packet_size;
	if (pid != PID_IN) {
		hc_flags |= SL11H_HCTLMASK_WRITE;
	}
	if (len > buf_len) {
		len = buf_len;
	}

	if (ed_flags & ED_LOWSPEED) {
		if (HcFmRemaining < HcLsThreshold) {
			return 0;
		}
		if (!(HcRhPortStatus[0] & RH_PS_LSDA)) {
			hc_flags |= SL11H_HCTLMASK_PREAMBLE;
		}
	}
	if (ed_flags & ED_ISO) {
		BUG();
		hc_flags |= SL11H_HCTLMASK_ISOCH;
	}
	if (pid != PID_SETUP) {
		if ((td_flags & TD_T) == TD_T_DATA1) {
			hc_flags |= SL11H_HCTLMASK_SEQ;
		} else if ((td_flags & TD_T) != TD_T_DATA0) {
			hc_flags |= ((ed->hwHeadP & ED_C) ? SL11H_HCTLMASK_SEQ : 0);
		}
	}
	buf->hc = hc_flags;
	buf->len = len;
	buf->pid_ep = (pid << 4) | ed_en(ed_flags);
	buf->dev_addr = ed_fa(ed_flags);

	DPRINTK( "%s: pid=%d, len=%d\n", __func__, pid, len );

	start_xfer(ohci_to_hcd(ohci), dev, buf, td);

	return 1;
}

#define ED_HEADP(ed) (ed->hwHeadP & ~0xf)
#define ED_TAILP(ed) (ed->hwTailP & ~0xf)
#define ED_NEXTED(ed) (ed->hwNextED & ~0xf)

static inline struct td *find_td(struct ohci_hcd *ohci, struct hc_sl811_dev *dev, struct ed *ed)
{
	struct td *td = NULL;

	for (td = dma_to_td(ohci, ED_HEADP(ed)); td != NULL; td = dma_to_td(ohci, td->hwNextTD)) {
		if (ED_TAILP(ed) == td->td_dma) {
			// TD queue empty
			td = NULL;
			break;
		}
		if (td->hwINFO & TD_DONE) {
			printk("%s: TD @ %08x already done\n", __FUNCTION__, (u32)td);
		} else {
			break;
		}
	}

	return td;
}

/*
 * process_ed_list
 *
 * generic ED list processing routine
 *
 * Return codes:	1	TD found and processed
 *			0	no TD needed processing
 *			<0	Error
 */
#define ED_HALTED(ed) ((ed)->hwHeadP & ED_H)
#define EP_SKIP(ed) ((ed) & ED_SKIP)
#define EP_ISO(ed) ((ed) & ED_ISO)
static int process_ed_list(struct ohci_hcd *ohci, u32 *head_reg)
{
	int ret = 0;
	int done = 0;
	struct td *td = NULL;
	struct ed *ed = (struct ed *)(*head_reg);
	struct hc_sl811_dev *dev = ohci_to_sl811_dev(ohci);

	// walk ED list and find TD that needs processing
	do {
		u32 ed_flags = ed->hwINFO;

		if (!EP_SKIP(ed_flags) && !ED_HALTED(ed) &&
		    !(EP_ISO(ed_flags) && !(HcControl & OHCI_CTRL_IE))) {
			td = find_td(ohci, dev, ed);
		}
		ed = ed->ed_next;
		*head_reg = (u32)ed;
		if (td != NULL) {
			done = process_td(ohci, td);
			ret = done;
		}
		done |= (ed == NULL);
	} while (!done);

	return ret;
}

/*
 * process_ctrl_bulk_lists
 *
 * try to find a TD that needs processing in the ctrl or bulk ED list
 * according to cbsr and cbc
 *
 * Return codes:	1	TD found and processed
 *			0	no TD needed processing
 *			<0	Error
 *
 */
static int process_ctrl_bulk_lists(struct ohci_hcd *ohci)
{
	struct hc_sl811_dev *dev = ohci_to_sl811_dev(ohci);
	//struct ohci_hcd *ohci = hcd_to_ohci(hcd);
	int ret = 0;
	int cbsr = HcControl & OHCI_CTRL_CBSR;	// ctrl/bulk ratio
	int bulk_ena = (HcControl & OHCI_CTRL_BLE) && (HcBulkCurrentED != 0);
#if DEBUG_BUF_SIZE > 0
	struct usb_hcd *hcd = ohci_to_hcd(ohci);
#endif
	// reset ctrl/bulk ratio counter, if bulk processing disabled or
	// no bulk descriptors to process
	if (!bulk_ena) {
		dev->cbc = 0;
	}
	// process ctrl EDs until cbsr limit reached
	if ((HcControl & OHCI_CTRL_CLE) && (dev->cbc <= cbsr)) {
		if (HcControlCurrentED != 0) {
			DDPRINTK( "%s: process HcControlCurrentED\n", __func__ );
			//DEBUG_STORE(HcControlCurrentED, 0xf0);
			ret = process_ed_list(ohci, &HcControlCurrentED);
			if (ret > 0) {
				dev->cbc++;
				HcCmdStatus |= OHCI_CLF;
				return ret;
			}
			if (ret != 0) {
				return ret;
			}
		}
	}

	if (bulk_ena) {
		//DEBUG_STORE(HcBulkCurrentED, 0xf1);
		DDPRINTK( "%s: process HcBulkCurrentED\n", __func__ );
		ret = process_ed_list(ohci, &HcBulkCurrentED);
		if (ret > 0) {
			HcCmdStatus |= OHCI_BLF;
			dev->cbc = 0;
		}
		if (ret != 0) {
			return ret;
		}
	}

	return ret;
}

#define OHCI_FR ((1 << 14) - 1)
#define OHCI_FRT (1 << 31)
static inline u32 update_fm_remaining(struct ohci_hcd *ohci)
{
	struct hc_sl811_dev *dev = ohci_to_sl811_dev(ohci);

	u32 fmremaining = hc_sl811_read_reg(dev, SL11H_CTLREG2) << 6;
	HcFmRemaining = (HcFmRemaining & ~OHCI_FR) | fmremaining;

	return fmremaining;
}

/*
 * process_ed_lists
 *
 * try to find a TD that needs processing in the appropriate ED list
 * depending on frame number and HcFmRemaining...
 *
 * Return codes:	1	TD found and processed
 *			0	no TD needed processing
 *			<0	Error
 *
 */
#define OHCI_PS ((1 << 14) - 1)
static int process_ed_lists(struct ohci_hcd *ohci)
{
	int ret = 0;
	int periodicstart = HcPeriodicStart & OHCI_PS;	// limit for ctrl/bulk processing
	u32 fmremaining = update_fm_remaining(ohci);

	DDPRINTK( "%s: fmrem=%08x\n", __func__, fmremaining );

	/*
	 * process cbsr control desc.
	 * process bulk desc.
	 */
	// do control/bulk processing until periodicstart reached
	if (fmremaining > periodicstart) {
		DDPRINTK( "%s: BULK\n", __func__ );
		ret = process_ctrl_bulk_lists(ohci);
		if (ret != 0) {
			return ret;
		}
		fmremaining = update_fm_remaining(ohci);
	}
	DDPRINTK( "%s: fmrem=%08x\n", __func__, fmremaining );

	// do interrupt/iso processing
	if ((HcControl & OHCI_CTRL_PLE) && (HcPeriodCurrentED != 0)) {
		DDPRINTK( "%s: ISO\n", __func__ );
		ret = process_ed_list(ohci, &HcPeriodCurrentED);
		if (ret != 0) {
			return ret;
		}
		fmremaining = update_fm_remaining(ohci);
	}
	DDPRINTK( "%s: fmrem=%08x\n", __func__, fmremaining );

	if (fmremaining > 0) {
		// do control/bulk processing again until end of frame
		DDPRINTK( "%s: CTRL/BULK\n", __func__ );
		ret = process_ctrl_bulk_lists(ohci);
	}

	return ret;
}

static inline void retire_td(struct usb_hcd *hcd, struct td *td, struct ed *ed)
{
	struct hc_sl811_dev *dev = hcd_to_sl811_dev(hcd);
	struct ohci_hcd *ohci = hcd_to_ohci(hcd);
	u32 ed_flags = ed->hwINFO;
	u32 td_flags = td->hwINFO;
	u32 td_dma = td->td_dma;

	td->hwINFO |= TD_DONE;
	if (ed_flags & ED_ISO) {
		ed->hwHeadP = td->hwNextTD;
	} else if ((td_flags & TD_T) == TD_T_DATA0) {
		ed->hwHeadP = td->hwNextTD & ~ED_C;
	} else if ((td_flags & TD_T) == TD_T_DATA1) {
		ed->hwHeadP = td->hwNextTD | ED_C;
	}
	DEBUG_STORE(ed->hwHeadP, 0xc2);
	td->hwNextTD = HcDoneHead;
	HcDoneHead = td_dma;

	if (!(HcIntrStatus & OHCI_INTR_WDH)) {
		u32 int_delay = (td_flags & TD_DI) >> 21;
		u32 done_head;

		if (int_delay == 7) {
			return;
		}
		if (int_delay < dev->intrdelay) {
			dev->intrdelay = int_delay;
		}
		if (dev->intrdelay > 0) {
			return;
		}
		done_head = HcDoneHead;
		done_head |= ((HcIntrStatus & ~OHCI_INTR_WDH) ? 1 : 0);

		HccaDoneHead = done_head;
		HcDoneHead = 0;

		HcIntrStatus |= OHCI_INTR_WDH;
	}
}

static inline void update_data_toggle(struct usb_hcd *hcd, struct td *td)
{
	u32 td_flags = td->hwINFO;
	u32 ed_carry = td->ed->hwHeadP & ED_C;

	if ((td_flags & TD_T) == TD_T_TOGGLE) {
		DPRINTK("%s: Using Data toggle %d from ED\n", __FUNCTION__, ed_carry);
		td_flags |= (ed_carry ? TD_T_DATA0 : TD_T_DATA1);
		DEBUG_STORE((u32)td | ed_carry, 0x42);
	} else if ((td_flags & TD_T) == TD_T_DATA0) {
		td_flags = (td_flags & ~TD_T) | TD_T_DATA1;
		DEBUG_STORE((u32)td, 0x41);
	} else {
		td_flags = (td_flags & ~TD_T) | TD_T_DATA0;
		DEBUG_STORE((u32)td, 0x40);
	}
	td->hwINFO = td_flags;
}

static inline int check_data_toggle(struct td *td, struct xfer_buf *done_buf, struct usb_hcd *hcd)
{
	int ret = 0;
	int pkt_stat = done_buf->pkt_stat;
	u32 td_flags = td->hwINFO & TD_T;
	u32 ed_carry = td->ed->hwHeadP & ED_C;
	int data1 = pkt_stat & SL11H_STATMASK_SEQ;
	int err = (pkt_stat & SL811_PKT_ERR_MASK) ^ data1;

	if (err) {
		if (pkt_stat != SL11H_STATMASK_NAK) {
			DEBUG_STORE(0xffffff00 | pkt_stat, 0xc0);
		}
		return -1;
	}
	if (td_flags == TD_T_TOGGLE) {
		ret = ((ed_carry && data1) || (!ed_carry && !data1));
	} else {
		ret = (((td_flags == TD_T_DATA0) && !data1) || ((td_flags == TD_T_DATA1) && data1));
	}
	DEBUG_STORE((((((ret << 8) | ed_carry) << 8) | (td_flags >> 24)) << 8) | pkt_stat, 0xc0);
	if (!ret) {
		DPRINTK("%s: pkt_stat=%02x, ret=%d ed_c=%d, td_d=%d\n", __FUNCTION__,
			pkt_stat, ret, ed_carry, td_flags >> 24);
		print_debug_buffer();
	}

	return ret;
}

static inline int update_cbp(struct td *td, int len)
{
	u32 cbp = td->hwCBP;
	u32 be = td->hwBE;
	int finished = 0;

	if (cbp == 0) {
		return 1;
	}
	if (TD_BUF_LEN(td) == len) {
		cbp = 0;
		finished = 1;
	} else {
		u32 chunk_size = DMA_XFER_SIZE(cbp, len);

		DPRINTK("%s: len=%d, chunk_size=%d buf_len=%d\n", __FUNCTION__,
			len, chunk_size, TD_BUF_LEN(td));

		if (chunk_size == len) {
			cbp += len;
		} else {
			DPRINTK("%s: len=%04x chunk_size=%04x buf_len=%04x cbp=%08x be=%08x\n",
				 __FUNCTION__, len, chunk_size, TD_BUF_LEN(td), cbp, be);
			cbp = (be & ~PAGE_MASK) | (len - chunk_size);
			print_debug_buffer();
		}
		finished = cbp == be;
	}
	td->hwCBP = cbp;

	return finished;
}

#define TD_CC_SET(info, cc) (info) = ((info) & ~TD_CC) | (((cc) << 28) & TD_CC)

#define TD_EC_GET(info) (((info) & TD_EC) >> 26)
#define TD_EC_SET(info, ec) (info) = ((info) & ~TD_EC) | (((ec) << 26) & TD_EC)

static inline u32 update_td_status(struct usb_hcd *hcd, struct xfer_buf *buf, struct td *td)
{
	u32 ohci_int_status = 0;
	struct ed *ed = td->ed;
	struct hc_sl811_dev *dev = hcd_to_sl811_dev(hcd);
	u32 td_flags = td->hwINFO;
	u8 pkt_stat = buf->pkt_stat;
	u32 ed_flags = ed->hwINFO;
	int iso = ed_flags & ED_ISO;
	int td_finished = 0;
	int td_halt = 0;
	u32 cc = TD_CC_NOERROR;

	if (pkt_stat & (SL811_PKT_ERR_MASK & ~SL11H_STATMASK_NAK)) {
		DEBUG_STORE((u32)td, 0xc3);
	}
	if (unlikely((pkt_stat & SL811_PKT_ERR_MASK) != 0)) {
		if (pkt_stat & SL11H_STATMASK_NAK) {
			return 0;
		} else if (pkt_stat & SL11H_STATMASK_STALL) {
			cc = TD_CC_STALL;
			td_halt = 1;
		} else if (pkt_stat & (SL11H_STATMASK_TMOUT |
				       SL11H_STATMASK_ERROR |
				       SL11H_STATMASK_SEQ)) {
			int err_count = TD_EC_GET(td_flags) + 1;

			if (pkt_stat & SL11H_STATMASK_TMOUT) {
				cc = TD_DEVNOTRESP;
			} else if (pkt_stat & SL11H_STATMASK_SEQ) {
				cc = TD_CC_DATATOGGLEM;
			} else {
				cc = TD_CC_CRC;
			}

			DEBUG_STORE((u32)td | err_count, 0xe0);
			TD_EC_SET(td->hwINFO, err_count);
			if (err_count == 3) {
				td_halt = 1;
			}
		} else if (pkt_stat & SL11H_STATMASK_OVF) {
			cc = TD_DATAOVERRUN;
		}
	} else {
		TD_EC_SET(td->hwINFO, 0);
		td_finished = update_cbp(td, buf->len);
		if (!td_finished && buf->len < ed_mps(ed_flags)) {
			td_halt = 1;
			if ((td_flags & TD_R)) {
				char *pkt_types[] = {
					"UNDEF", "OUT", "ACK", "DATA0", "PING", "SOF", "NYET", "DATA2",
					"SPLIT", "IN", "NAK", "DATA1", "PREAMBLE/ERR", "SETUP", "STALL", "MDATA"};
				DPRINTK("%s: Short %s packet: %d of %d byte\n", __FUNCTION__,
					 pkt_types[ed_td_to_pid[ed_pid(ed_flags)][td_pid(td_flags)] & 0x0f], buf->len, ed_mps(ed_flags));
				if (likely((td->hwBE & PAGE_MASK) == ((td->hwCBP + buf->len - 1) & PAGE_MASK))) {
					td->hwBE = td->hwCBP + buf->len - 1;
				} else {
					DPRINTK("%s: Adjusting hwBE %08x + %08x -> %08lx\n",
						 __FUNCTION__, td->hwBE, buf->len,
						 (td->hwBE & ~PAGE_MASK) + ((td->hwCBP + buf->len - 1) & ~PAGE_MASK));
					td->hwBE = (td->hwBE & ~PAGE_MASK) + ((td->hwCBP + buf->len - 1) & ~PAGE_MASK);
				}
				td->hwCBP = 0;
				td_finished = 1;
			} else {
				DPRINTK("%s: Data underrun: %d byte transferred, %d byte requested\n",
					__FUNCTION__, buf->len, ed_mps(ed_flags));
				cc = TD_DATAUNDERRUN;
			}
		}
	}
	TD_CC_SET(td->hwINFO, cc);

	if (pkt_stat & SL11H_STATMASK_ACK) {
		update_data_toggle(hcd, td);
	}

	if (td_halt && !td_finished) {
		print_debug_buffer();
	}
	if (td_halt && !iso) {
		ed->hwHeadP |= ED_H;
		td_finished = 1;
	}

	if (td_finished) {
		if (cc != TD_CC_NOERROR) {
			dev->intrdelay = 0;
		}
		retire_td(hcd, td, ed);
	}

	return ohci_int_status;
}

static inline void kill_td(struct usb_hcd *hcd, struct td *td)
{
	retire_td(hcd, td, td->ed);
}

static u32 finish_xfer(struct usb_hcd *hcd, struct hc_sl811_dev *dev, struct xfer_buf *done_buf)
{
	u32 int_status = 0;
	struct td *td;

	if (dev->last_td != NULL) {
		td = dev->last_td;
		dev->last_td = NULL;
#if DEBUG_BUF_SIZE > 0
		u8 pid = done_buf->pid_ep;
		u8 addr = done_buf->dev_addr;
#endif
		hc_sl811_read_regs(dev, SL11H_PKTSTATREG, &done_buf->pkt_stat, 2);

		DPRINTK( "%s: td=%p, len=%d, stat=%04x\n", __func__, td, done_buf->count, done_buf->pkt_stat );

		done_buf->len -= done_buf->count;
		DEBUG_STORE((((((done_buf->len << 8) | pid) << 8) | addr) << 8) | done_buf->pkt_stat, 0xc1);
		if (!(done_buf->hc & SL11H_HCTLMASK_WRITE)) {
			//if (check_data_toggle(td, done_buf->pkt_stat, hcd)) {
			if (check_data_toggle(td, done_buf, hcd)) {
				done_buf->pkt_stat &= ~SL11H_STATMASK_SEQ;
			} else {
				done_buf->pkt_stat |= SL11H_STATMASK_SEQ;
			}
			//if (likely(done_buf->pkt_stat & SL811_PKT_ERR_MASK) == 0) {
			if (likely((done_buf->pkt_stat & (SL811_PKT_ERR_MASK & ~SL11H_STATMASK_SEQ)) == 0)) {
				// transfer data from SL811 buffer memory to TD buffer
				if (done_buf->len > 0) {
					sl811_dma_recv_data(hcd, td, dev, done_buf->len);
				}
			}
		} else {
			done_buf->pkt_stat &= ~SL11H_STATMASK_SEQ;
		}
		int_status = update_td_status(hcd, done_buf, td);
	}

	return int_status;
}

/*
 * Interrupt routine:
 * 
 * XFER_DONE interrupt:
 * - do outstanding data transfer of current TD
 * - prepare next TD, if any
 * - postprocess last completed TD 
 * - find next TD to process
 *
 * SOF interrupt:
 * - check current td pointer
 * - kickstart an ED list processing, if NULL
 *
 *
 */
static irqreturn_t ohci_sl811_interrupt(int irq, void *data, struct pt_regs *pt_regs)
{
	int handled = 0;
	struct usb_hcd *hcd = data;
	struct ohci_hcd *ohci = hcd_to_ohci(hcd);
	struct hc_sl811_dev *dev = hcd_to_sl811_dev(hcd);
	const int max_loops = 5;
	int loop = 0;
	u8 int_mask;
	u8 int_status;
	u8 addr_save = HC_SL811_READ_ADDR(dev);

	do {
		u8 svc_mask;

		int_status = hc_sl811_read_reg(dev, SL11H_INTSTATREG);
		int_mask = hc_sl811_read_reg(dev, SL11H_INTENBLREG);
		svc_mask = int_status & int_mask;

		SET_GPIO(5);
		if (loop == 0) {
			DEBUG_STORE((((((int_status << 8) | int_mask) << 8) | svc_mask) << 8) | !!dev->last_td, 0x0f);
		}
		if (svc_mask == 0) {
			if (!handled) {
				DPRINTK("Spurious interrupt: status:mask=%02x:%02x\n", int_status,
					 int_mask);
				print_debug_buffer();
			}
			break;
		}
		handled = 1;
		DDPRINTK("%s: Servicing: %02x(%02x:%02x)\n", __FUNCTION__, svc_mask, int_status, int_mask);
#ifdef SOF_B
		if (svc_mask & SL11H_INTMASK_XFERDONE_B) {
			DPRINTK( "%s: XFERDONE_B\n", __func__ );
			svc_mask &= ~SL11H_INTMASK_XFERDONE_B;
			hc_sl811_clr_mask(dev, SL11H_INTENBLREG, SL11H_INTMASK_XFERDONE_B);
		}
#endif
		if (svc_mask & SL11H_INTMASK_XFERDONE) {
			svc_mask &= ~SL11H_INTMASK_XFERDONE;
			DPRINTK( "%s: XFERDONEB\n", __func__ );

			if (dev->last_td != NULL) {
				HcIntrStatus |= finish_xfer(hcd, dev, &dev->xfer_buf);
				process_ed_lists(ohci);
			} else {
				//hc_sl811_clr_mask(dev, SL11H_INTENBLREG, SL11H_INTMASK_XFERDONE);
			}
		}

#if 0
		if (svc_mask & SL11H_INTMASK_USBRESET) {
			u8 ctl1 = hc_sl811_read_reg(dev, SL11H_CTLREG1) & SL11H_CTL1MASK_SUSPEND;
			svc_mask &= ~SL11H_INTMASK_USBRESET;

			hc_sl811_clr_mask(dev, SL11H_INTENBLREG, SL11H_INTMASK_USBRESET);
			if (ctl1) {
				DPRINTK("%s: Resume Interrupt occured\n", __FUNCTION__);
				if (HcRhStatus & RH_HS_DRWE) {
					HcIntrStatus |= OHCI_INTR_RD;
				}
			} else {
				int connected = hc_sl811_handle_insrmv(hcd);

				DPRINTK("%s: Device Present Interrupt occured: %d\n", __FUNCTION__,
					 connected);
				hc_sl811_start_sof(hcd, connected);
			}
		}
#endif
		if (svc_mask & SL11H_INTMASK_INSRMV) {
			int connected;

			svc_mask &= ~SL11H_INTMASK_INSRMV;
			int_mask &= ~SL11H_INTMASK_INSRMV;

			DPRINTK("%s: Device Insert/Remove occured\n", __FUNCTION__);
			if (dev->last_td) {
				DPRINTK("%s: Nuking last_td: %08x\n", __FUNCTION__,
					 (u32)dev->last_td);
				kill_td(hcd, dev->last_td);
				dev->last_td = NULL;
			}

			hc_sl811_usb_reset(hcd, -1);
			connected = hc_sl811_handle_insrmv(hcd);
			DPRINTK("%s: Device %spresent\n", __FUNCTION__, connected ? "" : "not ");
			hc_sl811_start_sof(hcd, connected);
		}

		if (svc_mask & SL11H_INTMASK_SOFINTR) {
			u16 frame_no = HcFmNumber + 1;
			int int_no = frame_no & 0x1f;	// index into interrupt table

			svc_mask &= ~SL11H_INTMASK_SOFINTR;

			if (dev->intrdelay > 0  && dev->intrdelay < 7) {
				dev->intrdelay--;
				if (dev->intrdelay == 0) {
					HcIntrStatus |= OHCI_INTR_WDH;
				}
			}
			HcFmRemaining = HcFmInterval & (OHCI_FR | OHCI_FRT);
			HccaFrameNumber = HcFmNumber = frame_no;
			if ((frame_no & 0x7fff) == 0) {
				//DPRINTK("%s: Frame number overflow\n", __FUNCTION__);
				HcIntrStatus |= OHCI_INTR_FNO;
			}
			HcIntrStatus |= OHCI_INTR_SF;
			if (HcPeriodCurrentED != 0) {
				int soc = (HcCmdStatus & OHCI_SOC) + (1 << 16);

				dev->last_td = NULL;
				// scheduling overrun
				DPRINTK("%s: Scheduling overrun at frame %04x\n", __FUNCTION__,
					 frame_no);
				HcIntrStatus |= OHCI_INTR_SO;
				HcCmdStatus = (HcCmdStatus & ~(OHCI_SOC | OHCI_CLF | OHCI_BLF)) |
					(soc & OHCI_SOC);
				HcControlCurrentED = 0;
				HcBulkCurrentED = 0;
				HcPeriodCurrentED = 0;
				update_fm_remaining(ohci);
			}
			if ((HcControl & OHCI_CTRL_PLE) && (HccaInterruptTable[int_no] != 0)) {
				HcPeriodCurrentED = (u32)ohci->periodic[int_no];
			}
			if ((HcControl & OHCI_CTRL_CLE) && (HcControlCurrentED == 0)) {
				if (HcCmdStatus & OHCI_CLF) {
					HcCmdStatus &= ~OHCI_CLF;
					HcControlCurrentED = HcControlHeadED;
				}
			}
			if ((HcControl & OHCI_CTRL_BLE) && (HcBulkCurrentED == 0)) {
				if (HcCmdStatus & OHCI_BLF) {
					HcCmdStatus &= ~OHCI_BLF;
					HcBulkCurrentED = HcBulkHeadED;
				}
			}
			process_ed_lists(ohci);
		}

		if (svc_mask) {
			DPRINTK("%s: Unhandled SL811 interrupt %02x\n", __FUNCTION__,
				 svc_mask);
			hc_sl811_clr_mask(dev, SL11H_INTENBLREG, svc_mask);
		}
		// acknowledge all interrupts we have serviced right now

		//DPRINTK("%s: Acknowledging interrupts %02x(svc:%02x,int:%02x,msk:%02x)\n", __FUNCTION__, int_status & int_mask, svc_mask, int_status, int_mask);
		hc_sl811_write_reg(dev, SL11H_INTSTATREG, int_status & int_mask);
		DEBUG_STORE(((loop << 8) | (int_status & int_mask)), 0x0b);
		PULSE_GPIO(4);
	} while (++loop < max_loops);

	CLR_GPIO(5);
	HC_SL811_WRITE_ADDR(dev, addr_save);
	if (HcIntrEnable & OHCI_INTR_MIE) {
		if (HcIntrStatus & HcIntrEnable) {
			u32 ohci_intr_status = HcIntrStatus & HcIntrEnable;

			//DPRINTK("%s: Calling ohci_irq(%08x)\n", __FUNCTION__, ohci_intr_status);
			if (usb_hcd_irq(hcd->irq, hcd, pt_regs) != IRQ_HANDLED) {
				DPRINTK("%s: Spurious OHCI Interrupt: %08x\n", __FUNCTION__,
					 ohci_intr_status);
			}
		}
	}
	return IRQ_RETVAL(handled);
	//return IRQ_RETVAL(1);
}
