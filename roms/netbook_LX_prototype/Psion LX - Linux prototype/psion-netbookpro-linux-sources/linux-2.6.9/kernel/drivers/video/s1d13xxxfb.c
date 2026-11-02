/* drivers/video/s1d13xxxfb.c
 *
 * (c) 2004 Simtec Electronics
 *
 * Driver for Epson S1D13xxx series framebuffer chips 
 *
 * Note, only currently tested with S1D13806.
 *
 * Released under GPL
*/

#include <linux/config.h>
#include <linux/module.h>
#include <linux/device.h>

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/fb.h>

#include <asm/io.h>

#ifdef __arm__
#include <asm/mach-types.h>
#endif

#include "s1d13xxxfb.h"

#define PFX "s1d138xx: "

#define MAX_REGS (128)

unsigned short regsave_list[] = {
    0x0001, 0x01FC, 0x0004, 0x0005, 0x0008, 0x0009, 0x0010,
    0x0014, 0x0018, 0x001C, 0x001E, 0x0021, 0x002A, 0x002B,
    0x0020, 0x0030, 0x0031, 0x0032, 0x0034, 0x0035, 0x0036,
    0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x0040, 0x0041,
    0x0042, 0x0043, 0x0044, 0x0046, 0x0047, 0x0048, 0x004A,
    0x004B, 0x0050, 0x0052, 0x0053, 0x0054, 0x0056, 0x0057,
    0x0058, 0x0059, 0x005A, 0x005B, 0x0060, 0x0062, 0x0063,
    0x0064, 0x0066, 0x0067, 0x0068, 0x006A, 0x006B, 0x0070,
    0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
    0x0078, 0x007A, 0x007B, 0x007C, 0x007E, 0x0080, 0x0081,
    0x0082, 0x0083, 0x0084, 0x0085, 0x0086, 0x0087, 0x0088,
    0x008A, 0x008B, 0x008C, 0x008E, 0x0100, 0x0101, 0x0102,
    0x0103, 0x0104, 0x0105, 0x0106, 0x0108, 0x0109, 0x010A,
    0x010C, 0x010D, 0x0110, 0x0111, 0x0112, 0x0113, 0x0114,
    0x0115, 0x0118, 0x0119, 0x01E0, 0x01E2, 0x01F0, 0x01F1,
    0x01F4, 0x01FC
};


struct s1dfb_regsave {
	unsigned char       reg_val;
};


struct s1dfb {
	void                *disp_mem;
	void                *regs;
	void                *disp_save;   /* pm: save entire screen here */
	struct s1dfb_mach   *machine;
	struct fb_info       fbinfo;

	unsigned int         pseudo_palette[16];
	struct s1dfb_regsave regsave[ARRAY_SIZE(regsave_list)+1];
};

#if 0
#define dbg(x...) do { printk(KERN_DEBUG x); } while(0)
#else
#define dbg(x...) do { } while (0)
#endif

struct s1dfb *s1dfb;

 unsigned int
s1d13xxx_readreg(struct s1dfb *fb, int regno)
{
	//dbg("s1d13xxx_readreg: fb=%p, regno=%d\n", fb, regno);

	return readb((unsigned long)fb->regs + regno);
}

void
s1d13xxx_writereg(struct s1dfb *fb, int regno, int value)
{
	//dbg("s1d13xxx_writereg: fb=%p, %04x, value=%02x\n", fb, regno, value);

	writeb(value, (unsigned long)fb->regs + regno);
}

void
s1d13xxx_runinit(struct s1dfb *fb, struct s1d13xxx_reginit *inits)
{
	const struct s1d13xxx_regval *ptr;
	int count;

	ptr = inits->values;

	for (count = inits->length; count > 0; count--, ptr++) {
		s1d13xxx_writereg(fb, ptr->addr, ptr->value);
	}
}


/* framebuffer control routines */

static int
s1d13xxx_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	dbg("s1d13xxx_check_var: var=%p, info=%p\n", var, info);


	return 0;
}

static void
s1d13xxx_setup_pseudocolour(struct fb_info *info)
{
	info->fix.visual = FB_VISUAL_PSEUDOCOLOR;

	info->var.red.length = 4;
	info->var.green.length = 4;
	info->var.blue.length = 4;
}

static void
s1d13xxx_setup_truecolour(struct fb_info *info)
{
	info->fix.visual = FB_VISUAL_TRUECOLOR;

	info->var.red.length = 5;
	info->var.red.offset = 11;

	info->var.green.length = 6;
	info->var.green.offset = 5;

	info->var.blue.length = 5;
	info->var.blue.offset = 0;
}

static int
s1d13xxx_set_par(struct fb_info *info)
{
	unsigned int val;

	dbg("s1d13xxx_set_par: info=%p\n", info);

	dbg("s1d13xxx_set_par: bpp=%d\n", info->var.bits_per_pixel);

	val = s1d13xxx_readreg(s1dfb, 0x40);   /* read colour control */

	val &= ~7;

	switch (info->var.bits_per_pixel) {
	case 4:
		s1d13xxx_setup_pseudocolour(info);
		val |= 2;
		break;
	case 8:
		s1d13xxx_setup_pseudocolour(info);
		val |= 3;
		break;
	case 16:
		s1d13xxx_setup_truecolour(info);
		val |= 5;
		break;

	default:
		dbg("bpp not supported!\n");
		return -EINVAL;
	}

	dbg("writing %02x to lcd display mode register\n", val);
	s1d13xxx_writereg(s1dfb, 0x40, val);

	info->fix.line_length  = info->var.xres * info->var.bits_per_pixel;
	info->fix.line_length /= 8;
	
	dbg("setting line_length to %d\n", info->fix.line_length);

	dbg("done setup\n");
	
	return 0;
}

static int
s1d13xxx_setcolreg(u_int regno,
		   u_int red,
		   u_int green,
		   u_int blue,
		   u_int transp,
		   struct fb_info *info)
{
	if (0) {
		dbg("s1d13xxx_setcolreg: %d: rgb=%d,%d,%d, tr=%d\n",
		    regno, red, green, blue, transp);
	}

	if (regno < 16) {
		unsigned int pseudo_val;
		/* deal with creating pseudo-palette entries */
		
		pseudo_val  = (red   >> 11) << info->var.red.offset;
		pseudo_val |= (green >> 10) << info->var.green.offset;
		pseudo_val |= (blue  >> 11) << info->var.blue.offset;

		if (0) {
			dbg("s1d13xxx_setcolreg: pseudo %d, val %08x\n", 
			    regno, pseudo_val);
		}

		((u32 *)info->pseudo_palette)[regno] = pseudo_val;
	}

	if (info->var.bits_per_pixel > 8) {
		return 0;
	}

	s1d13xxx_writereg(s1dfb, 0x1e2, regno);
	s1d13xxx_writereg(s1dfb, 0x1e4, red);
	s1d13xxx_writereg(s1dfb, 0x1e4, green);
	s1d13xxx_writereg(s1dfb, 0x1e4, blue);

	dbg("s1d13xxx_setcolreg: done\n");

	return 0;
}

static int
s1d13xxx_blank(int blank, struct fb_info *info)
{
	dbg("s1d13xxx_blank: blank=%d, info=%p\n", blank, info);

	if (s1dfb->machine->blank != NULL)
		(s1dfb->machine->blank)(s1dfb, blank);

	return 0;
}		

static int
s1d13xxx_pan_display(struct fb_var_screeninfo *var,
		     struct fb_info *info)
{
	return 0;
}

/* framebuffer information structures */



static struct fb_ops s1d13xxx_fbops = {
	.owner           = THIS_MODULE,
	.fb_check_var    = s1d13xxx_check_var,
	.fb_set_par      = s1d13xxx_set_par,
	.fb_setcolreg    = s1d13xxx_setcolreg,
	.fb_blank        = s1d13xxx_blank,

	.fb_pan_display  = s1d13xxx_pan_display,

	/* to be replaced by any acceleration we can */
	.fb_fillrect     = cfb_fillrect,
	.fb_copyarea     = cfb_copyarea,
	.fb_imageblit    = cfb_imageblit,
	.fb_cursor       = soft_cursor
};

static void
do_test_pattern(unsigned long base)
{
	unsigned short *sp = (unsigned short *)base;
	int x, y;
	int r, g, b;

	dbg("writing default pattern to screen\n");

	for (y = 0; y < 600; y++) {
		for (x = 0; x < 800; x++) {
			r = x / 32;
			g = y / 16;
			b = 0;

			if (r > 0x1f)
				r = 0x1f;
			if (g > 0x1f)
				g = 0x1f;

			sp[x + (y*800)] = r | (g<<5) | (r << 11);
		}
	}

	dbg("written pattern to screen\n");
}



static int
s1d13xxx_init_fbinfo(struct s1dfb *sdfb, struct fb_info *fbinfo)
{
	unsigned long size;

	dbg("s1d13xxx_init_fbinfo: sdfb=%p, fbinfo=%p\n", sdfb, fbinfo);

	fbinfo->var.xres_virtual   = 800;
	fbinfo->var.yres_virtual   = 600;
	fbinfo->var.xres           = 800;
	fbinfo->var.yres           = 600;
	
	fbinfo->var.bits_per_pixel = 16;
	fbinfo->var.activate       = FB_ACTIVATE_NOW;
	fbinfo->var.height         = -1;
	fbinfo->var.width          = -1;

	size = 1280*1024;

	if (size < (800*600*2)) {
		printk(KERN_ERR "size is not big enough for 800x600 16bpp\n");
	}

	fbinfo->screen_base        = ioremap_nocache((u32)sdfb->disp_mem, size);
	fbinfo->pseudo_palette     = sdfb->pseudo_palette;

	dbg("fbinfo->screen_base = %p, size %08x\n",
	    fbinfo->screen_base, (int)size);

	do_test_pattern((unsigned long)fbinfo->screen_base);

	fbinfo->fix.type           = FB_TYPE_PACKED_PIXELS;
	fbinfo->fix.smem_start     = (unsigned long)sdfb->disp_mem;
	fbinfo->fix.smem_len       = size;

	return 0;
}

void
s1d13xxx_set_machine(struct s1dfb *sdfb, struct s1dfb_mach *mach)
{
	dbg("%s: machine at %p (%s)\n", __FUNCTION__, mach, mach->name);

	sdfb->machine = mach;
}



/* s1d13xxx_probe()
 *
 * allocate resources and try and identify the framebuffer chip we
 * are dealing with
*/

int 
s1d13xxx_probe(struct s1dfb *sdfb)
{
	struct fb_info *fbinfo = &sdfb->fbinfo;
	unsigned int vers;

	s1dfb = sdfb;

	vers = s1d13xxx_readreg(sdfb, S1DREG_REVCODE);
	if ((vers >> 2) != 7) {
		printk(KERN_ERR PFX "did not detect controller\n");
		return -ENOENT;
	}

	printk(PFX "s1d13806: revision %d\n", vers & 3);

	/* ok, now let us register a framebuffer driver */

	strcpy(fbinfo->fix.id, "s1d13xxx");
	
	fbinfo->fbops = &s1d13xxx_fbops;
	fbinfo->flags = FBINFO_FLAG_DEFAULT;

	s1d13xxx_init_fbinfo(sdfb, fbinfo);

	fb_alloc_cmap(&fbinfo->cmap, 256, 0);

	if (machine_is_netbookpro()) {
		s1dfb_netbookpro_init(sdfb);
	}

	if (register_framebuffer(fbinfo) < 0) {
		printk(PFX ": failed to install framebuffer\n");
		return -EINVAL;
	}
	
	printk(KERN_INFO "fb%d: %s framebuffer device\n",
	       fbinfo->node, fbinfo->fix.id);

	return 0;
}

int
s1d13xxx_setup(const char *args)
{
	dbg("s1d13xxx_setup: args=%p\n", args);
	dbg("s1313xxx_setup: '%s'\n", args);

	return 0;
}

static void free_sdfb(struct s1dfb *sdfb)
{
	if (sdfb != NULL) {
		/* TODO: unmap resources */

		kfree(sdfb);
	}
}

static int __init s1d13xxx_devprobe(struct device *dev)
{
	struct platform_device *platdev = to_platform_device(dev);
	struct resource *res;
	struct s1dfb *sdfb;
	int seen = 0;
	int i;

	res = platdev->resource;

	dbg("probe called: device is %p\n", dev);

	sdfb = kmalloc(sizeof(*sdfb), GFP_KERNEL);
	if (sdfb == NULL) {
		printk(KERN_ERR PFX "failed to allocate device info\n");
		return -ENOMEM;
	}

	memset(sdfb, 0, sizeof(*sdfb));

	for (i = 0; i < platdev->num_resources; i++, res++) {
		if (!(res->flags & IORESOURCE_MEM))
			continue;

		switch (seen) {
		case 0:
			sdfb->disp_mem = (void *)res->start;
			seen++;
			break;
		case 1:
			sdfb->regs = ioremap_nocache(res->start, 0x1000);
			seen++;
			break;

		default:
			/* more resources than we needed? */
			dbg("extra resource %d,%p\n", i, res);
		}
	}

	if (seen < 2) {
		printk(KERN_ERR PFX "not enough supplied resources\n");
		free_sdfb(sdfb);
		return -EINVAL;
	}

	if (sdfb->disp_mem != NULL && sdfb->regs != NULL) {
		s1d13xxx_probe(sdfb);
		dev_set_drvdata(dev, sdfb);
	} else {
		printk(KERN_ERR PFX "no allocate resources to probe\n");
		free_sdfb(sdfb);
		return -EINVAL;
	}

	return 0;
}

static int s1d13xxx_suspend(struct device *dev, u32 state, u32 level)
{
	int i;

#if 0
	if (s1dfb->disp_save == NULL) {
		s1dfb->disp_save = kmalloc(1280*1024, GFP_KERNEL);
	}

	if (s1dfb->disp_save == NULL) {
		printk(KERN_ERR PFX "no memory to save screen");
		return -ENOMEM;
	}

	memcpy(s1dfb->disp_save, s1dfb->fbinfo.screen_base,
	       1024*1280);
#else
	s1dfb->disp_save = NULL;
#endif

	for (i = 0; i < ARRAY_SIZE(regsave_list); i++) {
		s1dfb->regsave[i].reg_val = s1d13xxx_readreg(s1dfb, regsave_list[i]);
	}

	if (s1dfb->machine->suspend != NULL) {
		(s1dfb->machine->suspend)(s1dfb);
	}

	return 0;
}

static int s1d13xxx_resume(struct device *dev, u32 level)
{
	int i;

	if (level != RESUME_ENABLE)
		return 0;

	for (i = 0; i < ARRAY_SIZE(regsave_list); i++) {
		s1d13xxx_writereg(s1dfb, regsave_list[i],
				  s1dfb->regsave[i].reg_val);
	}

	if (s1dfb->disp_save != NULL) {
		memcpy(s1dfb->fbinfo.screen_base, s1dfb->disp_save,
		       1280*1024);
		kfree(s1dfb->disp_save);
	} else {
		//do_test_pattern(s1dfb->fbinfo.screen_base);
	}

	if (s1dfb->machine->resume != NULL) {
		(s1dfb->machine->resume)(s1dfb);
	}

	return 0;
}

static struct device_driver s1d_driver = {
	.name        = "epson-s1d138xxx",
	.bus         = &platform_bus_type,
	.probe       = s1d13xxx_devprobe,	
#ifdef CONFIG_PM
	.suspend     = s1d13xxx_suspend,
	.resume      = s1d13xxx_resume
#endif
};


static int __init s1d13xxx_init(void)
{
	printk("Epson S1D13XXX FB Driver, (c) 2004 Simtec Electronics\n");

	return driver_register(&s1d_driver);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ben Dooks, Simtec Electronics <ben@simtec.co.uk>");

late_initcall(s1d13xxx_init);
