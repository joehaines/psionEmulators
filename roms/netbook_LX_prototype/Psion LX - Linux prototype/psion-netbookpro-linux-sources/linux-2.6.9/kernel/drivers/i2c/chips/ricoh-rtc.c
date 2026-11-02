/* linux/drivers/i2c/chips/ricoh-rtc.c
 *
 * (c) 2004 Simtec Electronics, Ben Dooks <ben@simtec.co.uk>
 *
 *
 * Based on drivers/i2c/chips/rtc8564.c
 *
 * Driver for Ricoh RTC Chips
 *
 * This should support the R2051 series, and the RV5C386 which
 * are very similar chips.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/ioctl.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/bcd.h>

#include <linux/i2c.h>
#include <linux/i2c-sensor.h>

#include <linux/rtc.h>
#include <linux/file.h>
#include <linux/miscdevice.h>
#include <linux/init.h>

#include <asm/uaccess.h>

#include "ricoh-rtc.h"

struct ricoh_rtc {
	struct i2c_client     client;
	struct rtc_time       alarm;

	ricohrtc_type_t       type;
	const char            *name;
	unsigned char         hrmode;
	unsigned char         ctrl1;
	unsigned char         ctrl2;
};

static unsigned short ignore[] = { I2C_CLIENT_END };
static unsigned short force_addr[] = {
	ANY_I2C_BUS, 0x32, 0,
	I2C_CLIENT_END, I2C_CLIENT_END, I2C_CLIENT_END
};

static struct i2c_client_address_data addr_data = {
	.normal_i2c		= ignore,
	.normal_i2c_range	= ignore,
	.probe			= ignore,
	.probe_range		= ignore,
	.ignore			= ignore,
	.ignore_range		= ignore,
	.force			= force_addr,
};

#define I2C_DRIVERID_RICOHRTC (0x100)

static struct i2c_driver ricohrtc_driver;

/* misc device registration */

static struct ricoh_rtc *cur_rtc;

static int ricohrtc_ioctl(struct inode *inode, struct file *file,
			  unsigned int cmd, unsigned long arg);

static struct file_operations ricohrtc_fops = {
        .ioctl  = ricohrtc_ioctl,
};

static struct miscdevice ricohrtc_dev = {
        .minor       = RTC_MINOR,
        .name        = "rtc",
	.devfs_name  = "rtc",
        .fops        = &ricohrtc_fops,
	.dev         = NULL
};

/* register access code */

static int ricohrtc_read(struct ricoh_rtc *rtc, 
			 ricohrtc_reg_t reg,
			 unsigned char *buf,
			 unsigned int length)
{
	unsigned int ret;
	unsigned char addr;
	struct i2c_msg msgs[2] = {
		{ rtc->client.addr, 0, 1, &addr },
		{ rtc->client.addr, I2C_M_RD, length, buf }
	};

	addr = ((unsigned char)reg) << 4;
	
	if (rtc == NULL || buf == NULL) {
		pr_debug("%s: null argmuent\n", __FUNCTION__);
		return -EINVAL;
	}

	ret = i2c_transfer(rtc->client.adapter, msgs, 2);
	if (ret != 2) {
		pr_debug("%s: i2c_transfer() returned %d\n",
			 __FUNCTION__, ret);
		if (ret == 0)
			ret = 1;
		return ret;
	}

	return 0;
}

static int ricohrtc_write(struct ricoh_rtc *rtc, 
			  ricohrtc_reg_t reg,
			  unsigned char *buf,
			  unsigned int length)
{
	unsigned int ret;
	unsigned char addr;
	struct i2c_msg msgs[2] = {
		{ rtc->client.addr, 0, 1, &addr },
		{ rtc->client.addr, I2C_M_NOSTART, length, buf }
	};

	addr = ((unsigned char)reg) << 4;
	
	if (rtc == NULL || buf == NULL) {
		pr_debug("%s: null argmuent\n", __FUNCTION__);
		return -EINVAL;
	}

	ret = i2c_transfer(rtc->client.adapter, msgs, 2);
	if (ret != 2) {
		pr_debug("%s: i2c_transfer() returned %d\n",
			 __FUNCTION__, ret);

		if (ret == 0)
			ret = 1;
		return ret;
	}

	return 0;
}

static char *
ricohrtc_type_to_name(ricohrtc_type_t type)
{
	switch (type) {
	default:
	case RICOH_UNKNOWN:
		return "unknown";
	case RICOH_R2051:
		return "R2051";
	case RICOH_R5C386:
		return "R5C386";
	}

	return NULL;
}

/* i2c code */

static int ricohrtc_detach_client(struct i2c_client *client)
{
	i2c_detach_client(client);

	kfree(i2c_get_clientdata(client));
	return 0;
}

static int ricohrtc_detect(struct i2c_adapter *adapter, int address, int kind)
{
	struct ricoh_rtc  *rtc;
	unsigned char tmp;
	unsigned char tmp2;
	int err = 0, stopped = 0;

	pr_debug("%s: adapter=%p, address=%d, kind=%d\n",
		 __FUNCTION__, adapter, address, kind);

	rtc = kmalloc(sizeof(*rtc), GFP_KERNEL);
	if (rtc == NULL) {
		printk(KERN_ERR "no memory for new ricoh rtc\n");
		err = -ENOMEM;
		goto exit;
	}

	memset(rtc, 0, sizeof(*rtc));

	rtc->client.addr    = address;
	rtc->client.adapter = adapter;
	rtc->client.driver  = &ricohrtc_driver;
	rtc->client.flags   = 0;

	strlcpy(rtc->client.name, "ricoh-rtc", I2C_NAME_SIZE);

	i2c_set_clientdata(&rtc->client, rtc);

	/* ok, we'll check to see if we can read both the registers */

	if (ricohrtc_read(rtc, RICOHRTC_REG_Control1, &rtc->ctrl1, 1) != 0) {
		printk(KERN_ERR "%s: cannot ready RICOHRTC_REG_Control1\n",
		       __FUNCTION__);
		err = -ENOENT;
		goto exit_free;
	}

	if (ricohrtc_read(rtc, RICOHRTC_REG_Control2, &rtc->ctrl2, 1) != 0) {
		printk(KERN_ERR "%s: cannot ready RICOHRTC_REG_Control2\n",
		       __FUNCTION__);
		err = -ENOENT;
		goto exit_free;
	}

	pr_debug("%s: ctrl1=%02x, ctrl2=%02x\n",
		 __FUNCTION__, rtc->ctrl1, rtc->ctrl2);

	/*
	 * Only the RV5C386 allows bit 7 to be set to one.
	 */
	if (ricohrtc_read(rtc, RICOHRTC_REG_OscillatorAdj, &tmp, 1) != 0) {
		printk(KERN_ERR "%s: cannot read OscillatorAdj\n",
		       __FUNCTION__);
		err = -ENOENT;
		goto exit_free;
	}

	if ((tmp & 0x80) == 0) {
		tmp2 = tmp | 0x80;

		if (ricohrtc_write(rtc, RICOHRTC_REG_OscillatorAdj, &tmp2, 1) != 0) {
			printk(KERN_ERR "%s: cannot read OscillatorAdj\n",
			       __FUNCTION__);
			err = -ENOENT;
			goto exit_free;
		}

		if (ricohrtc_read(rtc, RICOHRTC_REG_OscillatorAdj, &tmp, 1) != 0) {
			printk(KERN_ERR "%s: cannot read OscillatorAdj\n",
			       __FUNCTION__);
			err = -ENOENT;
			goto exit_free;
		}

		/* put the oscillator adjust register back where it was */
		tmp2 ^= 0x80;

		if (ricohrtc_write(rtc, RICOHRTC_REG_OscillatorAdj, &tmp2, 1) != 0) {
			printk(KERN_ERR "%s: cannot ready OscillatorAdj\n",
			       __FUNCTION__);
			err = -ENOENT;
			goto exit_free;
		}
	}

	if (tmp & 0x80) {
		rtc->type = RICOH_R5C386;
	} else {
		rtc->type = RICOH_R2051;
	}

	/* work out if this is a 12 or 24hr mode */

	if (rtc->ctrl1 & RICOHRTC_CTRL1_24HR)
		rtc->hrmode = 24;
	else
		rtc->hrmode = 12;

	/* check to see if the crystal or oscillator stopped? */

	switch (rtc->type) {
	case RICOH_R2051:
		stopped = !(rtc->ctrl2 & R2051RTC_CTRL2_XSTP);
		if (stopped) {
			rtc->ctrl2 |= R2051RTC_CTRL2_XSTP;
			ricohrtc_write(rtc, RICOHRTC_REG_Control2,
				       &rtc->ctrl2, 1);
		}
		break;

	case RICOH_R5C386:
		stopped = rtc->ctrl2 & RV386RTC_CTRL2_XSTP;
		if (stopped) {
			rtc->ctrl2 &= ~RV386RTC_CTRL2_XSTP;
			ricohrtc_write(rtc, RICOHRTC_REG_Control2,
				       &rtc->ctrl2, 1);
		}
		break;

	default:;
	}


	/* register our device */

	if ((err = i2c_attach_client(&rtc->client))) {
		printk(KERN_ERR "%s: failed to attach client (%d)\n",
		       __FUNCTION__, err);
		goto exit_free;
	}

	if (cur_rtc == NULL) {
		cur_rtc = rtc;

		misc_register(&ricohrtc_dev);
	}

	rtc->name = ricohrtc_type_to_name(rtc->type);
	printk(KERN_INFO "I2C: Ricoh %s RTC, %dHr mode%s\n",
	       rtc->name, rtc->hrmode,
	       stopped ? ", oscillator restarted" : "");

	return 0;

 exit_free:
	kfree(rtc);
 exit:
	printk(KERN_ERR "%s: exiting with error %d\n", __FUNCTION__, err);
	return err;
}

/* get/set time routines */

static int
ricohrtc_get_datetime(struct ricoh_rtc *rtc,
		      struct rtc_time *dt)
{
	unsigned char buf[15];
	int ret = -EIO;

	if (rtc == NULL || dt == NULL) {
		pr_debug("%s: invalid parameters\n", __FUNCTION__);
		return -EINVAL;
	}

	memset(buf, 0, sizeof(buf));
	memset(dt, 0, sizeof(*dt));

	ret = ricohrtc_read(rtc, 0, buf, RICOHRTC_REG_Years+1);
	if (ret)
		return ret;

	/* century stored in minute alarm reg */
	dt->tm_year  = BCD2BIN(buf[RICOHRTC_REG_Years]);
	dt->tm_mday  = BCD2BIN(buf[RICOHRTC_REG_DayOfMonth] & 0x3f);
	dt->tm_wday  = BCD2BIN(buf[RICOHRTC_REG_DayOfWeek] & 7);
	dt->tm_mon   = BCD2BIN(buf[RICOHRTC_REG_Months] & 0x1f) - 1;
	dt->tm_sec   = BCD2BIN(buf[RICOHRTC_REG_Seconds] & 0x7f);
	dt->tm_min   = BCD2BIN(buf[RICOHRTC_REG_Minutes] & 0x7f);
	dt->tm_hour  = BCD2BIN(buf[RICOHRTC_REG_Hours] & 0x3f);

	/*
	 * Convert the "binary" 12 hour time into 24 hour time.
	 */
	if (rtc->hrmode == 12) {
		if (dt->tm_hour == 12 || dt->tm_hour == 32)
			dt->tm_hour -= 12;
		if (dt->tm_hour >= 20)
			dt->tm_hour -= 20 - 12;
	}

	if (buf[RICOHRTC_REG_Months] & RICOHRTC_REG_Months_Y2K)
		dt->tm_year += 100;

	pr_debug("%s: %d,%d,%d %d(%d)/%d/%d\n",
		 __FUNCTION__,
		 dt->tm_hour, dt->tm_min, dt->tm_sec,
		 dt->tm_mday, dt->tm_wday, dt->tm_mon, dt->tm_year);

	return 0;
}

static int
ricohrtc_set_datetime(struct ricoh_rtc *rtc,
		      struct rtc_time *dt,
		      int datetoo)
{
	unsigned char buf[RICOHRTC_REG_OscillatorAdj];
	unsigned int hr;
	int ret;
	int len = RICOHRTC_REG_Hours;

	pr_debug("%s: %d,%d,%d %d(%d)/%d/%d\n",
		 __FUNCTION__,
		 dt->tm_hour, dt->tm_min, dt->tm_sec,
		 dt->tm_mday, dt->tm_wday, dt->tm_mon, dt->tm_year);

	hr = dt->tm_hour;

	/*
	 * If we're running in 12 hour mode, convert to 12 hour time.
	 * Note: this is designed to come out correctly once the
	 * BCD conversion is done.
	 */
	if (rtc->hrmode == 12) {
		if (hr >= 12)
			hr += 20 - 12;
		if (hr == 0 || hr == 20)
			hr += 12;
	}

	buf[RICOHRTC_REG_Seconds] = BIN2BCD(dt->tm_sec);
	buf[RICOHRTC_REG_Minutes] = BIN2BCD(dt->tm_min);
	buf[RICOHRTC_REG_Hours]   = BIN2BCD(hr);

	if (datetoo) {
		len = RICOHRTC_REG_Years;
		buf[RICOHRTC_REG_DayOfMonth] = BIN2BCD(dt->tm_mday);
		buf[RICOHRTC_REG_DayOfWeek]  = BIN2BCD(dt->tm_wday);
		buf[RICOHRTC_REG_Months]     = BIN2BCD(dt->tm_mon + 1) & 0x1f;

		buf[RICOHRTC_REG_Years]      = BIN2BCD(dt->tm_year % 100);

		if (dt->tm_year >= 2000 - 1900)
			buf[RICOHRTC_REG_Months] |= RICOHRTC_REG_Months_Y2K;
	}

	ret = ricohrtc_write(rtc, RICOHRTC_REG_Seconds, buf, len+1);
	if (ret != 0) 
		ret = -EIO;

	return ret;
}

static int
ricohrtc_set_alarm(struct ricoh_rtc *rtc, struct rtc_time *tm)
{
	memcpy(&rtc->alarm, tm, sizeof(struct rtc_time));
	return 0;
}

/* ioctl interface for rtc */

static int ricohrtc_ioctl(struct inode *inode,
			  struct file *file,
			  unsigned int cmd,
			  unsigned long arg)
{
	struct ricoh_rtc *rtc = cur_rtc;
	struct rtc_time rtctm;
	int ret;

	pr_debug("ioctl(%p,%p,0x%08x,%ld)\n", inode, file, cmd, arg);

	switch (cmd) {
	case RTC_AIE_OFF:
		memset(&rtctm, 0, sizeof(struct rtc_time));
		nbp_pcon_rtc_wakeup(&rtctm);
		return 0;

	case RTC_AIE_ON:
		nbp_pcon_rtc_wakeup(&rtc->alarm);
		return 0;

	case RTC_ALM_READ:
		ret = copy_to_user((void *)arg, &rtc->alarm, sizeof(rtctm));
		return ret ? -EFAULT : 0;

	case RTC_ALM_SET:
		if (copy_from_user(&rtctm, (void *)arg, sizeof(rtctm)))
			return -EFAULT;
		return ricohrtc_set_alarm(rtc, &rtctm);

	case RTC_RD_TIME:
		ricohrtc_get_datetime(rtc, &rtctm);
		ret = copy_to_user((void *)arg, &rtctm, sizeof(rtctm));
		return ret ? -EFAULT : 0;

	case RTC_SET_TIME:
		if (!capable(CAP_SYS_TIME))
			return -EACCES;

		if (copy_from_user(&rtctm, (void *)arg, sizeof(rtctm)))
			return -EFAULT;

		return ricohrtc_set_datetime(rtc, &rtctm, 1);

	case RTC_EPOCH_READ:
		return put_user(1900, (unsigned long *)arg);
	}
	
	return -EINVAL;
}

/*
 * Return current RTC seconds since 1970-01-01 00:00:00, the standard
 * Unix Epoch.
 */
static unsigned long ricohrtc_time(struct i2c_client *client)
{
	struct ricoh_rtc *rtc = i2c_get_clientdata(client);
	struct rtc_time rtctm;

	ricohrtc_get_datetime(rtc, &rtctm);

	return mktime(rtctm.tm_year + 1900, rtctm.tm_mon + 1, rtctm.tm_mday,
		      rtctm.tm_hour, rtctm.tm_min, rtctm.tm_sec);
}


static int ricohrtc_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_probe(adapter, &addr_data, ricohrtc_detect);
}

static unsigned long ricohrtc_delta;

extern unsigned long save_time_delta(unsigned long);
extern void restore_time_delta(unsigned long, unsigned long);

static int ricohrtc_suspend(struct i2c_client *client, u32 state)
{
	ricohrtc_delta = save_time_delta(ricohrtc_time(client));
	return 0;
}

static int ricohrtc_resume(struct i2c_client *client)
{
	restore_time_delta(ricohrtc_time(client), ricohrtc_delta);
	return 0;
}

static struct i2c_driver ricohrtc_driver = {
	.owner                  = THIS_MODULE,
	.name                   = "ricoh-rtc",
	.id                     = I2C_DRIVERID_RICOHRTC,
	.flags 	                = I2C_DF_NOTIFY,
	.attach_adapter	        = ricohrtc_attach_adapter,
	.detach_client	        = ricohrtc_detach_client,
	.suspend		= ricohrtc_suspend,
	.resume			= ricohrtc_resume,
};

/* module initialisation and finalisation, and definitions */

static int __init ricohrtc_init(void)
{
	printk("Ricoh RTC Driver, (c) 2004 Simtec Electronics\n");

	pr_debug("%s: $Id: ricoh-rtc.c,v 1.4 2004/08/27 12:09:02 ben Exp $\n", __FUNCTION__);
	return i2c_add_driver(&ricohrtc_driver);
}

static void __exit ricohrtc_exit(void)
{
	i2c_del_driver(&ricohrtc_driver);
}

MODULE_AUTHOR("Ben Dooks <ben@simtec.co.uk>");
MODULE_DESCRIPTION("Ricoh RTC Drivers");
MODULE_LICENSE("GPL");

module_init(ricohrtc_init);
module_exit(ricohrtc_exit);
