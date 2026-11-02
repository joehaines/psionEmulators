
#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/compiler.h>
#include <linux/init.h>
#include <linux/completion.h>
#include <linux/device.h>
#include <linux/delay.h>

#include <linux/dma-mapping.h>

#include <asm/hardware.h>
#include <asm/dma.h>

#include <asm/arch/netbookpro.h>
#include <asm/arch/pxa-regs.h>

#include "nbp-power.h"

#define PFX "nbp-spi: "

/* this driver is simply to deal with receiving SPI transfers via
 * dma. The transmit side is set to just send the same buffer each time
 * to ensure nothing is transmitted.
 *
 * The receive buffers are treated like a ring buffer, with the last
 * buffer in the ring marked as the stop point. When the spi request is
 * deal with, the buffer is marked as free again.
*/ 

#define SPI_ENABLE (1)

#define SPI_DMASIZE     (32)
#define SPI_RXBUFFERS   (12)

#define IS_LASTRX(x)    ((x) >= (SPI_RXBBUFFERS-1))

#define SPI_RXDMA_CMD \
  ( DCMD_INCTRGADDR | \
    DCMD_FLOWSRC    | \
    DCMD_ENDIRQEN   | \
    DCMD_BURST32    | \
    DCMD_WIDTH2     | \
    SPI_DMASIZE )

//     DCMD_ENDIAN 


//#define TX_IRQCFG (DCMD_ENDIRQEN)
#define TX_IRQCFG (0)

#define SPI_TXDMA_CMD \
  ( DCMD_INCSRCADDR | \
    DCMD_FLOWSRC    | \
    DCMD_BURST32    | \
    DCMD_WIDTH2     | \
    TX_IRQCFG       | \
    SPI_DMASIZE )

/* size of this structure must be aligned to 4 words */
struct spi_buffer {
        pxa_dma_desc    dma_desc;
	unsigned char   data[SPI_DMASIZE];
};

/*
 * This describes the DMA data buffer.
 */
struct spi_dma_data {
	struct spi_buffer	rx[SPI_RXBUFFERS] __attribute__((aligned(16)));
	struct spi_buffer	tx __attribute__((aligned(16)));
};

/*
 * This macro returns the DMA address of element 'x' from the above
 * structure.
 */
#define SPI_DMAPTR(x)	(spi_dmaaddr + offsetof(struct spi_dma_data, x))

/* dma descriptors for the channels */

static dma_addr_t              spi_dmaaddr;
static struct spi_dma_data    *spi_cpuaddr;
static int                     rx_chan;
static int                     tx_chan;

/* keep a mapping of the dma buffer's physical addresses for reference */
static dma_addr_t              spi_rxphys[SPI_RXBUFFERS];

/* receive state information */

static unsigned int            rx_last;    /* last read */

static int tx_irqs;

#if 0
static void dbg(const char *msg,...)
{
	va_list va;
	char buff[1024];

	va_start(va, msg);
	vsprintf(buff, msg, va);
	va_end(va);

	printascii(buff);
}
#else
#define dbg(msg,x...) do { } while (0) /*printk(KERN_DEBUG msg ,##x )*/
#endif

#if 0
static void dbg_buff(const char *name, struct spi_buffer *buff)
{
	unsigned int *ptr = (unsigned int *)buff->data;

	dbg(PFX "%s desc[%p]: DDADR=%08x DSA=%08x DTA=%08x CMD=%08x\n", 
	       name, buff,
	       buff->dma_desc.ddadr,
	       buff->dma_desc.dsadr,
	       buff->dma_desc.dtadr,
	       buff->dma_desc.dcmd);

	dbg(PFX "%s buffend:%p: %08x %08x %08x %08x\n", name,
	    ptr, ptr[0], ptr[1], ptr[2], ptr[3]);
	dbg(PFX "%s buffend:%p: %08x %08x %08x %08x\n", name
	    ptr, ptr[4], ptr[5], ptr[6], ptr[7]);

}
#else
#define dbg_buff(n,b) do { } while(0)
#endif

#if 0
static void dbg_state(const char *where)
{
	dbg(PFX "%s:\n", where);
	dbg(PFX "SSCR0=%08x, SSCR1=%08x, SSSR=%08x\n",
	    SSCR0, SSCR1, SSSR);
	dbg(PFX "DCSR(rx_chan)=%08x, DCSR(tx_chan)=%08x\n",
	    DCSR(rx_chan), DCSR(tx_chan));
	dbg(PFX "DCMD(rx_chan)=%08x, DCMD(tx_chan)=%08x\n",
	    DCMD(rx_chan), DCMD(tx_chan));
	dbg(PFX "DDADR(rx_chan)=%08x DDADR(tx_chan)=%08x\n",
	    DDADR(rx_chan), DDADR(tx_chan));
	dbg(PFX "DSADR(rx_chan)=%08x DSADR(tx_chan)=%08x\n",
	    DDADR(rx_chan), DDADR(tx_chan));
	dbg(PFX "DTADR(rx_chan)=%08x DTADR(tx_chan)=%08x\n",
	    DDADR(rx_chan), DDADR(tx_chan));
}
#else
#define dbg_state(where) do { } while (0)
#endif

/*
 * Zero the data areas, and initialise the DMA ring buffers.
 */
static int nbp_spi_init_dmabuf(struct device *dev)
{
	int i;

	spi_cpuaddr = dma_alloc_coherent(dev, sizeof(struct spi_dma_data),
					 &spi_dmaaddr, GFP_KERNEL);

	if (spi_cpuaddr == NULL) {
		printk(KERN_ERR PFX "no memory for DMA buffers\n");
		return -ENOMEM;
	}

	memset(spi_cpuaddr, 0, sizeof(struct spi_dma_data));

	for (i = 0; i < SPI_RXBUFFERS; i++) {
		u32 ddadr;

		ddadr = spi_dmaaddr;
		if (i != SPI_RXBUFFERS - 1)
			ddadr = SPI_DMAPTR(rx[i + 1].dma_desc);

		spi_cpuaddr->rx[i].dma_desc.ddadr = ddadr;
		spi_cpuaddr->rx[i].dma_desc.dsadr = __PREG(SSDR);
		spi_cpuaddr->rx[i].dma_desc.dtadr = SPI_DMAPTR(rx[i].data);
		spi_cpuaddr->rx[i].dma_desc.dcmd  = SPI_RXDMA_CMD;

		spi_rxphys[i] = SPI_DMAPTR(rx[i].dma_desc);

		dbg_buff("rx init", &spi_cpuaddr->rx[i]);
	}

	spi_cpuaddr->tx.dma_desc.ddadr = SPI_DMAPTR(tx.dma_desc);
	spi_cpuaddr->tx.dma_desc.dsadr = SPI_DMAPTR(tx.data);
	spi_cpuaddr->tx.dma_desc.dtadr = __PREG(SSDR);
	spi_cpuaddr->tx.dma_desc.dcmd  = SPI_TXDMA_CMD;

	dbg_buff("tx init", &spi_cpuaddr->tx);

	return 0;
}

static void nbp_spi_free_dmabuf(struct device *dev)
{
	dma_free_coherent(dev, sizeof(struct spi_dma_data), spi_cpuaddr,
			  spi_dmaaddr);
}

/*
 * Disable the SPI engine and its associated DMA channels.
 */
static void nbp_spi_disable(void)
{
	dbg(PFX "disabling SPI\n");

	SSCR0 &= ~SSCR0_SSE;

	DCSR(rx_chan) = 0;
	DCSR(tx_chan) = 0;

	pxa_set_cken(CKEN3_SSP, 0);
}

/*
 * Enable the SPI engine and its associated DMA channels.
 */
static void nbp_spi_enable(void)
{
	dbg(PFX "Enabling SPI\n");

	pxa_set_cken(CKEN3_SSP, 1);

	DRCMRRXSSDR = rx_chan | DRCMR_MAPVLD;
	DRCMR1      = tx_chan | DRCMR_MAPVLD;

	rx_last     = 0;
	DDADR(rx_chan) = SPI_DMAPTR(rx[0].dma_desc);
	DDADR(tx_chan) = SPI_DMAPTR(tx.dma_desc);

	/* configure the SPI engine */

	/* from 2.4:
	 *
	 * SSCR0 -> 0xF | (1<<7)  (0x8F)
	 * SSCR1 -> (11<<6) | (15<<10)  = (0x3EC0)
	 */

	//SSCR0 = SSCR0_Motorola | SSCR0_DataSize(16) | SSCR0_SerClkDiv(2);
	//SSCR1 = SSCR1_TxTresh(12) | SSCR1_RxTresh(16);

	SSCR0 = SSCR0_SSE | SSCR0_Motorola | SSCR0_DataSize(16) | SSCR0_SerClkDiv(4);
	SSCR1 = SSCR1_TxTresh(12) | SSCR1_RxTresh(16);

	DCSR(rx_chan) |= DCSR_RUN;
	DCSR(tx_chan) |= DCSR_RUN;
	
	barrier();

	dbg_state(__func__);
}

void nbp_spi_reset(void)
{
	dbg_state(__func__);

	if (0) {
		/* set the ssp's pin configurations */
		pxa_gpio_mode(GPIO19_DREQ1_MD);
		pxa_gpio_mode(GPIO23_SCLK_md);
		pxa_gpio_mode(GPIO24_SFRM_MD);
		pxa_gpio_mode(GPIO25_STXD_MD);
		pxa_gpio_mode(GPIO26_SRXD_MD);
	}

	nbp_spi_disable();
	nbp_spi_enable();
}


#define DCSR_IRQS (0xf)

static void spi_dma_rxirq(int irq, void *param, struct pt_regs *regs)
{
	unsigned long ddadr;
	unsigned long dcsr;
	unsigned long dtadr;

	dcsr  = DCSR(rx_chan);
	ddadr = DDADR(rx_chan);
	dtadr = DTADR(rx_chan);

	dbg(PFX "rx irq received, dcsr=%08lx, ddadr=%08lx, dtadr=%08lx (tx %d)\n",
	    dcsr, ddadr, dtadr, tx_irqs);

	if (dcsr & DCSR_BUSERR) {
		printk(KERN_ERR PFX "rx: dma error (%08lx)\n", dcsr);

		DCSR(rx_chan) = (DCSR(rx_chan) & ~DCSR_IRQS) | DCSR_BUSERR;
	}

        if (dcsr & DCSR_ENDINTR) {
		unsigned int buff = rx_last;

		DCSR(rx_chan) = (DCSR(rx_chan) & ~DCSR_IRQS) | DCSR_ENDINTR;

		dbg(PFX "rx: dma channel end\n");

		while (1) {
			struct spi_buffer *bufptr = &spi_cpuaddr->rx[buff];

			buff = (buff + 1) % SPI_RXBUFFERS;

			// see if this is the last buffer...
			if (spi_rxphys[buff] == (ddadr & ~15))
				break;

			rx_last = buff;

			dbg_buff("rx done", bufptr);

			nbp_battmon_rx(bufptr->data);

			/* zero the buffer we where given */
			memset(bufptr->data, 0, sizeof(bufptr->data));
		}
	}
}

static void spi_dma_txirq(int irq, void *param, struct pt_regs *regs)
{
	unsigned int dcsr;

	dcsr  = DCSR(tx_chan);
	DCSR(tx_chan) |= DCSR_ENDINTR | DCSR_STARTINTR | DCSR_BUSERR;

	printk(KERN_DEBUG PFX "tx: dcsr=%08x\n", dcsr);

	tx_irqs++;
}

static int __init nbp_spi_probe(struct device *dev)
{
	int ret;

	ret = nbp_spi_init_dmabuf(dev);
	if (ret)
		return ret;

	rx_chan = pxa_request_dma("SPI RX", DMA_PRIO_LOW, spi_dma_rxirq, NULL);
	if (rx_chan < 0) {
		printk(KERN_ERR PFX "failed to allocate RX DMA channel (%d)\n",
		       rx_chan);
		nbp_spi_free_dmabuf(dev);
		return rx_chan;
	}

	tx_chan = pxa_request_dma("SPI TX", DMA_PRIO_LOW, spi_dma_txirq, NULL);
	if (tx_chan < 0) {
		printk(KERN_ERR PFX "failed to allocate TX DMA channel (%d)\n",
		       tx_chan);
		nbp_spi_free_dmabuf(dev);
		return tx_chan;
	}

	printk(KERN_INFO PFX "SPI: RX DMA %d, TX DMA %d\n", rx_chan, tx_chan);

	if (SPI_ENABLE) {
		nbp_spi_reset();
	}

	return 0;
}

static int nbp_spi_suspend(struct device *dev, u32 state, u32 level)
{
	if (level == SUSPEND_DISABLE)
		nbp_spi_disable();

	return 0;
}

static int nbp_spi_resume(struct device *dev, u32 level)
{
	if (level == RESUME_ENABLE)
		nbp_spi_enable();

	return 0;
}


static struct device_driver spi_driver = {
	.name        = "netbookpro-spi",
	.bus         = &platform_bus_type,
	.probe       = nbp_spi_probe,
	.suspend     = nbp_spi_suspend,
	.resume      = nbp_spi_resume,
};

static int __init nbp_spi_init(void)
{
	printk(KERN_INFO "NetBookPro SPI Driver, (c) 2004 Simtec Electronics\n");
	return driver_register(&spi_driver);
}

static void __exit nbp_spi_exit(void)
{
	driver_unregister(&spi_driver);
}

module_init(nbp_spi_init);
module_exit(nbp_spi_exit);

MODULE_AUTHOR("Ben Dooks, Simtec Electronics <ben@simtec.co.uk>");
MODULE_DESCRIPTION("Simple SPI Driver for PXA2XX for Psion NetBookPro");
MODULE_LICENSE("GPL");
