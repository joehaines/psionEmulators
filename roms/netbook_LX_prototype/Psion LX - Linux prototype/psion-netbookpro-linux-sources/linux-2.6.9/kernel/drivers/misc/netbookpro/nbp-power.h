
extern void nbp_battmon_rx(void *data);
extern void nbp_battmon_start(void);
extern void nbp_spi_reset(void);

typedef enum nbp_pcon_pwrflags {
	NBP_PCON_PWRF_BatteryPreset       = 0x0001,
	NBP_PCON_PWRF_ACPresent           = 0x0002,
	NBP_PCON_PWRF_ChargeEnabled       = 0x0004,
	NBP_PCON_PWRF_BackupBatteryPreset = 0x0008,
	NBP_PCON_PWRF_Calibrated          = 0x0010,
	NBP_PCON_PWRF_Calibrating         = 0x0020
} nbp_pcon_pwrflags_t;;

typedef enum nbp_pcon_chargestate {
	NBP_PCON_CS_NotCharging           = 0,
	NBP_PCON_CS_Charging              = 1,
	NBP_PCON_CS_QuickChargeComplete   = 2,
	NBP_PCON_CS_ChargeComplete        = 3	
} nbp_pcon_chargestate_t;


/* pcon_battinfo_t should be 32bytes long */

typedef struct pcon_battinfo {
	u16               flags;
	u16               unused[4];
	u16               max_capacity;
	u16               temperature;
	u16               charge_state;
	u16               main_batt_voltage;
	u16               backup_batt_voltage;
	u8                main_batt_capacity_percent;
	u8                backup_batt_capacity_percent;
	u16               charge_current;
	u16               discharge_current;
	u16               pad[2];
	u16               magic;
} __attribute__((__packed__)) pcon_battinfo_t;

#define NETBOOKPRO_PCON_BATTINFO_MAGIC         (0xADB0)

extern int nbp_battmon_getstate(pcon_battinfo_t *info);

