/* drivers/misc/netbookpro/nvram.c
 *
 * (c) 2004 Simtec Electronics
 *
 * Based (heavily) on CMOS/NV-RAM driver in drivers/char/nvram.c
*/

#define DEBUG
#define NVRAM_VERSION	"1.3"

#include <linux/types.h>
#include <linux/module.h>
#include <linux/config.h>
#include <linux/sched.h>
#include <linux/nvram.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>

#include <asm/uaccess.h>
#include <asm/semaphore.h>
#include <asm/arch/netbookpro.h>
#include <asm/arch/netbookpro-ioctl.h>

#define I2C_DRIVERID_NVRAM 0x0

#define EEPROM_ADDRESS (0xA6 >> 1)

static unsigned short ignore[] = { I2C_CLIENT_END };
static unsigned short force_addr[] = {
	ANY_I2C_BUS, EEPROM_ADDRESS, 0,
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

extern struct list_head misc_list;

#define NVRAM_BYTES (128)

// nvram flags
#define NVRAM_EXCL  (2)
#define NVRAM_WRITE (1)

static int nvram_open_cnt;
static int nvram_open_mode;
static struct i2c_client *nvram_client;

static DECLARE_MUTEX(nvram_lock);
static DECLARE_MUTEX(nvram_state_lock);

// we have a full 128 bytes of nvram to play with

static int touch_unlocked = 0;

#define NVRAM_ACCESS_READ	I2C_M_RD
#define NVRAM_ACCESS_WRITE	I2C_M_NOSTART

/*
 * nvram_access - low level EEPROM IO accessor
 * @client: i2c client
 * @buf: data buffer to read/write
 * @addr: EEPROM address (0 <= addr <= NVRAM_BYTES
 * @len: size of data length
 * @mode: %NVRAM_ACCESS_READ or %NVRAM_ACCESS_WRITE
 */
static int nvram_access(struct i2c_client *client, unsigned char *buf,
			unsigned char addr, size_t len, int mode)
{
	struct i2c_msg msgs[2];
	int ret;

	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &addr;

	msgs[1].addr = client->addr;
	msgs[1].flags = mode;
	msgs[1].len = len;
	msgs[1].buf = buf;

	ret = i2c_transfer(client->adapter, msgs, 2);

	return ret == 2 ? 0 : ret < 0 ? ret : -EIO;
}

static void nv_write_byte(unsigned char addr, unsigned char byte)
{
	if (addr >= EEPROM_NETBOOK2_BOOST_SECURITY_ADDRESS &&
	    addr < EEPROM_NETBOOK2_BOOST_SECURITY_ADDRESS +
		   EEPROM_NETBOOK2_BOOST_SECURITY_SIZE)
		return;

	if (addr >= EEPROM_NETBOOK2_TOUCH_CALIBRATION_DATA_ADDRESS &&
	    addr < EEPROM_NETBOOK2_TOUCH_CALIBRATION_DATA_ADDRESS +
		   EEPROM_NETBOOK2_TOUCH_CALIBRATION_DATA_SIZE &&
	    !touch_unlocked)
		return;

	nvram_access(nvram_client, &byte, addr, 1, NVRAM_ACCESS_WRITE);
}

/*
 * The are the file operation function for user access to /dev/nvram
 */

static long long
nvram_llseek(struct file *file, loff_t offset, int origin)
{
	pr_debug("nvram_llseek(%p, %ld, %d)\n", file, (long)offset, origin);

	switch (origin) {
	case 0:
		/* nothing to do */
		break;
	case 1:
		offset += file->f_pos;
		break;
	case 2:
		offset += NVRAM_BYTES;
		break;
	}
	return (offset >= 0) ? (file->f_pos = offset) : -EINVAL;
}



static ssize_t
nvram_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	unsigned char contents[NVRAM_BYTES];
        loff_t loc = *ppos;
	ssize_t ret;

	/* read *ppos for maximum size of nvram, or count bytes */

	down(&nvram_state_lock);
	if (!nvram_client) {
		ret = -ENXIO;
		goto out;
	}

	if (loc >= NVRAM_BYTES) {
		ret = 0;
		goto out;
	}

	if ((loc + count) > NVRAM_BYTES)
		count = NVRAM_BYTES - (unsigned int)loc;

	pr_debug("nvram_read(%p,%p,%ld,%ld)\n",
		 file, buf, (long)count, (long)loc);

	if (count == 0) {
		ret = 0;
		goto out;
	}

	memset(contents, 0xff, sizeof(contents));

	ret = nvram_access(nvram_client, contents, (unsigned char)loc,
			   count, NVRAM_ACCESS_READ);
	if (ret) {
		pr_debug("nvram_access: returned %d\n", ret);
		goto out;
	}

	if (copy_to_user(buf, contents, count))
		return -EFAULT;

	*ppos = loc + count;
	ret = count;

 out:
	up(&nvram_state_lock);

	return ret;
}

static ssize_t
nvram_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	unsigned char contents[NVRAM_BYTES];
	unsigned char *tmp;
        loff_t loc = *ppos;
	ssize_t ret;
	int i;

	down(&nvram_state_lock);
	if (!nvram_client) {
		ret = -ENXIO;
		goto out;
	}

	if (loc >= NVRAM_BYTES) {
		ret = 0;
		goto out;
	}

	if ((loc + count) > NVRAM_BYTES)
		count = NVRAM_BYTES - (unsigned int)loc;

	pr_debug("nvram_write(%p,%p,%d,%lld)\n", file, buf, count, loc);

	if (count == 0) {
		ret = 0;
		goto out;
	}

	if (copy_from_user(contents, buf, count)) {
		ret = -EFAULT;
		goto out;
	}

	for (i = loc, tmp = contents; count-- > 0; i++, tmp++)
		nv_write_byte(i, *tmp);

	touch_unlocked = 0;

	*ppos = i;
	ret = tmp - contents;

 out:
	up(&nvram_state_lock);

	return ret;
}


static int
nvram_ioctl(struct inode *inode, struct file *file,
	    unsigned int cmd, unsigned long arg)
{
	int addr, ret;

	pr_debug("nvram_ioctl(%p,%p,0x%x,0x%lx)\n",
		 inode, file, cmd, arg);

	if (!capable(CAP_SYS_ADMIN))
			return -EACCES;

	down(&nvram_state_lock);
	if (!nvram_client) {
		ret = -ENXIO;
		goto out;
	}

	switch (cmd) {
	case NVRAM_INIT:
		for (addr = 0; addr < NVRAM_BYTES; addr++)
			nv_write_byte(addr, 0);

		touch_unlocked = 0;
		ret = 0;
		break;

	case NVRAM_SETCKS:
		touch_unlocked = 0;
		ret = 0;
		break;

	case NVRAM_UNLOCK_TOUCH_CALIBRATION:
		touch_unlocked = 1;
		ret = 0;
		break;

	default:
		ret = -ENOTTY;
		break;
	}
 out:
	up(&nvram_state_lock);
	
	return ret;
}

static int nvram_open(struct inode *inode, struct file *file)
{
	int ret;

	pr_debug("nvram_open(%p,%p) (%d)\n", inode, file, file->f_flags);

	down(&nvram_state_lock);
	if (!nvram_client) {
		ret = -ENXIO;
		goto out;
	}

	if ((nvram_open_cnt && (file->f_flags & O_EXCL)) ||
	    (nvram_open_mode & NVRAM_EXCL) ||
	    ((file->f_mode & FMODE_WRITE) && (nvram_open_mode & NVRAM_WRITE))) {
		ret = -EBUSY;
		goto out;
	}

	if (file->f_flags & O_EXCL)
		nvram_open_mode |= NVRAM_EXCL;
	if (file->f_mode & FMODE_WRITE)
		nvram_open_mode |= NVRAM_WRITE;
	nvram_open_cnt++;
	ret = 0;

 out:
	up(&nvram_state_lock);

	return ret;
}


static int nvram_release(struct inode *inode, struct file *file)
{
	pr_debug("nvram_release(%p,%p)\n", inode, file);

	down(&nvram_state_lock);

	nvram_open_cnt--;

	/* if only one instance is open, clear the EXCL bit */
	if (nvram_open_mode & NVRAM_EXCL)
		nvram_open_mode &= ~NVRAM_EXCL;
	if (file->f_mode & 2)
		nvram_open_mode &= ~NVRAM_WRITE;

	up(&nvram_state_lock);

	return 0;
}


static struct file_operations nvram_fops = {
	.owner		= THIS_MODULE,
	.llseek		= nvram_llseek,
	.read		= nvram_read,
	.write		= nvram_write,
	.ioctl		= nvram_ioctl,
	.open		= nvram_open,
	.release	= nvram_release,
};

static struct miscdevice nvram_dev = {
	.minor	= NVRAM_MINOR,
	.name	= "nvram",
	.fops	= &nvram_fops
};

static struct i2c_driver nvram_driver;

static int nvram_detect(struct i2c_adapter *adapter, int address, int kind)
{
	int ret;

	down(&nvram_state_lock);
	if (nvram_client) {
		ret = -EBUSY;
		goto out;
	}

	nvram_client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL);
	if (!nvram_client) {
		ret = -ENOMEM;
		goto out;
	}

	memset(nvram_client, 0, sizeof(struct i2c_client));

	nvram_client->addr	= address;
	nvram_client->adapter	= adapter;
	nvram_client->driver	= &nvram_driver;
	strlcpy(nvram_client->name, "nvram", I2C_NAME_SIZE);

	ret = i2c_attach_client(nvram_client);
	if (ret) {
		printk(KERN_ERR "%s: failed to attach client (%d)\n",
		       __FUNCTION__, ret);
		goto free;
	}

	printk(KERN_INFO "I2C: NVRAM at bus address 0x%x\n", address);

	ret = misc_register(&nvram_dev);
	if (ret) {
		printk(KERN_ERR "nvram: can't misc_register on minor=%d\n",
		    NVRAM_MINOR);
		goto remove_i2c;
	}

#if 0
	if (!create_proc_read_entry("driver/nvram", 0, 0, nvram_read_proc,
		NULL)) {
		printk(KERN_ERR "nvram: can't create /proc/driver/nvram\n");
		ret = -ENOMEM;
		goto outmisc;
	}
#endif

	goto out;

#if 0
 outmisc:
	misc_deregister(&nvram_dev);
#endif
 remove_i2c:
	i2c_detach_client(nvram_client);
 free:
	kfree(nvram_client);
	nvram_client = NULL;
 out:
	up(&nvram_state_lock);
	return ret;
}

static int nvram_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_probe(adapter, &addr_data, nvram_detect);
}

static int nvram_detach_client(struct i2c_client *client)
{
	//remove_proc_entry("driver/nvram", 0);
	misc_deregister(&nvram_dev);

	down(&nvram_state_lock);
	i2c_detach_client(client);
	kfree(client);
	nvram_client = NULL;
	up(&nvram_state_lock);
	return 0;
}

static struct i2c_driver nvram_driver = {
	.owner		= THIS_MODULE,
	.name		= "nvram",
	.id		= I2C_DRIVERID_NVRAM,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= nvram_attach_adapter,
	.detach_client	= nvram_detach_client,
};

static int __init nvram_init(void)
{
	printk(KERN_INFO "Non-volatile memory driver v" NVRAM_VERSION "\n");
	return i2c_add_driver(&nvram_driver);
}

static void __exit nvram_exit(void)
{
	i2c_del_driver(&nvram_driver);
}

module_init(nvram_init);
module_exit(nvram_exit);

MODULE_LICENSE("GPL");
