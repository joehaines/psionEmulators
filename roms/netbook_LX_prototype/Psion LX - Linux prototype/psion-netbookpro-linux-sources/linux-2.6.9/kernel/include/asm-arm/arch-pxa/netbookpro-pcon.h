
/* old ioctl() style commands */

#define I2C_SETREDLED           _IOW('i', 1, int)
#define I2C_SETUSBHOSTPOWER     _IOW('i', 2, int)
#define I2C_SETMMCPOWER         _IOW('i', 3, int)
#define I2C_SETVMODEMPOWER      _IOW('i', 4, int)
#define I2C_SETSERIALPOWER      _IOW('i', 5, int)
#define I2C_SETPCMCIAPOWER      _IOW('i', 6, int)
#define I2C_SETCFPOWER          _IOW('i', 7, int)

#define I2C_GETUSBHOSTPOWER     _IOR('i', 8, int)
#define I2C_GETMMCPOWER         _IOR('i', 9, int)
#define I2C_GETVMODEMPOWER      _IOR('i', 10, int)
#define I2C_GETSERIALPOWER      _IOR('i', 11, int)
#define I2C_GETPCMCIAPOWER      _IOR('i', 12, int)
#define I2C_GETCFPOWER          _IOR('i', 13, int)

/* internal command definitions */

#define PCON_CMD(x) ((x))

#define PCON_MONITORBATTERY     PCON_CMD(0x01)
#define PCON_KEYBOARD_CMD       PCON_CMD(0x02)
#define PCON_POWER_STATE        PCON_CMD(0x03)

#define PCON_FLG_ASYNC          (1)

struct pcon_cmd_eedata {
	unsigned int addr;
	unsigned int length;
	unsigned char *data;
};

/* values for system */

typedef enum {
	PCON_CMD_Ready          = 0,
	PCON_CMD_ADCRead        = 9,
	PCON_CMD_GetVersion     = 10,
	PCON_CMD_SetMMCPower    = 11,
	PCON_CMD_SetModemPower  = 12,
	PCON_CMD_SetUSBPower    = 13,
	PCON_CMD_SetRedLED      = 14,
	PCON_CMD_SetSerialPower = 15,
	PCON_CMD_GetSerialCfg   = 17,
	PCON_CMD_BatteryMonitor = 18,
	PCON_CMD_GetMMCPower    = 19,
	PCON_CMD_GetVModemPower = 20,
	PCON_CMD_GetUSBPower    = 21,
	PCON_CMD_SetWakeupAlarm = 22,
} pcon_cmd_t; 


typedef enum pcon_pwrstate_e {
	PCON_PWRSTATE_HWReset    = 0,
	PCON_PWRSTATE_SWReset    = 1,
	PCON_PWRSTATE_Off        = 2,
	PCON_PWRSTATE_Suspend    = 3,
	PCON_PWRSTATE_Sleep      = 4,
	PCON_PWRSTATE_Booting    = 5,
	PCON_PWRSTATE_LoadingOS  = 6,
	PCON_PWRSTATE_Idle       = 7,
	PCON_PWRSTATE_Run        = 8,
	PCON_PWRSTATE_Comatose   = 9,
	PCON_PWRSTATE_PowerFail  = 10
} pcon_pwrstate_t;


#define NETBOOKPRO_PCON_KEYBOARD_CMD_ADDR      (0x00)
#define NETBOOKPRO_PCON_KEYBOARD_CMD_VAL_READY (0)
#define NETBOOKPRO_PCON_KEYBOARD_CMD_VAL_BUSY  (1)

#define NETBOOKPRO_PCON_POWER_PCMCIA           (3)
#define NETBOOKPRO_PCON_POWER_CF               (2)

#define NETBOOKPRO_PCON_POWERCONFIG            (4)
#define NETBOOKPRO_PCON_POWER                  (5)

#define NETBOOKPRO_PCON_COMMAND_ADDR           (12)


#define NETBOOKPRO_I2C_EEPEMU_KEYBOARD_START   (0x00)
#define NETBOOKPRO_I2C_EEPEMU_KEYBOARD_SIZE    (1)

#define NETBOOKPRO_I2C_EEPEMU_SERIAL_START     (0x02)
#define NETBOOKPRO_I2C_EEPEMU_SERIAL_SIZE      (1)

#define NETBOOKPRO_I2C_EEPEMU_POWER_START      (0x05)
#define NETBOOKPRO_I2C_EEPEMU_POWER_SIZE       (1)

#define NETBOOKPRO_I2C_EEPEMU_CMDRESP_START    (13)
#define NETBOOKPRO_I2C_EEPEMU_CMDRESP_SIZE     (4)


/* exported functions */

extern int nbp_pcon_write2(unsigned int, unsigned int);
extern int nbp_pcon_command(unsigned long cmd, unsigned int arg,
			    unsigned int flags, int *result);

extern int nbp_pcon_ready(void);

struct rtc_time;
extern int nbp_pcon_rtc_wakeup(struct rtc_time *tm);

struct pcon_device;

struct pcon_interface {
	struct list_head node;
	int (*add)(struct pcon_device *);
	void (*remove)(struct pcon_device *);
	int (*suspend)(struct pcon_device *, int);
	int (*resume)(struct pcon_device *);
};

/*
 * New interface.  pcon "drivers" should use this.
 */
int pcon_drv_add(struct pcon_interface *intf);
void pcon_drv_remove(struct pcon_interface *intf);

struct pcon_device {
	struct list_head node;
	struct pcon_interface *intf;
	struct device *dev;
	void *data;
};

#define clsdev_to_pcon_dev(cdev)	container_of(cdev, struct pcon_device, cdev)

/*
 * Use this to register the pcon "device".
 */
struct pcon_device *pcon_dev_add(const char *name, struct device *dev, void *data);
void pcon_dev_remove(struct pcon_device *cdev);
int pcon_dev_resume(struct pcon_device *pcon);
int pcon_dev_suspend(struct pcon_device *pcon, int state);
