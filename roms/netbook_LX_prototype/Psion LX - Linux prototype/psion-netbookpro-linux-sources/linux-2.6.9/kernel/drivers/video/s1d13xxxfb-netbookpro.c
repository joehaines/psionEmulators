/* drivers/video/s1d13x-netbookpro.c
 *
 * (c) 2004 Simtec Electronics
 *
 * Netbookpro specific bits for Epson 13806
 */


#include <linux/config.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/types.h>

#include <linux/proc_fs.h>

#include <asm/uaccess.h>

#include "s1d13xxxfb.h"

#define PFX "s1dfb-nbp: "

#if 0
extern void printascii(const char *);
#define dbg(x...) do { char buff[1024]; printascii(PFX); sprintf(buff, x); printascii(buff); } while(0)
#else
#define dbg(x...) do {} while(0)
#endif




static const struct s1d13xxx_regval init_netbookpro_vals[] = {
    { 0x0001, 0x00 },   // Miscellaneous Register
    { 0x01FC, 0x00 },   // Display Mode Register
    { 0x0004, 0xFF },   // General IO Pins Configuration Register 0
    { 0x0005, 0x1F },   // General IO Pins Configuration Register 1
    { 0x0008, 0x7F },   // General IO Pins Control Register 0
    { 0x0009, 0x00 },   // General IO Pins Control Register 1

    { 0x0010, 0x01 },   // Memory Clock Configuration Register
    { 0x0014, 0x00 },   // LCD Pixel Clock Configuration Register
    { 0x0018, 0x02 },   // CRT/TV Pixel Clock Configuration Register
    { 0x001C, 0x02 },   // MediaPlug Clock Configuration Register
    { 0x001E, 0x01 },   // CPU To Memory Wait State Select Register

    { 0x0021, 0x03 },   // DRAM Refresh Rate Register
    { 0x002A, 0x00 },   // DRAM Timings Control Register 0
    { 0x002B, 0x01 },   // DRAM Timings Control Register 1
    { 0x0020, 0x80 },   // Memory Configuration Register

    { 0x0030, 0x25 },   // Panel Type Register
    { 0x0031, 0x00 },   // MOD Rate Register
    { 0x0032, 0x63 },   // LCD Horizontal Display Width Register
    { 0x0034, 0x1F },   // TFT FPLINE Start Position Register
    { 0x0035, 0x01 },   // TFT FPLINE Start Position Register
    { 0x0036, 0x0B },   // TFT FPLINE Pulse Width Register
    { 0x0038, 0x57 },   // LCD Vertical Display Height Register 0
    { 0x0039, 0x02 },   // LCD Vertical Display Height Register 1
    { 0x003A, 0x1B },   // LCD Vertical Non-Display Period Register
    { 0x003B, 0x0A },   // TFT FPFRAME Start Position Register
    { 0x003C, 0x01 },   // TFT FPFRAME Pulse Width Register

    { 0x0040, 0x05 },   // LCD Display Mode Register
    { 0x0041, 0x00 },   // LCD Miscellaneous Register
    { 0x0042, 0x00 },   // LCD Display Start Address Register 0
    { 0x0043, 0x00 },   // LCD Display Start Address Register 1
    { 0x0044, 0x00 },   // LCD Display Start Address Register 2
    { 0x0046, 0x20 },   // LCD Memory Address Offset Register 0
    { 0x0047, 0x03 },   // LCD Memory Address Offset Register 1
    { 0x0048, 0x00 },   // LCD Pixel Panning Register
    { 0x004A, 0x00 },   // LCD Display FIFO High Threshold Control Register
    { 0x004B, 0x00 },   // LCD Display FIFO Low Threshold Control Register

    { 0x0050, 0x00 },   // CRT/TV Horizontal Display Width Register
    { 0x0052, 0x00 },   // CRT/TV Horizontal Non-Display Period Register
    { 0x0053, 0x00 },   // CRT/TV HRTC Start Position Register
    { 0x0054, 0x00 },   // CRT/TV HRTC Pulse Width Register
    { 0x0056, 0x00 },   // CRT/TV Vertical Display Height Register 0
    { 0x0057, 0x00 },   // CRT/TV Vertical Display Height Register 1
    { 0x0058, 0x00 },   // CRT/TV Vertical Non-Display Period Register
    { 0x0059, 0x00 },   // CRT/TV VRTC Start Position Register
    { 0x005A, 0x00 },   // CRT/TV VRTC Pulse Width Register
    { 0x005B, 0x00 },   // TV Output Control Register
    
    { 0x0060, 0x00 },   // CRT/TV Display Mode Register
    { 0x0062, 0x00 },   // CRT/TV Display Start Address Register 0
    { 0x0063, 0x00 },   // CRT/TV Display Start Address Register 1
    { 0x0064, 0x00 },   // CRT/TV Display Start Address Register 2
    { 0x0066, 0x00 },   // CRT/TV Memory Address Offset Register 0
    { 0x0067, 0x00 },   // CRT/TV Memory Address Offset Register 1
    { 0x0068, 0x00 },   // CRT/TV Pixel Panning Register
    { 0x006A, 0x00 },   // CRT/TV Display FIFO High Threshold Control Register
    { 0x006B, 0x00 },   // CRT/TV Display FIFO Low Threshold Control Register

    { 0x0070, 0x00 },   // LCD Ink/Cursor Control Register
    { 0x0071, 0x00 },   // LCD Ink/Cursor Start Address Register
    { 0x0072, 0x00 },   // LCD Cursor X Position Register 0
    { 0x0073, 0x00 },   // LCD Cursor X Position Register 1
    { 0x0074, 0x00 },   // LCD Cursor Y Position Register 0
    { 0x0075, 0x00 },   // LCD Cursor Y Position Register 1
    { 0x0076, 0x00 },   // LCD Ink/Cursor Blue Color 0 Register
    { 0x0077, 0x00 },   // LCD Ink/Cursor Green Color 0 Register
    { 0x0078, 0x00 },   // LCD Ink/Cursor Red Color 0 Register
    { 0x007A, 0x00 },   // LCD Ink/Cursor Blue Color 1 Register
    { 0x007B, 0x00 },   // LCD Ink/Cursor Green Color 1 Register
    { 0x007C, 0x00 },   // LCD Ink/Cursor Red Color 1 Register
    { 0x007E, 0x00 },   // LCD Ink/Cursor FIFO Threshold Register

    { 0x0080, 0x00 },   // CRT/TV Ink/Cursor Control Register
    { 0x0081, 0x00 },   // CRT/TV Ink/Cursor Start Address Register
    { 0x0082, 0x00 },   // CRT/TV Cursor X Position Register 0
    { 0x0083, 0x00 },   // CRT/TV Cursor X Position Register 1
    { 0x0084, 0x00 },   // CRT/TV Cursor Y Position Register 0
    { 0x0085, 0x00 },   // CRT/TV Cursor Y Position Register 1
    { 0x0086, 0x00 },   // CRT/TV Ink/Cursor Blue Color 0 Register
    { 0x0087, 0x00 },   // CRT/TV Ink/Cursor Green Color 0 Register
    { 0x0088, 0x00 },   // CRT/TV Ink/Cursor Red Color 0 Register
    { 0x008A, 0x00 },   // CRT/TV Ink/Cursor Blue Color 1 Register
    { 0x008B, 0x00 },   // CRT/TV Ink/Cursor Green Color 1 Register
    { 0x008C, 0x00 },   // CRT/TV Ink/Cursor Red Color 1 Register
    { 0x008E, 0x00 },   // CRT/TV Ink/Cursor FIFO Threshold Register

    { 0x0100, 0x00 },   // BitBlt Control Register 0
    { 0x0101, 0x01 },   // BitBlt Control Register 1
    { 0x0102, 0x00 },   // BitBlt ROP Code/Color Expansion Register
    { 0x0103, 0x0C },   // BitBlt Operation Register
    { 0x0104, 0x00 },   // BitBlt Source Start Address Register 0
    { 0x0105, 0x00 },   // BitBlt Source Start Address Register 1
    { 0x0106, 0x00 },   // BitBlt Source Start Address Register 2
    { 0x0108, 0x28 },   // BitBlt Destination Start Address Register 0
    { 0x0109, 0xBD },   // BitBlt Destination Start Address Register 1
    { 0x010A, 0x06 },   // BitBlt Destination Start Address Register 2
    { 0x010C, 0x20 },   // BitBlt Memory Address Offset Register 0
    { 0x010D, 0x03 },   // BitBlt Memory Address Offset Register 1

    { 0x0110, 0x09 },   // BitBlt Width Register 0
    { 0x0111, 0x00 },   // BitBlt Width Register 1
    { 0x0112, 0x0b },   // BitBlt Height Register 0
    { 0x0113, 0x00 },   // BitBlt Height Register 1
    { 0x0114, 0x00 },   // BitBlt Background Color Register 0
    { 0x0115, 0x00 },   // BitBlt Background Color Register 1
    { 0x0118, 0x00 },   // BitBlt Foreground Color Register 0
    { 0x0119, 0x00 },   // BitBlt Foreground Color Register 1
    { 0x01E0, 0x00 },   // Look-Up Table Mode Register
    { 0x01E2, 0x00 },   // Look-Up Table Address Register
    { 0x01F0, 0x10 },   // Power Save Configuration Register
    { 0x01F1, 0x00 },   // Power Save Status Register
    { 0x01F4, 0x00 },   // CPU-to-Memory Access Watchdog Timer Register
    { 0x01FC, 0x01 }    // Display Mode Register
};

struct s1d13xxx_reginit init_netbookpro = {
	.values    = init_netbookpro_vals,
	.length    = ARRAY_SIZE(init_netbookpro_vals)
};

static struct s1dfb *save_fb;


static void
netbookpro_backlight_off(struct s1dfb *fb)
{
	unsigned int value = s1d13xxx_readreg(fb, 8);

	dbg("%s:\n", __FUNCTION__);
	
	s1d13xxx_writereg(fb, 8, value & 0xf9);
}

static void
netbookpro_backlight_on(struct s1dfb *fb, int level)
{
	unsigned int value = s1d13xxx_readreg(fb, 8);

	dbg("%s: fb=%p, level=%d\n", __FUNCTION__, fb, level);

	/* turning back light on with a value */
	if(level !=0)
		value = (level<<3) | (value & 0x07);

        /* enable backlight power */
	s1d13xxx_writereg(fb, 8, value | 2);
	udelay(5);

	/* set backlight enable and full brightness to strike tube */
	s1d13xxx_writereg(fb, 8, 0x00ff);
	udelay(200);

	/* set backlight enable and value */
	s1d13xxx_writereg(fb, 8, value | 6);
}

static int backlight_level = 31;

static void
netbookpro_backlight_set(struct s1dfb *fb, int level)
{
	unsigned int value = s1d13xxx_readreg(fb, 8);

	dbg("%s: fb=%p, level=%d\n", __FUNCTION__, fb, level);

	value &= 7;
	s1d13xxx_writereg(fb, 8, value | (level << 3));
}

static void
s1dfb_netbookrpo_blank(struct s1dfb *sdfb, int blank)
{
	if (blank)
		netbookpro_backlight_off(sdfb);
	else
		netbookpro_backlight_on(sdfb, backlight_level);
}

void
netbookpro_backlight_mod(int by)
{
	unsigned long flags;

	if (!save_fb)
		return;

	local_irq_save(flags);
	backlight_level += by;
	if (backlight_level < 0)
		backlight_level = 0;
	if (backlight_level > 31)
		backlight_level = 31;
	local_irq_restore(flags);

	netbookpro_backlight_on(save_fb, backlight_level);

}

/* 
 * procfs file entry 
 * 
 * create and handle driver/backlight
 *
 */

#ifdef CONFIG_PROC_FS

static struct proc_dir_entry *backlight_file;

static int proc_read_backlight(char *page, char **start,
			       off_t off, int count, 
			       int *eof, void *data)
{
	u16 value;
	int len;

	value = s1d13xxx_readreg(save_fb, 8);

	if((value & 0x6)==0)
	{
		value=0;
	}
	else
	{
		value=(value>>3)+1;
	}
	len = sprintf(page, "%d\n",value);

	return len;
}


static int proc_write_backlight(struct file *file,
				const char *buffer,
				unsigned long count, 
				void *data)
{
	char string[12] = {'\0'};
	int level=16;
	int value=s1d13xxx_readreg(save_fb, 8);

	if (count > sizeof(string) - 1)
		return -EINVAL;
	
	if (copy_from_user(string, buffer, count))
		return -EFAULT;
	
	string[count] = '\0';

	level=simple_strtoul(string, NULL, 0);

	/* level can be between 0 and 32 
	 * 0 is backlight off
	 * 1-32 is backlight level
	 */
	if(level < 0 ) level=0;
	if(level > 32) level=32;

	backlight_level = level;   /* save for later */

	if((value & 0x6)==0) {
		if(level > 0) {
			/* set value, enable inverter and backlight power */
			netbookpro_backlight_on(save_fb, level - 1);
		}
	} else {
		if(level==0) {
			/* turn inverter off and disable backlight power */
			netbookpro_backlight_off(save_fb);
			return count;
		}
		else {
			netbookpro_backlight_set(save_fb, level - 1);
		}
	}

	return count;
}

/* creates the proc file for backlight */
static int create_proc_file(const char *filename)
{
	backlight_file = create_proc_entry(filename, 0666, NULL);
	if(backlight_file == NULL) {
		printk(KERN_ERR "Failed to crate /proc/%s\n", filename);
		return -ENOMEM;
	}

	backlight_file->read_proc = proc_read_backlight;
	backlight_file->write_proc = proc_write_backlight;
	backlight_file->owner = THIS_MODULE;
		
	return 0;
}
#else
#define create_proc_file(x) 
#endif /* CONFIG_PROC_FS */

static void s1dfb_netbookpro_suspend(struct s1dfb *fb)
{
	netbookpro_backlight_off(fb);
}

static void s1dfb_netbookpro_resume(struct s1dfb *fb)
{
	netbookpro_backlight_on(fb, backlight_level);

}

static struct s1dfb_mach nbp_machine = {
	.name    = "netbookpro",
	.blank   = s1dfb_netbookrpo_blank,
	.suspend = s1dfb_netbookpro_suspend,
	.resume  = s1dfb_netbookpro_resume,
};

int
s1dfb_netbookpro_init(struct s1dfb *sdfb)
{
	printk(PFX "s1d13806: initialsing netbookpro settings\n");

	save_fb = sdfb;

	s1d13xxx_set_machine(sdfb, &nbp_machine);
	s1d13xxx_runinit(sdfb, &init_netbookpro);

	netbookpro_backlight_on(sdfb, 32-1);

	create_proc_file("driver/backlight");

	return 0;
}

