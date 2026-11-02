/*
 * SL811HS HCD (Host Controller Driver) for USB.
 * 
 * COPYRIGHT (C) by CYPRESS SEMICONDUCTOR INC 
 * 
 *
 */

//#define USE_DMA //Can't use DMA because of SL811 register address alignment :(
//#define CONSISTENT_ALLOC
//#define KERNEL_THREAD
//#define DEBUG_BUF_SIZE 4096
#if DEBUG_BUF_SIZE > 0
static struct debug_buffer {
	u32 tag;
	u32 data;
	u32 frame_no;
	u32 timestamp;
} *debug_buffer;
static int debug_on = -1;
static int debug_level = 0x10;
static unsigned int debug_buf_wr = 0;
static unsigned int debug_buf_cnt = 0;
static unsigned int debug_buf_rd = 0;

#define print_debug_buffer() _print_debug_buffer(__FUNCTION__, __LINE__)
static void _print_debug_buffer(const char *func, int line)
{
	BUG_ON(debug_buf_cnt > DEBUG_BUF_SIZE);
	BUG_ON(debug_buf_wr > DEBUG_BUF_SIZE);
	BUG_ON(debug_buf_rd > DEBUG_BUF_SIZE);
	if (debug_buffer != NULL) {
		printk(KERN_WARNING "%s: %s @ %d: cnt=%d, rd=%d, wr=%d\n", __FUNCTION__,
		       func, line, debug_buf_cnt, debug_buf_rd, debug_buf_wr);
		while (debug_buf_cnt > 0) {
			struct debug_buffer *dbg = &debug_buffer[debug_buf_rd];
			static u32 last_time = 0;

			printk(KERN_DEBUG "dbg: %02x: %08x frame: %04x rem: %06x time: %10u(%10u)\n",
			       (u8)dbg->tag, dbg->data, dbg->frame_no, dbg->tag >> 8,
			       dbg->timestamp, dbg->timestamp - last_time);
			last_time = dbg->timestamp;
			debug_buf_rd++;
			debug_buf_cnt--;
			if (debug_buf_rd >= DEBUG_BUF_SIZE) {
				debug_buf_rd -= DEBUG_BUF_SIZE;
			}
#if 0
			if ((dbg->tag & 0xff) == 0xc0) {
				printk("\n");
			}
#endif
		}
	}
}

#define DEBUG_STORE(data, tag) _debug_store(hcd ? hcd_to_ohci(hcd)->regs : NULL, data, tag)
static inline void _debug_store(struct ohci_regs *regs, u32 data, u8 tag) {
	if (debug_on && tag >= debug_level) {
		if (likely(debug_buffer != NULL)) {
			struct debug_buffer *dbg;

			BUG_ON(debug_buf_cnt > DEBUG_BUF_SIZE);
			BUG_ON(debug_buf_wr > DEBUG_BUF_SIZE);
			BUG_ON(debug_buf_rd > DEBUG_BUF_SIZE);
			if (debug_buf_cnt == DEBUG_BUF_SIZE) {
#if 0
				print_debug_buffer();
#endif
			}
			dbg = &debug_buffer[debug_buf_wr++];
			dbg->data = data;
			if (regs != NULL) {
				dbg->tag = tag | (regs->fmremaining << 8);
				dbg->frame_no = regs->fmnumber;
			} else {
				dbg->tag = tag;
				dbg->frame_no = -1;
			}
			dbg->timestamp = OSCR;
			if (unlikely(debug_buf_wr >= DEBUG_BUF_SIZE)) {
				debug_buf_wr -= DEBUG_BUF_SIZE;
			}
			if (debug_buf_cnt < DEBUG_BUF_SIZE) {
				debug_buf_cnt++;
			} else {
				debug_buf_rd++;
				if (unlikely(debug_buf_rd >= DEBUG_BUF_SIZE)) {
					debug_buf_rd -= DEBUG_BUF_SIZE;
				}
			}
		}
	}
}
#define ENABLE_DEBUG if (debug_on >= 0) debug_on++
#define DISABLE_DEBUG if (debug_on > 0) debug_on--
#define DEBUG_LVL(n) debug_level = (n)
#else
#define print_debug_buffer() do {} while(0)
#define debug_on 0
#define ENABLE_DEBUG
#define DISABLE_DEBUG
#define DEBUG_LVL(n)
#define DEBUG_STORE(data, tag) do { (hcd) = (hcd); } while(0)
#endif

#define DATA0_WR	0x07	// (Arm+Enable+tranmist to Host+DATA0)
#define DATA1_WR	0x47	// (Arm+Enable+tranmist to Host on DATA1)
#define ZDATA0_WR	0x05	// (Arm+Transaction Ignored+tranmist to Host+DATA0)
#define ZDATA1_WR	0x45	// (Arm+Transaction Ignored+tranmist to Host+DATA1)
#define DATA0_RD	0x03	// (Arm+Enable+received from Host+DATA0)
#define DATA1_RD	0x43	// (Arm+Enable+received from Host+DATA1)

#define PID_SETUP	0x2d	// USB Specification 1.1 Standard Definition
#define PID_SOF		0xA5
#define PID_IN		0x69
#define PID_OUT		0xe1

#define SL11H_HOSTCTLREG	0
#define SL11H_BUFADDRREG	1
#define SL11H_BUFLNTHREG	2
#define SL11H_PKTSTATREG	3	/* read */
#define SL11H_PIDEPREG		3	/* write */
#define SL11H_XFERCNTREG	4	/* read */
#define SL11H_DEVADDRREG	4	/* write */
#define SL11H_CTLREG1		5
#define SL11H_INTENBLREG	6

#define SL11H_HOSTCTLREG_B	8
#define SL11H_BUFADDRREG_B	9
#define SL11H_BUFLNTHREG_B	0x0A
#define SL11H_PKTSTATREG_B	0x0B	/* read */
#define SL11H_PIDEPREG_B	0x0B	/* write */
#define SL11H_XFERCNTREG_B	0x0C	/* read */
#define SL11H_DEVADDRREG_B	0x0C	/* write */

#define SL11H_INTSTATREG	0x0D	/* write clears bitwise */
#define SL11H_HWREVREG		0x0E	/* read */
#define SL11H_SOFLOWREG		0x0E	/* write */
#define SL11H_SOFTMRREG		0x0F	/* read */
#define SL11H_CTLREG2		0x0F	/* write */
#define SL11H_DATA_START	0x10

/* Host control register bits (addr 0) */
#define SL11H_HCTLMASK_ARM	0x01
#define SL11H_HCTLMASK_ENBLEP	0x02
#define SL11H_HCTLMASK_WRITE	0x04
#define SL11H_HCTLMASK_ISOCH	0x10
#define SL11H_HCTLMASK_AFTERSOF	0x20
#define SL11H_HCTLMASK_SEQ	0x40
#define SL11H_HCTLMASK_PREAMBLE	0x80

/* Packet status register bits (addr 3) */
#define SL11H_STATMASK_ACK	0x01
#define SL11H_STATMASK_ERROR	0x02
#define SL11H_STATMASK_TMOUT	0x04
#define SL11H_STATMASK_SEQ	0x08
#define SL11H_STATMASK_SETUP	0x10
#define SL11H_STATMASK_OVF	0x20
#define SL11H_STATMASK_NAK	0x40
#define SL11H_STATMASK_STALL	0x80

#define SL811_PKT_ERR_MASK (SL11H_STATMASK_ERROR |	\
			    SL11H_STATMASK_TMOUT |	\
			    SL11H_STATMASK_OVF |	\
			    SL11H_STATMASK_NAK |	\
			    SL11H_STATMASK_STALL |	\
			    SL11H_STATMASK_SEQ)

/* Control register 1 bits (addr 5) */
#define SL11H_CTL1MASK_SOFENA	(1 << 0)
#define SL11H_CTL1MASK_NOTXEOF2	(1 << 2) /* ??? */
#define SL11H_CTL1MASK_DSTATE	0x18
#define SL11H_CTL1MASK_NSPD	0x20
#define SL11H_CTL1MASK_SUSPEND	0x40
#define SL11H_CTL1MASK_CLK12	0x80

#define SL11H_CTL1VAL_NORMAL	0x00
#define SL11H_CTL1VAL_RESET	0x08
#define SL11H_CTL1VAL_FORCE_J	0x10
#define SL11H_CTL1VAL_FORCE_K	0x18

/* Interrupt enable (addr 6) and interrupt status register bits (addr 0xD) */
#define SL11H_INTMASK_XFERDONE	0x01
#define SL11H_INTMASK_XFERDONE_B 0x02
#define SL11H_INTMASK_SOFINTR	0x10
#define SL11H_INTMASK_INSRMV	0x20
#define SL11H_INTMASK_USBRESET	0x40
#define SL11H_INTMASK_DSTATE	0x80	/* only in status reg */

#define SL811_INT_MASK		(SL11H_INTMASK_SOFINTR | SL11H_INTMASK_INSRMV)

/* HW rev and SOF lo register bits (addr 0xE) */
#define SL11H_HWRMASK_HWREV	0xF0

/* SOF counter and control reg 2 (addr 0xF) */
#define SL11H_CTL2MASK_SOFHI	0x3F
#define SL11H_CTL2MASK_DSWAP	0x40
#define SL11H_CTL2MASK_HOSTMODE	0x80

#define SL811_MAP_SIZE		256
#define SL811_BUF_SIZE		(SL811_MAP_SIZE - SL11H_DATA_START)

#define ohci_to_hcd(ohci) (&(ohci)->hcd)

struct xfer_buf {
	u8 len;
	union {
		u8 pid_ep;
		u8 pkt_stat;
	} __attribute__((packed));
	union {
		u8 dev_addr;
		u8 count;
	} __attribute__((packed));
	u8 hc;
} __attribute__((packed));

struct hc_sl811_dev {
	struct device	dev;
	struct resource	res;
	//unsigned long phys_addr;
	//unsigned long mem_size;
	unsigned long	dma_addr;
	void		*addr_reg;
	void		*data_reg;
	int dma;
	int irq;
#if defined(CONSISTENT_ALLOC) || defined(TEST_DMA)
	char *dma_buf;
	dma_addr_t dma_bus_addr;
#endif
	//struct semaphore hw_lock;
#ifdef KERNEL_THREAD
	struct completion complete;
	struct task_struct *task;
	wait_queue_head_t task_wait;
	//wait_queue_head_t update;
#endif
#ifdef DEV_REGS
	struct ohci_regs ohci_regs;
#endif
	struct td *last_td;
	struct xfer_buf xfer_buf;
	int cbc;
	int intrdelay;
	int hw_rev;
#ifdef KERNEL_THREAD
	union {
		int i;
		struct {
			int irq:1;
			int sof:1;
			int shutdown:1;
			int portreset:1;
#if 0
			int control:1;
			int cmdstatus:1;
			int intrenable:1;
			int intrstatus:1;
			int ed_controlhead:1;
			int ed_controlcurrent:1;
			int ed_bulkhead:1;
			int ed_bulkcurrent:1;
			int roothub_a:1;
			int roothub_b:1;
			int roothub_status:1;
			int roothub_portstatus:1;
#endif
		} b;
	} flags;
#endif
};

struct hc_sl811_driver {
	struct device_driver	drv;
	unsigned int		devid;
	int (*probe)(struct hc_sl811_dev *dev);
	int (*remove)(struct hc_sl811_dev *dev);
	int (*suspend)(struct hc_sl811_dev *dev, u32 state);
	int (*resume)(struct hc_sl811_dev *dev);
};

#define SL811_DRV(_d)      container_of((_d), struct hc_sl811_driver, drv)

/*
 * SL811 HW Interface
 */

#define hc_sl811_addr_reg (dev->addr_reg)
#define hc_sl811_data_reg (dev->data_reg)

static inline void HC_SL811_WRITE_ADDR(struct hc_sl811_dev *dev, int reg)
{
	void *hcd = NULL;

	writeb(reg, hc_sl811_addr_reg);
	DEBUG_STORE(reg, 0x01);
}

static inline void HC_SL811_WRITE_DATA(struct hc_sl811_dev *dev, int val)
{
	void *hcd = NULL;

	writeb(val, hc_sl811_data_reg);
	DEBUG_STORE(val, 0x03);
}

static inline u8 HC_SL811_READ_ADDR(struct hc_sl811_dev *dev)
{
	u8 reg;
	void *hcd = NULL;

	reg = readb(hc_sl811_addr_reg);
	DEBUG_STORE(reg, 0x00);

	return reg;
}

static inline u8 HC_SL811_READ_DATA(struct hc_sl811_dev *dev)
{
	u8 val;
	void *hcd = NULL;

	val = readb(hc_sl811_data_reg);
	DEBUG_STORE(val, 0x02);

	return val;
}

static inline u8 hc_sl811_read_reg(struct hc_sl811_dev *dev, int reg)
{
	u8 val;

	HC_SL811_WRITE_ADDR(dev, reg);
	val = HC_SL811_READ_DATA(dev);
	DDPRINTK("%s: %02x<%02x\n", __FUNCTION__, val, reg);

	return val;
}

static inline void hc_sl811_write_reg(struct hc_sl811_dev *dev, int reg, u8 val)
{
	HC_SL811_WRITE_ADDR(dev, reg);
	HC_SL811_WRITE_DATA(dev, val);
	DDPRINTK("%s: %02x>%02x\n", __FUNCTION__, val, reg);
}

static inline void hc_sl811_set_mask(struct hc_sl811_dev *dev, int reg, u8 mask)
{
	u8 val = hc_sl811_read_reg(dev, reg);
	if ((val | mask) != val) {
		DDPRINTK("%s: %02x->%02x\n", __FUNCTION__, val, val | mask);
	}
	val |= mask;
	hc_sl811_write_reg(dev, reg, val);
}

static inline void hc_sl811_clr_mask(struct hc_sl811_dev *dev, int reg, u8 mask)
{
	u8 val = hc_sl811_read_reg(dev, reg);
	if ((val & ~mask) != val) {
		DDPRINTK("%s: %02x->%02x\n", __FUNCTION__, val, val & ~mask);
	}
	val &= ~mask;
	hc_sl811_write_reg(dev, reg, val);
}

static int hc_sl811_handle_insrmv(struct usb_hcd *hcd);
static void hc_sl811_usb_reset(struct usb_hcd *hcd, int assert);
static void hc_sl811_start_sof(struct usb_hcd *hcd, int lowspeed);
