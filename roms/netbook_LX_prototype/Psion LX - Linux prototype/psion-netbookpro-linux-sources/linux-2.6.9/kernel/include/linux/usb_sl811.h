
/*
 * board initialization should put one of these into dev->platform_data
 * and place the sl811hs onto platform_bus.
 */

struct sl811_platform_data {
	unsigned	can_wakeup:1;

	/* given port_power, msec/2 after power on till power good */
	u8		potpg;

	/* mA/2 power supplied on this port (max = default = 250) */
	u8		power;

	/* sl811 relies on an external source of VBUS current */
	/* this must be safe to call in_irq() */
	void 		(*port_power)(struct device *dev, int is_on);

	/* pulse sl811 nRST (probably with a GPIO) */
	void 		(*reset)(struct device *dev);

	// FIXME KARO boards need something like these:
	// bool = check_overcurrent()
};

