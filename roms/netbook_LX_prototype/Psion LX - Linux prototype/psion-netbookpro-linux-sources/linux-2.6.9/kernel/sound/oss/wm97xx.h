/*
 * Register bits for Wolfson WM97xx series of codecs
 */
 
#ifndef _WM97XX_H_
#define _WM97XX_H_

#include <linux/ac97_codec.h>	/* AC97 control layer */
#include <linux/input.h>		/* Input device layer */

/*
 * WM97xx AC97 Touchscreen registers
 */
#define AC97_WM97XX_DIGITISER1	0x76
#define AC97_WM97XX_DIGITISER2	0x78
#define AC97_WM97XX_DIGITISER_RD 0x7a
#define AC97_WM9713_DIG1		0x74
#define AC97_WM9713_DIG2		AC97_WM97XX_DIGITISER1
#define AC97_WM9713_DIG3		AC97_WM97XX_DIGITISER2

/*
 * WM97xx register bits
 */
#define WM97XX_POLL		0x8000		/* initiate a polling measurement */
#define WM97XX_ADCSEL_X		0x1000		/* x coord measurement */
#define WM97XX_ADCSEL_Y		0x2000		/* y coord measurement */
#define WM97XX_ADCSEL_PRES	0x3000		/* pressure measurement */
#define WM97XX_ADCSEL_MASK	0x7000
#define WM97XX_COO		0x0800		/* enable coordinate mode */
#define WM97XX_CTC		0x0400		/* enable continuous mode */
#define WM97XX_CM_RATE_93	0x0000		/* 93.75Hz continuous rate */
#define WM97XX_CM_RATE_187	0x0100		/* 187.5Hz continuous rate */
#define WM97XX_CM_RATE_375	0x0200		/* 375Hz continuous rate */
#define WM97XX_CM_RATE_750	0x0300		/* 750Hz continuous rate */
#define WM97XX_CM_RATE_8K	0x00f0		/* 8kHz continuous rate */
#define WM97XX_CM_RATE_12K	0x01f0		/* 12kHz continuous rate */
#define WM97XX_CM_RATE_24K	0x02f0		/* 24kHz continuous rate */
#define WM97XX_CM_RATE_48K	0x03f0		/* 48kHz continuous rate */
#define WM97XX_CM_RATE_MASK	0x03f0
#define WM97XX_RATE(i)		(((i & 3) << 8) | ((i & 4) ? 0xf0 : 0))
#define WM97XX_DELAY(i)		((i << 4) & 0x00f0)	/* sample delay times */
#define WM97XX_DELAY_MASK	0x00f0
#define WM97XX_SLEN		0x0008		/* slot read back enable */
#define WM97XX_SLT(i)		((i - 5) & 0x7)	/* touchpanel slot selection (5-11) */
#define WM97XX_SLT_MASK		0x0007
#define WM97XX_PRP_DETW		0x4000		/* pen detect on, digitiser off, wake up */
#define WM97XX_PRP_DET		0x8000		/* pen detect on, digitiser off, no wake up */
#define WM97XX_PRP_DET_DIG	0xc000		/* pen detect on, digitiser on */
#define WM97XX_RPR		0x2000		/* wake up on pen down */
#define WM97XX_PEN_DOWN		0x8000		/* pen is down */
#define WM97XX_ADCSRC_MASK	0x7000		/* ADC source mask */

/* WM9712 Bits */
#define WM9712_45W		0x1000		/* set for 5-wire touchscreen */
#define WM9712_PDEN		0x0800		/* measure only when pen down */
#define WM9712_WAIT		0x0200		/* wait until adc is read before next sample */
#define WM9712_PIL		0x0100		/* current used for pressure measurement. set 400uA else 200uA */ 
#define WM9712_MASK_HI		0x0040		/* hi on mask pin (47) stops conversions */
#define WM9712_MASK_EDGE	0x0080		/* rising/falling edge on pin delays sample */
#define	WM9712_MASK_SYNC	0x00c0		/* rising/falling edge on mask initiates sample */
#define WM9712_RPU(i)		(i&0x3f)	/* internal pull up on pen detect (64k / rpu) */
#define WM9712_ADCSEL_COMP1	0x4000		/* COMP1/AUX1 measurement (pin29) */
#define WM9712_ADCSEL_COMP2	0x5000		/* COMP2/AUX2 measurement (pin30) */
#define WM9712_ADCSEL_BMON	0x6000		/* BMON/AUX3 measurement (pin31) */
#define WM9712_ADCSEL_WIPER	0x7000		/* WIPER/AUX4 measurement (pin12) */
#define WM9712_PD(i)		(0x1 << i)  /* power management */ 

/* WM9712 Registers */
#define AC97_WM9712_POWER	0x24
#define AC97_WM9712_REV		0x58

/* WM9705 Bits */
#define WM9705_PDEN		0x1000		/* measure only when pen is down */
#define WM9705_PINV		0x0800		/* inverts sense of pen down output */
#define WM9705_BSEN		0x0400		/* BUSY flag enable, pin47 is 1 when busy */
#define WM9705_BINV		0x0200		/* invert BUSY (pin47) output */
#define WM9705_WAIT		0x0100		/* wait until adc is read before next sample */
#define WM9705_PIL		0x0080		/* current used for pressure measurement. set 400uA else 200uA */ 
#define WM9705_PHIZ		0x0040		/* set PHONE and PCBEEP inputs to high impedance */
#define WM9705_MASK_HI		0x0010		/* hi on mask stops conversions */
#define WM9705_MASK_EDGE	0x0020		/* rising/falling edge on pin delays sample */
#define	WM9705_MASK_SYNC	0x0030		/* rising/falling edge on mask initiates sample */
#define WM9705_PDD(i)		(i & 0x000f)	/* pen detect comparator threshold */
#define WM9705_ADCSEL_BMON	0x4000		/* BMON measurement */
#define WM9705_ADCSEL_AUX	0x5000		/* AUX measurement */
#define WM9705_ADCSEL_PHONE	0x6000		/* PHONE measurement */
#define WM9705_ADCSEL_PCBEEP	0x7000		/* PCBEEP measurement */

/* WM9713 Bits */
#define WM9713_PDPOL		0x0400		/* Pen down polarity */
#define WM9713_POLL			0x0200		/* initiate a polling measurement */
#define WM9713_CTC			0x0100		/* enable continuous mode */
#define WM9713_ADCSEL_AUX4	0x0080		/* AUX 4 measurement */
#define WM9713_ADCSEL_AUX3	0x0040		/* AUX 3 measurement */
#define WM9713_ADCSEL_AUX2	0x0020		/* AUX 2 measurement */
#define WM9713_ADCSEL_AUX1	0x0010		/* AUX 1 measurement */
#define WM9713_ADCSEL_X		0x0002		/* X measurement */
#define WM9713_ADCSEL_Y		0x0004		/* Y measurement */
#define WM9713_ADCSEL_PRES	0x0008		/* Pressure measurement */
#define WM9713_COO			0x0001		/* enable coordinate mode */
#define WM9713_PDEN			0x0800		/* measure only when pen down */
#define WM9713_ADCSEL_MASK	0x00fe		/* ADC selection mask */

/* AUX ADC ID's */
#define TS_COMP1		0x0
#define TS_COMP2		0x1
#define TS_BMON			0x2
#define TS_WIPER		0x3

/* ID numbers */
#define WM97XX_ID1		0x574d
#define WM9712_ID2		0x4c12
#define WM9705_ID2		0x4c05
#define WM9713_ID2		0x4c13

/* Codec GPIO's */
#define WM97XX_MAX_GPIO	16
#define WM97XX_GPIO_1	(1 << 1)
#define WM97XX_GPIO_2	(1 << 2)
#define WM97XX_GPIO_3	(1 << 3)
#define WM97XX_GPIO_4	(1 << 4)
#define WM97XX_GPIO_5	(1 << 5)
#define WM97XX_GPIO_6	(1 << 6)
#define WM97XX_GPIO_7	(1 << 7)
#define WM97XX_GPIO_8	(1 << 8)
#define WM97XX_GPIO_9	(1 << 9)
#define WM97XX_GPIO_10	(1 << 10)
#define WM97XX_GPIO_11	(1 << 11)
#define WM97XX_GPIO_12	(1 << 12)
#define WM97XX_GPIO_13	(1 << 13)
#define WM97XX_GPIO_14	(1 << 14)
#define WM97XX_GPIO_15	(1 << 15)


#define AC97_LINK_FRAME		21		/* time in uS for AC97 link frame */


/*---------------- Return codes from sample reading functions ---------------*/

/* More data is available; call the sample gathering function again */
#define RC_AGAIN		0x00000001
/* The returned sample is valid */
#define RC_VALID		0x00000002
/* The pen is up (the first RC_VALID without RC_PENUP means pen is down) */
#define RC_PENUP		0x00000004
/* The pen is down (RC_VALID implies RC_PENDOWN, but sometimes it is helpful
   to tell the handler that the pen is down but we don't know yet his coords,
   so the handler should not sleep or wait for pendown irq) */
#define RC_PENDOWN		0x00000008

/* The wm97xx driver provides a private API for writing platform-specific
 * drivers. 
 */

/* The structure used to return sampled data into */
typedef struct {
	int x;
	int y;
	int p;
} wm97xx_data_t;

/* The identifier of an auxiliary ADC conversion.
 */
typedef enum {
	WM97XX_AC_BMON,
	WM97XX_AC_AUX1, /* WM9705: AUX     WM9712: COMP1 */
	WM97XX_AC_AUX2, /* WM9705: PHONE   WM9712: COMP2 */
	WM97XX_AC_AUX3  /* WM9705: PCBEEP  WM9712: WIPER */
} wm97xx_aux_conv_t;

/* Codec GPIO status
 */
typedef enum {
	WM97XX_GPIO_HIGH,
	WM97XX_GPIO_LOW
} wm97xx_gpio_status_t;

/* Codec GPIO direction
 */
typedef enum {
	WM97XX_GPIO_IN,
	WM97XX_GPIO_OUT
} wm97xx_gpio_dir_t;

/* Codec GPIO polarity
 */
typedef enum {
	WM97XX_GPIO_POL_HIGH,
	WM97XX_GPIO_POL_LOW
} wm97xx_gpio_pol_t;

/* Codec GPIO sticky
 */
typedef enum {
	WM97XX_GPIO_STICKY,
	WM97XX_GPIO_NOTSTICKY
} wm97xx_gpio_sticky_t;

/* Codec GPIO wake
 */
typedef enum {
	WM97XX_GPIO_WAKE,
	WM97XX_GPIO_NOWAKE
} wm97xx_gpio_wake_t;

/*
 * codec event callback.
 */
typedef int (*codec_event_t)(int status);

/*
 * continuous mode read callback
 */
typedef int (*codec_read_t)(struct input_dev* idev, int pen_down_time, int pressure);

/*
 * codec id
 */
extern u32 wm97xx_id;

struct wm97xx_device {
	char *name;
	struct list_head	devices;
	int	(*probe)	(void);
	int (*remove)	(void);
	int	(*suspend)	(u32 state, u32 level);
	int	(*resume)	(u32 level);
	codec_read_t cont_read;
};

/*
 * WM97xx arch specific drivers must register with the AC97 plugin
 */
extern int wm97xx_driver_register (struct wm97xx_device *dev);
extern void wm97xx_driver_unregister (struct wm97xx_device *dev);

/* Enable or disable continuous mode. The driver itself doesn't
 * support continuous mode since it is highly architecture-specific.
 */
extern void wm97xx_set_continuous_mode (struct wm97xx_device *dev, int enable, int slot, int rate);


/* A special function to be called before and after the auxiliary conversion.
 * This function can e.g. toggle some external switch via a platform specific
 * GPIO to route the required analog signal to the respective WM97xx input.
 * The 'stage' parameter is 0 before conversion, 1 after.
 */
typedef void (*wm97xx_ac_prepare) (wm97xx_aux_conv_t ac, int stage);

/* Start a periodic auxiliary ADC conversion with given period.
 * Note that auxiliary conversions are performed only when the
 * pen is not down, since they are considered slowly-changing
 * events. Period is given in jiffies. Additionally, a function can
 * be defined to be called before and after every conversion. This
 * can be used e.g. to route some analog signal to the respective
 * input via platform-specific GPIOs and such.
 * Use a period of 0 to disable sampling.
 */
extern int wm97xx_request_auxconv(struct wm97xx_device *dev, wm97xx_aux_conv_t ac, int period, wm97xx_ac_prepare func);
extern void wm97xx_free_auxconv(struct wm97xx_device *dev, wm97xx_aux_conv_t ac);

/* Get the result of an auxiliary ADC conversion.
 * If an auxiliary ADC reading is not yet available, the function
 * returns -1. The platform-specific driver must start
 */
extern int wm97xx_get_auxconv (struct wm97xx_device *dev, wm97xx_aux_conv_t ac);

/* Codec IRQ access
 * The codec can generate IRQ's (over AC97 link or IRQ pin) for 
 * numerous external or internal events. This is used to 
 * enable and disable codec IRQ's
 */
extern int wm97xx_set_codec_irq(struct wm97xx_device *dev, int irq, int link);
extern int wm97xx_clear_codec_irq(struct wm97xx_device *dev); 
 
/* Register interest in external/internal codec events */
extern int wm97xx_request_codec_event(struct wm97xx_device *dev, int gpio, codec_event_t work);
extern void wm97xx_free_codec_event(struct wm97xx_device *dev, int gpio);
 
/* Codec PEN IRQ access
 * The codec can additionally generate an IRQ (using a dedicated pin) 
 * for Pen down events. This is used to enable and disable codec 
 * Pen down IRQ's
 */
extern int wm97xx_set_pen_irq(struct wm97xx_device *dev, int irq);
extern void wm97xx_clear_pen_irq(struct wm97xx_device *dev); 

/* Codec GPIO access (not supported on WM9705)
 * This can be used to set/get codec GPIO and Virtual GPIO status.
 */
extern wm97xx_gpio_status_t wm97xx_get_codec_gpio (struct wm97xx_device *dev, u32 gpio);
extern void wm97xx_set_codec_gpio (struct wm97xx_device *dev, u32 gpio, wm97xx_gpio_status_t status);
extern void wm97xx_config_codec_gpio (struct wm97xx_device *dev, u32 gpio, wm97xx_gpio_dir_t dir, wm97xx_gpio_pol_t pol,
							wm97xx_gpio_sticky_t sticky, wm97xx_gpio_wake_t wake);
	 
/* Codec register access
 * This can be used to read and write codec registers.
 */
extern u16 wm97xx_codec_reg_read (struct wm97xx_device *dev, u16 reg);
extern void wm97xx_codec_reg_write (struct wm97xx_device *dev, u16 reg, u16 val);

#endif
