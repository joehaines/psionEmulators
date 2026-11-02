/* drivers/input/keyboard/netbookpro.c
 *
 * (c) 2004 Simtec Electronics
 * Written by Ben Dooks
 * 
 * $Id: netbookpro.c,v 1.10 2004/09/14 20:25:30 rmk Exp $
*/

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-pxa.h>

#include <asm/apm.h>
#include <asm/arch/netbookpro.h>
#include <asm/arch/netbookpro-pcon.h>

/* things that come from the framebuffer driver */

extern void netbookpro_backlight_mod(int by);

#define PFX "nbp-keyb: "

static int debug;

#define DBG(x...)				\
	do {					\
		if (debug)			\
			pr_debug(PFX x);	\
	} while (0)

/* keyboard input device registration */

static struct input_dev kbd_dev;

/* keyboard mapping code */

#define NBP_NUMKEYCODES (0x80)

static unsigned char normal_map[NBP_NUMKEYCODES] = {
	[0x5b]  = KEY_ESC,
	[0x5c]  = KEY_1,
	[0x5d]  = KEY_2,
	[0x47]  = KEY_3,
	[0x36]  = KEY_4,
	[0x37]  = KEY_5,
	[0x5e]  = KEY_6,
	[0x2d]  = KEY_7,
	[0x1d]  = KEY_8,
	[0x1e]  = KEY_9,
	[0x11]  = KEY_0,
	[0x0f]  = KEY_MINUS,
	[0x0d]  = KEY_BACKSPACE,  /* delete or backspace? */

	[0x02]  = KEY_TAB,
	[0x4f]  = KEY_Q,
	[0x53]  = KEY_W,
	[0x46]  = KEY_E,
	[0x35]  = KEY_R,
	[0x38]  = KEY_T,
	[0x03]  = KEY_Y,
	[0x2c]  = KEY_U,
	[0x1c]  = KEY_I,
	[0x1f]  = KEY_O,
	[0x12]  = KEY_P,
	[0x10]  = KEY_EQUAL,
	[0x00]  = KEY_ENTER,

	[0x3f]  = KEY_BACKSLASH,   /* ? correct */
	[0x50]  = KEY_A,
	[0x52]  = KEY_S,
	[0x45]  = KEY_D,
	[0x42]  = KEY_F,
	[0x39]  = KEY_G,
	[0x61]  = KEY_H,
	[0x2b]  = KEY_J,
	[0x1b]  = KEY_K,
	[0x20]  = KEY_L,
	[0x13]  = KEY_SEMICOLON,
	[0x29]  = KEY_APOSTROPHE,

	[0x07]  = KEY_LEFTSHIFT,
	[0x51]  = KEY_Z,
	[0x54]  = KEY_X,
	[0x44]  = KEY_C,
	[0x43]  = KEY_V,
	[0x3a]  = KEY_B,
	[0x06]  = KEY_N,
	[0x2a]  = KEY_M,
	[0x28]  = KEY_COMMA,
	[0x5f]  = KEY_DOT,
	[0x60]  = KEY_UP,
	[0x15]  = KEY_RIGHTSHIFT,

	[0x23]  = KEY_LEFTCTRL,
	[0x41]  = KEY_LEFTALT,
	[0x31]  = KEY_LEFTMETA,
	[0x34]  = KEY_SPACE,
	[0x4d]  = KEY_SLASH,
	[0x04]  = KEY_LEFT,
	[0x05]  = KEY_DOWN,
	[0x01]  = KEY_RIGHT,
};

/*
 * Numlock mappings.  This table defines only the special keys
 * which are affected by the numlock mode.  Other keys are mapped
 * by the above table.
 */
static unsigned char numlck_map[NBP_NUMKEYCODES] = {
	[0x2d]      = KEY_KP7,
	[0x1d]      = KEY_KP8,
	[0x1e]      = KEY_KP9,
	[0x11]      = KEY_KPSLASH,

	[0x2c]      = KEY_KP4,
	[0x1c]      = KEY_KP5,
	[0x1f]      = KEY_KP6,
	[0x12]      = KEY_KPASTERISK,

	[0x2b]      = KEY_KP1,
	[0x1b]      = KEY_KP2,
	[0x20]      = KEY_KP3,
	[0x13]      = KEY_KPMINUS,

	[0x2a]      = KEY_KP0,
	[0x28]      = KEY_KPDOT,
	[0x5f]      = KEY_KPPLUS,
};

/*
 * mappings for function key - note *ONLY* keys that produce a unique
 * keycode should be here - others should be handled in userspace as
 * keymap modifiers , fn is the left meta modifier
 */
static unsigned char fn_map[NBP_NUMKEYCODES] = {
	[0x02]      = KEY_CAPSLOCK,
        [0x41]      = KEY_UNKNOWN,
        [0x4d]      = KEY_F1,
        [0x04]      = KEY_HOME,
        [0x60]      = KEY_PAGEUP,
        [0x05]      = KEY_PAGEDOWN,
        [0x01]      = KEY_END,
        [0x28]      = KEY_BRIGHTNESSDOWN,
        [0x5f]      = KEY_BRIGHTNESSUP,
	[0x5b]      = KEY_SLEEP
};

/*
 * Record the state of key presses, so that we can do the right thing
 * after the FN key is released.
 */
#define KP_NONE		(0)
#define KP_DOWN		(1 << 0)
#define KP_FN		(1 << 1)
#define KP_NUMLCK	(1 << 2)

static unsigned char keystate[NBP_NUMKEYCODES];

static int num_lck;

#define key_fn		(keystate[0x31] & KP_DOWN)
#define key_shift	(keystate[0x15] & KP_DOWN)
#define key_alt		(keystate[0x41] & KP_DOWN)

static void nbp_keyb_process_key(unsigned int key)
{
	unsigned char *map = normal_map;
	unsigned int up;

	up   = key & 0x80;
	key &= 0x7f;

	/*
	 * Use the Fn map if the function key is pressed and this
	 * is a key press, or the key is currently in "pressed Fn"
	 * state.  We do not want to map a press without Fn and a
	 * subsequent release of the same key with Fn to different
	 * keycodes.
	 * 
	 * Ditto for the numlock state.
	 */
	if (fn_map[key] && ((key_fn && !up) || keystate[key] & KP_FN)) {
		keystate[key] |= KP_FN;
		map = fn_map;
	} else if (numlck_map[key] && ((num_lck && !up) || keystate[key] & KP_NUMLCK)) {
		keystate[key] |= KP_NUMLCK;
		map = numlck_map;
	}

	/*
	 * The KP_DOWN bit always indicates whether a key is
	 * pressed or released.  Releases always clear all
	 * state about the key.
	 */
	if (up)
		keystate[key] = KP_NONE;
	else
		keystate[key] |= KP_DOWN;

	/*
	 * Now handle the special right-shift + alt combination
	 * for numlock.
	 */
	if (key == 0x41 && key_shift && key_alt) /* alt key */
		num_lck ^= 1;

	/*
	 * Map the keycode from the LX keyboard code to the input
	 * subsystem keycode, using the appropriate mapping table.
	 */
	key = map[key];

	/*
	 * Some keys have additional operations after we have
	 * decoded them.  We don't report these "special" keys
	 * to the input layer.
	 */
	switch (key) {
	case KEY_BRIGHTNESSUP:
                if(!up)
			netbookpro_backlight_mod(4);
                break;

	case KEY_BRIGHTNESSDOWN:
                if(!up)
			netbookpro_backlight_mod(-4);
                break;

	case KEY_SLEEP:
                if(!up) {
#ifdef TEST_ALARM_WAKEUP
			RTAR = RCNR + 60;
#endif
			DBG("System suspend by keyboard\n");
#ifdef CONFIG_APM
			apm_queue_event(APM_SYS_SUSPEND);
#endif
		}
		break;

	default:
		input_report_key(&kbd_dev, key, !up);
		break;
	}
}

static void nbp_keyb_reset(void)
{
	unsigned char *map;
	int key;

	for (key = 0; key < NBP_NUMKEYCODES; key++)
		if (keystate[key] & KP_DOWN) {
			map = normal_map;
			if (keystate[key] & KP_FN)
				map = fn_map;
			else if (keystate[key] & KP_NUMLCK)
				map = numlck_map;
			keystate[key] = KP_NONE;
			input_report_key(&kbd_dev, map[key], 0);
		}

	num_lck = 0;
}

static int nbp_keyb_sendready(int flags)
{
	int ret;

	DBG("nbp_keyb_sendready: flags=%d\n", flags);

	ret = nbp_pcon_command(PCON_KEYBOARD_CMD,
			       NETBOOKPRO_PCON_KEYBOARD_CMD_VAL_READY,
			       flags, NULL);

	DBG("nbp_keyb_sendready: returned %d\n", ret);

	return ret < 0 ? 1 : 0;
}


static int nbp_keyb_eeprom_write(struct i2c_eeprom_emu *eep, 
				 int addr, int val)
{
	DBG("nbp_keyb_eeprom_write(%p,%d,0x%02x)\n", eep, addr, val);

	if (addr != 0)
		return 0;

	nbp_keyb_sendready(PCON_FLG_ASYNC);
	nbp_keyb_process_key((unsigned char)val);

	return 0;
}

static struct i2c_eeprom_emu_watcher nbp_keyb_watcher = {
	.read  = NULL,
	.write = nbp_keyb_eeprom_write
};

static int nbp_keyb_add(struct pcon_device *cdev)
{
	int i;

	init_input_dev(&kbd_dev);

	kbd_dev.evbit[0]    = BIT(EV_KEY) | BIT(EV_REP);
	kbd_dev.keycode     = normal_map;
	kbd_dev.keycodesize = sizeof(unsigned char);
	kbd_dev.keycodemax  = ARRAY_SIZE(normal_map);

	for (i = 0; i < ARRAY_SIZE(normal_map); i++) {
		if (normal_map[i])
			__set_bit(normal_map[i], kbd_dev.keybit);
	}
	for (i = 0; i < ARRAY_SIZE(numlck_map); i++) {
		if (numlck_map[i])
			__set_bit(numlck_map[i], kbd_dev.keybit);
	}
	for (i = 0; i < ARRAY_SIZE(fn_map); i++) {
		if (fn_map[i])
			__set_bit(fn_map[i], kbd_dev.keybit);
	}

	kbd_dev.name        = "NetBookPro Keyboard";
	kbd_dev.phys        = "pcon/input0";
	kbd_dev.dev         = cdev->dev;

	input_register_device(&kbd_dev);

	printk(KERN_INFO "input: %s\n", kbd_dev.name);

	i2c_eeprom_emu_addwatcher(i2c_pxa_get_eeprom(),
				  NETBOOKPRO_I2C_EEPEMU_KEYBOARD_START,
				  NETBOOKPRO_I2C_EEPEMU_KEYBOARD_SIZE,
				  &nbp_keyb_watcher);

	return 0;
}

static void nbp_keyb_remove(struct pcon_device *cdev)
{
	/* what about our eeprom emulation? */

	input_unregister_device(&kbd_dev);
}

static int nbp_keyb_resume(struct pcon_device *cdev)
{
	nbp_keyb_reset();
	return 0;
}

static struct pcon_interface nbp_keyb_intf = {
	.add	= nbp_keyb_add,
	.remove	= nbp_keyb_remove,
	.resume	= nbp_keyb_resume,
};

static int __init nbp_keyb_init(void)
{
	printk(KERN_INFO "NetBookPro Keyboard Driver, (c) 2004 Simtec Electronics\n");

	return pcon_drv_add(&nbp_keyb_intf);
}

static void __exit nbp_keyb_exit(void)
{
	pcon_drv_remove(&nbp_keyb_intf);
}

module_init(nbp_keyb_init);
module_exit(nbp_keyb_exit);

MODULE_AUTHOR("Ben Dooks, Simtec Electronics <ben@simtec.co.uk>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Psion Teklogix NetBookPro Keyboard Driver");

module_param(debug, int, 0600);
