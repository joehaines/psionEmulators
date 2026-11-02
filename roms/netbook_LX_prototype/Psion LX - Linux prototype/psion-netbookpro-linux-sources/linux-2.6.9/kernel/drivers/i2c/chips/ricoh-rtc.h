
typedef enum {
	RICOHRTC_REG_Seconds         = 0,
	RICOHRTC_REG_Minutes         = 1,
	RICOHRTC_REG_Hours           = 2,
	RICOHRTC_REG_DayOfWeek       = 3,
	RICOHRTC_REG_DayOfMonth      = 4,
	RICOHRTC_REG_Months          = 5,
	RICOHRTC_REG_Years           = 6,
	RICOHRTC_REG_OscillatorAdj   = 7,
	RICOHRTC_REG_AlarmWMinute    = 8,
	RICOHRTC_REG_AlarmWHour      = 9,
	RICOHRTC_REG_AlarmWDayOfWeek = 10,
	RICOHRTC_REG_AlarmDMinute    = 11,
	RICOHRTC_REG_AlarmDHour      = 12,
	RICOHRTC_REG_Control1        = 14,
	RICOHRTC_REG_Control2        = 15
} ricohrtc_reg_t;

typedef enum {
	RICOH_UNKNOWN,
	RICOH_R2051,
	RICOH_R5C386
} ricohrtc_type_t;

#define RICOHRTC_CTRL1_WALE       (1<<7)
#define RICOHRTC_CTRL1_DALE       (1<<6)
#define RICOHRTC_CTRL1_24HR       (1<<5)
#define RICOHRTC_CTRL1_SCRATCH    (1<<4)
#define RICOHRTC_CTRL1_TEST       (1<<3)
#define RICOHRTC_CTRL1_CTMask     (0x7)

#define RICOHRTC_CTRL2_VDSL       (1<<7)
#define RICOHRTC_CTRL2_VDET       (1<<6)

#define RV386RTC_CTRL2_SCRATCH2   (1<<5)
#define R2051RTC_CTRL2_XSTP       (1<<5)

#define R2051RTC_CTRL2_VDSL_16V   (1<<4)
#define RV386RTC_CTRL2_XSTP       (1<<4)

#define RICOHRTC_CTRL2_SCRATCH1   (1<<3)
#define RICOHRTC_CTRL2_CTFG       (1<<2)
#define RICOHRTC_CTRL2_WAFG       (1<<1)
#define RICOHRTC_CTRL2_DAFG       (1<<0)

#define RICOHRTC_REG_Months_Y2K   (1<<7)


