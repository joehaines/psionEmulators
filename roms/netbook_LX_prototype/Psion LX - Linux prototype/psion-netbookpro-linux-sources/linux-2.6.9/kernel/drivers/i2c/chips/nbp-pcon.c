/*
 *  linux/drivers/i2c/chips/nbp-pcon.c
 *
 *  Driver for Psion Teklogix NetbookPro PCON chip
 *  Copyright (C) 2004 Simtec Electronics
 *
 *  based on linux/drivers/acorn/char/pcf8583.c
 *  Copyright (C) 2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
/* This driver provides the low-level i2c sending for the NetBookPro's
 * power-management and general support processor. This allows other parts
 * of the system to send messages to the chip for processing (including 
 * the userland)
 *
 * Note, this code does not provide the i2c client facilities needed by
 * the PCon to send data to the systems that need information feeding in.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioctl.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/rtc.h>

#include <asm/arch/netbookpro.h>
#include <asm/arch/netbookpro-pcon.h>

struct pcon_client {
	struct i2c_client      client;
	int                    thr_die;
	wait_queue_head_t      thr_wait;
	wait_queue_head_t      thr_sleep;
	struct completion      thr_complete;
	void                   *pcon_dev;
};

/* pcon_cmd_ent
 *
 * command entry for the command queue 
*/

struct pcon_cmd_ent {
	struct pcon_cmd_ent	*next;
	struct completion	done;

	unsigned int		flags;

#define PCON_FLG__ASYNC		(1 << 0)	/* defined elsewhere */
#define PCON_FLG_CHKRDY		(1 << 1)
#define PCON_FLG_RECVMSG	(1 << 2)

	int			sendlen;
	int			error;
	unsigned char		sendmsg[16];
	unsigned char		recvmsg[1];
};

static struct pcon_cmd_ent *queue;
static struct pcon_client *cur_client;

#define PFX "pcon: "

#if 0
#define dbg(x...) do { if (debug) { printk(KERN_INFO ""  x); } } while(0)
#else
#define dbg(x...) do { if (debug) { printk(KERN_DEBUG ""  x); } } while(0)
#endif

#if 0
#define dbgcmd(x...) do { if (debug) { printk(KERN_INFO ""  x); } } while(0)
#else
#define dbgcmd(x...) do { if (debug) { printk(KERN_DEBUG ""  x); } } while(0)
#endif

#if 0
#define dbgrdy(x...) do { if (debug) { printk(KERN_INFO ""  x); } } while(0)
#else
#define dbgrdy(x...) do { if (debug) { printk(KERN_DEBUG ""  x); } } while(0)
#endif

#if 0
#undef dbgrdy
#undef dbgcmd
#undef dbg

extern void printascii(const char *);
#define dbg(x...) do { char buff[200]; printascii(PFX); sprintf(buff, x); printascii(buff); } while(0)
#define dbgrdy(x...) dbg(x)
#define dbgcmd(x...) dbg(x)
#endif

static int debug = 0;
//static int debug = 1;
MODULE_PARM(debug, "i");

static struct i2c_driver pcon_driver;

#define PCON_I2C_ADDR (0x26 >> 1)

/* we cannot allow the pcon to be `probed` as it seems to manage to
 * make it reset the system shortly afterwards...
*/

static unsigned short ignore[]      = { I2C_CLIENT_END };
static unsigned short normal_addr[] = { ANY_I2C_BUS, PCON_I2C_ADDR, 0,
					I2C_CLIENT_END, I2C_CLIENT_END, I2C_CLIENT_END };

static struct i2c_client_address_data addr_data = {
	.normal_i2c		= ignore,
	.normal_i2c_range	= ignore,
	.probe			= ignore,
	.probe_range		= ignore,
	.ignore			= ignore,
	.ignore_range		= ignore,
	.force			= normal_addr,
};

static int
pcon_do_read(struct pcon_client *client, int loc, unsigned char *result)
{
	unsigned char loc_data[1];
	struct i2c_msg msgs[2] = {
		{ client->client.addr,        0, 1, loc_data },
		{ client->client.addr, I2C_M_RD, 1, result }
	};

	dbgrdy("pcon_do_read: client=%p, loc=%d, result=%p\n",
	       client, loc, result);

	loc_data[0] = loc;

	return i2c_transfer(client->client.adapter, msgs, 2); 
}

static int
pcon_send(struct pcon_client *client, char *data, int length)
{
	struct i2c_msg msgs[1] = {
		{ client->client.addr, 0, length, data },
	};
	int ret;

	ret = i2c_transfer(client->client.adapter, msgs, 1); 
	
	return (ret == 1) ? 1 : 0;
}

static int
pcon_check_ready(struct pcon_client *client)
{
	unsigned char inbuff;
	int ret;

	dbgrdy("%s: called (%ld)\n", __FUNCTION__, jiffies); 

	while (1) {
		/* the sfi code places this in the way to make sure that the
		 * friendly pcon isn't going to ack us... dunno if this is
		 * going to help us or not with our problems with reboot
		 * after suspend...
		 *
		 * yep, this delay fixes the problem.
		 */
		msleep(100);

		ret = pcon_do_read(client, NETBOOKPRO_PCON_COMMAND_ADDR,
				   &inbuff);
	
		dbgrdy("pcon_check_ready: ret=%d, inbuff=0x%02x\n", ret, inbuff);

		if (ret == 2 && inbuff == PCON_CMD_Ready) {
			dbgrdy("%s: pcon ready (%ld)\n", __FUNCTION__, jiffies); 
			return 0;
		}

		/* ok, we didn't get an ok response, let us 
		 * wait for a little while and try again */

		dbgrdy("%s: pcon not ready (%ld)\n", __FUNCTION__, jiffies); 
		
		msleep(100);
	}

	return 0;
}

static int pcon_thread(void *param)
{
	struct pcon_client *pc = (struct pcon_client *)param;
	unsigned long flags;
	int ret;

	dbg("pcon_thread: param=%p, pc=%p\n", param,pc);

	daemonize("nbp-pcon");
	current->flags |= PF_NOFREEZE;

	while (1) {
		struct pcon_cmd_ent *cmd;

		wait_event_interruptible(pc->thr_wait, pc->thr_die || queue);

		if (pc->thr_die)
			break;

		/*
		 * remove one item from the queue, and execute it
		 */
		local_irq_save(flags);
		cmd = queue;
		if (cmd != NULL)
			queue = cmd->next;
		local_irq_restore(flags);

		if (!cmd)
			continue;

		dbg("%s: sendmsg %p: %x %02x %02x %02x...\n",
		    __FUNCTION__, cmd, cmd->flags,
		    cmd->sendmsg[0], cmd->sendmsg[1], cmd->sendmsg[2]);

		if (cmd->flags & PCON_FLG_CHKRDY)
			pcon_check_ready(pc);

		if (cmd->flags & PCON_FLG_RECVMSG)
			ret = pcon_do_read(pc, cmd->sendmsg[0], cmd->recvmsg);
		else
			ret = pcon_send(pc, cmd->sendmsg, cmd->sendlen);

		cmd->error = ret;

		if ((cmd->flags & PCON_FLG_ASYNC) == 0) {
			dbg("%s: completing command\n", __FUNCTION__);
			complete(&cmd->done);
		} else {
			dbg("%s: freeing command\n", __FUNCTION__);
			kfree(cmd);
		}
	}

	printk(KERN_DEBUG PFX "thread exiting\n");

	complete_and_exit (&pc->thr_complete, 0);
}

static int pcon_attach(struct i2c_adapter *adap, int addr, int kind)
{
	struct pcon_client *pc;
	int ret;

	dbg("pcon_attach: adap=%p, addr=%02x, kind=%d\n", adap, addr, kind);

	if (addr != PCON_I2C_ADDR) {
		printk("pcon_attach: unknown i2c address 0x%02x\n", addr);
		return -ENOENT;
	}

	pc = kmalloc(sizeof(*pc), GFP_KERNEL);

	dbg("pcon_attach: new client at %p\n", pc);

	if (pc == NULL) {
		printk(KERN_ERR ": no memory for pcon client data\n");
		ret = -ENOMEM;
		goto done;
	}

	memset(pc, 0, sizeof(*pc));

	strlcpy(pc->client.name, "PCON", I2C_NAME_SIZE);
	i2c_set_clientdata(&pc->client, pc);

	pc->client.id      = pcon_driver.id;
	pc->client.flags   = I2C_CLIENT_ALLOW_USE | I2C_DF_NOTIFY;
	pc->client.addr    = addr;
	pc->client.adapter = adap;
	pc->client.driver  = &pcon_driver;

	init_waitqueue_head(&pc->thr_wait);
	init_waitqueue_head(&pc->thr_sleep);
	init_completion(&pc->thr_complete);

	dbg("pcon_attach: attaching new client\n");
	ret = i2c_attach_client(&pc->client);
	dbg("pcon_attach: ret=%d\n", ret);
	if (ret) {
		printk(KERN_ERR "cannot attach client: %d\n", ret);
		goto done;
	}

	cur_client = pc;

	ret = kernel_thread(pcon_thread, pc, CLONE_KERNEL);
	if (ret < 0) {
		printk(KERN_ERR "cannot start thread: %d\n", ret);
		i2c_detach_client(&pc->client);
		goto done;
	}

	dbg("pcon_attach: thread pid is %d\n", ret);

	if (1) {
		printk(KERN_INFO "Moving power state to Run\n");
		nbp_pcon_write2(NETBOOKPRO_PCON_POWER,
				PCON_PWRSTATE_Run);
	}
	pc->pcon_dev = pcon_dev_add("pcon", &pc->client.dev, pc);

done:
	if (ret < 0)
		kfree(pc);

	return ret;
}

static int pcon_probe(struct i2c_adapter *adap)
{
	return i2c_probe(adap, &addr_data, pcon_attach);
}

static int pcon_detach(struct i2c_client *client)
{
	struct pcon_client *pc = i2c_get_clientdata(client);
	int ret = i2c_detach_client(client);

	if (ret) {
		dev_err(&client->dev, "Client deregistration failed, "
			"client not detached.\n");
	} else {
		pcon_dev_remove(pc->pcon_dev);
		pc->thr_die = 1;
		wake_up(&pc->thr_wait);
		wait_for_completion(&pc->thr_complete);
		kfree(pc);
	}

	return ret;
}

static int pcon_suspend(struct i2c_client *client, u32 state)
{
	struct pcon_client *pc = i2c_get_clientdata(client);
	int ret = 0;
	if (pc)
		ret = pcon_dev_suspend(pc->pcon_dev, state);
	return ret;
}

static int pcon_resume(struct i2c_client *client)
{
	struct pcon_client *pc = i2c_get_clientdata(client);
	int ret = 0;
	if (pc)
		ret = pcon_dev_resume(pc->pcon_dev);
	return ret;
}

static int
pcon_command(struct pcon_client *pc, unsigned int flags, unsigned char *msg,
	     int len, int *result)
{
	struct pcon_cmd_ent *ent;
	unsigned long irq_flags;
	int ret = 0;

	ent = kmalloc(sizeof(*ent), (flags & PCON_FLG_ASYNC)
		? GFP_ATOMIC : GFP_KERNEL);
	if (ent == NULL) {
		dbg("no memory to allocate command entity\n");
		printk(KERN_ERR "%s: no memory for command\n", __FUNCTION__);
		return -ENOMEM;
	}

	dbg("%s: got new block %p\n", __FUNCTION__, ent);

	init_completion(&ent->done);
	ent->next    = NULL;
	ent->flags   = flags;
	ent->sendlen = len;
	memcpy(ent->sendmsg, msg, len);

	dbg("%s: adding %p to queue of commands\n", __FUNCTION__, ent);

	local_irq_save(irq_flags);
	
	if (queue == NULL) {
		queue = ent;
	} else {
		struct pcon_cmd_ent *ptr = queue;

		for (; ptr->next != NULL; ptr = ptr->next);

		ptr->next = ent;
	}

	local_irq_restore(irq_flags);

	dbg("%s: checking if we need to wakeup for %p\n", __FUNCTION__, ent);

	if (pc != NULL) {
		dbg("%s: waking client...\n", __FUNCTION__);
		wake_up(&pc->thr_wait);
	}

	if (flags & PCON_FLG_ASYNC) {
		// nothing to do, leave it to it.
	} else {
		wait_for_completion(&ent->done);

		if (flags & PCON_FLG_RECVMSG) {
			if (ent->error == 2) {
				if (result)
					*result = ent->recvmsg ? 1 : 0;
				ret = 0;
			} else
				ret = ent->error;
		}

		dbg("%s: freeing block %p\n", __FUNCTION__, ent);
		/* dump this, we've been left with it*/
		kfree(ent);
	}

	dbg("%s: done\n", __FUNCTION__);

	return ret;
}

int
nbp_pcon_command(unsigned long command, unsigned int value,
		 unsigned int flags, int *result)
{
	unsigned char sendmsg[4];
	int sendlen;

	flags &= PCON_FLG_ASYNC;
	if (flags != PCON_FLG_ASYNC)
		might_sleep();

	/*
	 * Decode the command into the message we need to send.
	 */
	switch (command) {
	case I2C_SETREDLED:
		flags |= PCON_FLG_CHKRDY;
		sendlen = 3;
		sendmsg[0] = NETBOOKPRO_PCON_COMMAND_ADDR;
		sendmsg[1] = PCON_CMD_SetRedLED;
		sendmsg[2] = value ? 1 : 0;
		break;

	case I2C_SETSERIALPOWER:
		flags |= PCON_FLG_CHKRDY;
		sendlen = 3;
		sendmsg[0] = NETBOOKPRO_PCON_COMMAND_ADDR;
		sendmsg[1] = PCON_CMD_SetSerialPower;
		sendmsg[2] = value ? 1 : 0;
		break;

	case I2C_SETMMCPOWER:
		flags |= PCON_FLG_CHKRDY;
		sendlen = 3;
		sendmsg[0] = NETBOOKPRO_PCON_COMMAND_ADDR;
		sendmsg[1] = PCON_CMD_SetMMCPower;
		sendmsg[2] = value ? 1 : 0;
		break;

	case I2C_SETVMODEMPOWER:
		flags |= PCON_FLG_CHKRDY;
		sendlen = 3;
		sendmsg[0] = NETBOOKPRO_PCON_COMMAND_ADDR;
		sendmsg[1] = PCON_CMD_SetModemPower;
		sendmsg[2] = value ? 1 : 0;
		break;

	case I2C_SETUSBHOSTPOWER:
		flags |= PCON_FLG_CHKRDY;
		sendlen = 3;
		sendmsg[0] = NETBOOKPRO_PCON_COMMAND_ADDR;
		sendmsg[1] = PCON_CMD_SetUSBPower;
		sendmsg[2] = value ? 1 : 0;
		break;

	case PCON_MONITORBATTERY:
		flags |= PCON_FLG_CHKRDY;
		sendlen = 3;
		sendmsg[0] = NETBOOKPRO_PCON_COMMAND_ADDR;
		sendmsg[1] = PCON_CMD_BatteryMonitor;
		sendmsg[2] = value ? 1 : 0;
		break;

	case PCON_KEYBOARD_CMD:
		sendlen = 2;
		sendmsg[0] = NETBOOKPRO_PCON_KEYBOARD_CMD_ADDR;
		sendmsg[1] = value;
		break;

	case PCON_POWER_STATE:
		sendlen = 2;
		sendmsg[0] = NETBOOKPRO_PCON_POWER;
		sendmsg[1] = value;
		break;

	case I2C_SETPCMCIAPOWER:
		sendlen = 2;
		sendmsg[0] = NETBOOKPRO_PCON_POWER_PCMCIA;
		sendmsg[1] = value ? 1 : 0;
		break;

	case I2C_SETCFPOWER:
		sendlen = 2;
		sendmsg[0] = NETBOOKPRO_PCON_POWER_CF;
		sendmsg[1] = value ? 1 : 0;
		break;

	/*
	 * These two aren't currently used.
	 */
	case I2C_GETPCMCIAPOWER:
		flags |= PCON_FLG_RECVMSG;
		sendlen = 1;
		sendmsg[0] = NETBOOKPRO_PCON_POWER_PCMCIA;
		break;

	case I2C_GETCFPOWER:
		flags |= PCON_FLG_RECVMSG;
		sendlen = 1;
		sendmsg[0] = NETBOOKPRO_PCON_POWER_CF;
		break;

	default:
		printk(KERN_ERR "%s: uknown set command %lu\n",
		       __FUNCTION__, command);
		return -EINVAL;
	}

	return pcon_command(cur_client, flags, sendmsg, sendlen, result);
}
EXPORT_SYMBOL(nbp_pcon_command);

int nbp_pcon_rtc_wakeup(struct rtc_time *tm)
{
	unsigned char sendmsg[10];

	sendmsg[0] = NETBOOKPRO_PCON_COMMAND_ADDR;
	sendmsg[1] = PCON_CMD_SetWakeupAlarm;
	sendmsg[2] = tm->tm_sec;
	sendmsg[3] = tm->tm_min;
	sendmsg[4] = tm->tm_hour;
	sendmsg[5] = tm->tm_wday; /* 6 = Saturday */
	sendmsg[6] = tm->tm_mday; /* 1 = 1st */
	sendmsg[7] = tm->tm_mon;
	sendmsg[8] = tm->tm_year >> 8;
	sendmsg[9] = tm->tm_year & 255;

	return pcon_command(cur_client, PCON_FLG_CHKRDY, sendmsg, 10, NULL);
}
EXPORT_SYMBOL(nbp_pcon_rtc_wakeup);

int
nbp_pcon_write2(unsigned int to, unsigned int val)
{
	if (cur_client != NULL) {
		char data[2];

		data[0] = to;
		data[1] = val;
	
		pcon_send(cur_client, data, 2);
	} else {
		printk(KERN_ERR "%s: no client for write(%d,%d)\n",
		       __FUNCTION__, to, val);
	}

	return 0;
}


static struct i2c_driver pcon_driver = {
	.owner		= THIS_MODULE,
	.name		= "PCON",
	.id		= I2C_DRIVERID_EXP3,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter = pcon_probe,
	.detach_client	= pcon_detach,
	.suspend	= pcon_suspend,
	.resume		= pcon_resume,
};

static __init int pcon_init(void)
{
	return i2c_add_driver(&pcon_driver);
}

static __exit void pcon_exit(void)
{
	i2c_del_driver(&pcon_driver);
}

MODULE_AUTHOR("Ben Dooks <ben@simtec.co.uk>");
MODULE_DESCRIPTION("Psion NetbookPro PCon chip driver");
MODULE_LICENSE("GPL");

module_init(pcon_init);
module_exit(pcon_exit);
