/* drivers/video/s1d3xxxfb.h
 *
 * (c) 2004 Simtec Electronics
 *
 * Header file for Epson S1D13XXX driver code
*/

struct s1d13xxx_regval {
	unsigned short       addr;
	unsigned char        value;
};

struct s1d13xxx_reginit {
	const struct s1d13xxx_regval   *values;
	unsigned int                    length;
};

struct s1dfb;

struct s1dfb_mach {
	const char   *name;
	void        (*blank)(struct s1dfb *fb, int to);
	void        (*suspend)(struct s1dfb *fb);
	void        (*resume)(struct s1dfb *fb);
};

extern void s1d13xxx_runinit(struct s1dfb *fb, struct s1d13xxx_reginit *);
extern void s1d13xxx_set_machine(struct s1dfb *sdfb, struct s1dfb_mach *mach);

extern void s1d13xxx_writereg(struct s1dfb *fb, int regno, int value);
extern unsigned int s1d13xxx_readreg(struct s1dfb *fb, int regno);



extern int s1dfb_netbookpro_init(struct s1dfb *sdfb);


/* some register definitions */

#define S1DREG_REVCODE       (0x00)
#define S1DREG_GPIO_CTRL0    (0x08)
