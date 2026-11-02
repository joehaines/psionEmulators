/* drivers/mtd/nand/netbookpro.c
 *
 * Written by Ben Dooks <ben@simtec.co.uk>
 *   Based on 2.4 version by Mark Whittaker
 *
 * (c) 2004 Simtec Electronics
 *
 * NAND device driver for Psion NetbookPro
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/device.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <asm/io.h>

struct nbp_mtd {
	void             *io_base;
	void             *io_sync;
	unsigned long     tmp;
	struct mtd_info  *mtd;
	struct mtd_info   mtd_info;
	struct nand_chip  nand_chip;
};


static struct mtd_partition partition_info[] = {
        { name:   "NetbookPro nand 1",
          offset: 0x00400000,
          size:   0x03800000 },
        { name:   "NetbookPro nand 2",
          offset: 0x03c00000,
          size:   0x00400000 }
};

struct nbp_mtd *nbp_mtd = NULL;

#define nbp_nand_getmymtd(x) nbp_mtd

/* nbp_nand_dosync()
 *
 * needed due to bus-reordering within the PXA itself (see section on
 * I/O ordering in PXA manual (section 2.3, p35)
*/

static void
nbp_nand_dosync(struct nbp_mtd *my_mtd)
{
	unsigned long tmp;

	/* this should generate the read, followed by something that
	 * depends on the read
	 */
	tmp = readl(my_mtd->io_sync);

	/* generate instruction dependant on the value of the read */
	__asm__ __volatile__("mov %1, %0\n" : "=r" (tmp) : "r" (tmp));
}

static void
nbp_nand_hwcontrol(struct mtd_info *mtd, int cmd)
{
	nbp_nand_dosync(nbp_nand_getmymtd(mtd));

	switch (cmd) {
	case NAND_CTL_SETCLE:
		GPSR(21) = GPIO_bit(21);
		break;

	case NAND_CTL_CLRCLE:
		GPCR(21) = GPIO_bit(21);
		break;

	case NAND_CTL_SETALE:
		GPSR(22) = GPIO_bit(22);
		break;

	case NAND_CTL_CLRALE:
		GPCR(22) = GPIO_bit(22);
		break;

	case NAND_CTL_SETNCE:
		GPCR(62) = GPIO_bit(62);
		break;

	case NAND_CTL_CLRNCE:
		GPSR(62) = GPIO_bit(62);
		break;

	case NAND_CTL_SETWP:
		GPCR(61) = GPIO_bit(61);
		break;

	case NAND_CTL_CLRWP:
		GPSR(61) = GPIO_bit(61);
		break;

	default:
		printk(KERN_ERR "%s: unknown cmd %d\n", __FUNCTION__, cmd);
	}

	nbp_nand_dosync(nbp_nand_getmymtd(mtd));
}


static void
nbp_nand_writebyte(struct mtd_info *mtd, u_char byte)
{
	struct nbp_mtd *my_mtd = nbp_nand_getmymtd(mtd);

	writeb(byte, my_mtd->io_base);
	nbp_nand_dosync(my_mtd);
}

static void
nbp_nand_writeword(struct mtd_info *mtd, u16 word)
{
	struct nbp_mtd *my_mtd = nbp_nand_getmymtd(mtd);

	writew(word, my_mtd->io_base);
	nbp_nand_dosync(my_mtd);
}

/* not sure if we need to have sync on the reads, but to be
 * safe for the moment, we'll put this in here
*/

static u_char
nbp_nand_readbyte(struct mtd_info *mtd)
{
	struct nbp_mtd *my_mtd = nbp_nand_getmymtd(mtd);
	u_char res;

	res = readb(my_mtd->io_base);
	nbp_nand_dosync(my_mtd);

	return res;
}

static u16
nbp_nand_readword(struct mtd_info *mtd)
{
	struct nbp_mtd *my_mtd = nbp_nand_getmymtd(mtd);
	u16 res;

	res = readw(my_mtd->io_base);
	nbp_nand_dosync(my_mtd);

	return res;
}

#define is_aligned(b) (!((unsigned long)(b) & 1))

static void
nbp_nand_writebuf(struct mtd_info *mtd, const u_char *buf, int len)
{
	struct nbp_mtd *my_mtd = nbp_nand_getmymtd(mtd);
	struct nand_chip *this = mtd->priv;

	nbp_nand_dosync(my_mtd);
	if (is_aligned(buf)) {
		writesw(this->IO_ADDR_W, buf, len>>1);
	} else {
		int i;
		u16 *ptr = (u16 *)buf;

		for (i = 0; i < len; i+=2, ptr++) {
			writew(*ptr, this->IO_ADDR_W);
		}
	}
	nbp_nand_dosync(my_mtd);
}

static void
nbp_nand_readbuf(struct mtd_info *mtd, u_char *buf, int len)
{
	struct nbp_mtd *my_mtd = nbp_nand_getmymtd(mtd);
	struct nand_chip *this = mtd->priv;

	nbp_nand_dosync(my_mtd);

	if (is_aligned(buf)) {
		readsw(this->IO_ADDR_R, buf, len>>1);
	} else {
		int i;
		u16 *ptr = (u16 *)buf;

		for (i = 0; i < len; i+=2, ptr++) {
			*ptr = readw(this->IO_ADDR_R);
		}	
	}

	nbp_nand_dosync(my_mtd);
}

static int
nbp_nand_verifybuf(struct mtd_info *mtd, const u_char *buf, int len)
{
	struct nbp_mtd *my_mtd = nbp_nand_getmymtd(mtd);
	struct nand_chip *this = mtd->priv;
	int i;
	int err = 0;
	u16 *p = (u16 *) buf;
	u16 read;	
	len >>= 1;

	nbp_nand_dosync(my_mtd);

	for (i = 0; i < len; i++) {
		read = readw(this->IO_ADDR_R);
		if (read != p[i]) {
			printk(KERN_DEBUG "%s: err at %d (read %04x vs %04x)\n",
			       __FUNCTION__, i, read, p[i]);
			err = -EFAULT;
		}		
	}

	nbp_nand_dosync(my_mtd);
	return err;
}


static int
nbp_nand_devready(struct mtd_info *mtd)
{
	/* not sure wether we really need to sync here, but it shouldn't
	   cost us too much to check wether this is ok? */

	nbp_nand_dosync( nbp_nand_getmymtd(mtd));

	return (GPLR(20) & GPIO_bit(20));
}

static void nbp_nand_cleanup(void)
{
	if (nbp_mtd == NULL)
		return;

	if (nbp_mtd->mtd != NULL) {
		nand_release(nbp_mtd->mtd);
		nbp_mtd->mtd = NULL;
	}

	nbp_nand_hwcontrol(NULL, NAND_CTL_SETWP);

	iounmap(nbp_mtd->io_base);
	iounmap(nbp_mtd->io_sync);

	kfree (nbp_mtd);
}

static int netbookpro_nand_remove(struct device *dev)
{
	nbp_nand_cleanup();
	return 0;
}

static int netbookpro_nand_probe(struct device *dev)
{
	struct nbp_mtd *my_mtd;
	struct nand_chip *this;
	size_t data_size;

	printk(KERN_INFO "NetbookPro NAND driver, (c) 2004 Simtec Electronics\n");

	nbp_mtd = my_mtd = kmalloc(sizeof(*my_mtd), GFP_KERNEL);
	if (my_mtd == NULL) {
		printk(KERN_ERR "netbookpro: failed to create NAND MTD\n");
		return -ENOMEM;
	}

	memset(my_mtd, 0, sizeof(*my_mtd));

	this = &my_mtd->nand_chip;

	my_mtd->io_base = ioremap(0x04000000, 2);
	my_mtd->io_sync = ioremap(0x08000000, 4);

	printk(KERN_INFO "netbookpro: MTD registers at %p, sync %p\n",
	       my_mtd->io_base, my_mtd->io_sync);

	this->IO_ADDR_R  = (int)my_mtd->io_base;
	this->IO_ADDR_W  = (int)my_mtd->io_base;
	this->eccmode    = NAND_ECC_SOFT;
	this->options   |= (NAND_NO_AUTOINCR | NAND_BUSWIDTH_16);
	this->chip_delay = 15;               /* 15 us command delay time */

	/* install our routines */

	this->hwcontrol  = nbp_nand_hwcontrol;
	this->write_byte = nbp_nand_writebyte;
	this->write_word = nbp_nand_writeword;
	this->read_byte  = nbp_nand_readbyte;
	this->read_word  = nbp_nand_readword;
	this->verify_buf = nbp_nand_verifybuf;
	this->read_buf   = nbp_nand_readbuf;
	this->write_buf  = nbp_nand_writebuf;
	this->dev_ready  = nbp_nand_devready;

	nbp_nand_hwcontrol(NULL, NAND_CTL_CLRWP);

	/* set the ready/busy pin to input */
	pxa_gpio_mode(20 | GPIO_IN);

	/* set the mtd private data for the nand driver */
	my_mtd->mtd_info.priv = this;

	if (nand_scan (&my_mtd->mtd_info, 1)) {
		printk(KERN_ERR "netbookpro: no nand chips found?\n");
		nbp_nand_cleanup();
		return -ENXIO;
	}

	my_mtd->mtd      = &my_mtd->mtd_info;

	data_size = my_mtd->mtd_info.oobblock + my_mtd->mtd_info.oobsize;
	this->data_buf = kmalloc(data_size, GFP_KERNEL);
	if (this->data_buf == NULL) {
		printk(KERN_ERR "%s: cannot get %d bytes for data_buf \n",
		       __FUNCTION__, data_size);
		nbp_nand_cleanup();
		return -ENOMEM;
	}

	if (my_mtd->mtd_info.size <= 32*1024*1024)
		partition_info[0].size -= 0x2000000;

	printk(KERN_INFO "Adding partition at %08x, size %08x\n",
	       partition_info[0].offset, partition_info[0].size);

	add_mtd_partitions(&my_mtd->mtd_info, partition_info,
			   ARRAY_SIZE(partition_info));

	return 0;
}

#ifdef CONFIG_PM
static int netbookpro_nand_suspend(struct device *dev, u32 state, u32 level)
{
	return 0;
}

static int netbookpro_nand_resume(struct device *dev, u32 level)
{
	if (level == RESUME_ENABLE) {
		nbp_nand_hwcontrol(NULL, NAND_CTL_CLRWP);
		pxa_gpio_mode(20 | GPIO_IN);
	}

	return 0;
}
#else
#define netbookpro_nand_suspend NULL
#define netbookpro_nand_resume NULL
#endif

static struct device_driver netbookpro_nand_driver= {
	.name		= "netbookpro-nand",
	.bus		= &platform_bus_type,
	.probe		= netbookpro_nand_probe,
	.remove		= netbookpro_nand_remove,
	.suspend	= netbookpro_nand_suspend,
	.resume		= netbookpro_nand_resume,
};

void __exit nbp_exit(void)
{
	driver_unregister(&netbookpro_nand_driver);
	nbp_nand_cleanup();
}

int __init nbp_nand_init(void)
{
	return driver_register(&netbookpro_nand_driver);
}


module_init(nbp_nand_init);
module_exit(nbp_nand_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ben Dooks <ben@simtec.co.uk>");
MODULE_DESCRIPTION("NAND Driver for NetbookPro");
