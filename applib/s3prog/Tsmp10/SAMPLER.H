/* SYS$SAMP.LDD Header File, by Jason, PSION PLC March 1994 */

#define	P_SAMPLES_SINCE_LAST	0
#define	P_SAMPLES_FROM_LAST	1
#define	P_SAMPLES_FROM_NOW	-1

#define	P_FCONVERT_TO		10
#define	P_FCONVERT_FROM		11

typedef struct /* Conversion structure */
    {
    VOID *Source;
    VOID *Destination;
    } P_CONVERT;
