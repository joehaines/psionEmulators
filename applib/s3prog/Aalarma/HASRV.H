/*      INCLUDE - HASRV.H
Consumer Handheld alarm server constants.
Copyright (c) Psion PLC 1991.

 VER    DATE    BY  DESCRIPTION
----- -------- ---- -----------
0.01A          NSM  First release
2.00A  1/6/93  MHS  S3B release
*/
#define HASRV_H
#ifndef P_FILE_H
#include <p_file.h>
#endif

#define TypeTimedAlarm 1 /* Timed diary type alarm */
#define TypeUnTimedAlarm 2 /* Untimed diary type alarm */
#define OldMaxAlarmText 65 /* S3 Maximum size of alarm text including zts */
#define MaxAlarmText 161 /* Maximum size of alarm text including zts */

/* ALM: i/o functions as well as P_FCLOSE and P_FCANCEL */
#define A_FUNTIMED P_FREAD
#define A_FTIMED P_FWRITE
#define A_FUNTIMED_X P_FSEEK
#define A_FTIMED_X P_FSETEOF

#define TIME_TO_ALERT 0 /* First time is when to raise the alert */
#define TIME_EVENT_DUE 1 /* Second time is when the actual event is due */

typedef struct
	{
	UBYTE len;
	TEXT  name[8];
	UBYTE zero_term;
	} SE_SND;

typedef struct
	{
    ULONG absTime; /* Alarm time in seconds */
    ULONG dueTime; /* Actual time alarm is due for */
	SE_SND sound; /* The sound to play */
	} AXDATA;


