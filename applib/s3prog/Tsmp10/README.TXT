Using SMP: (SYS$SAMP.LDD), the sound sampling device driver
===========================================================

Files in this zip (TSMP10.ZIP)
------------------------------
README.TXT      This documentation
SYS$SAMP.LDD    The device driver itself
SAMPLER.H       Header file (for C programming)
TSMP.IMG        Example code ready build
TSMP.C          Source of example code (C)
TSMP.OPL        Example code (OPL)
TSMP2.OPL       A second Opl example program

The basic idea of sound samples
-------------------------------
When the microphone is enabled, a "sound value" is provided, by the
hardware, once every 8000th of a second.  This sound value is
actually provided as an ALAW encoded byte (8 bit value).

The PLIB function p_recordsoundw (and related functions) write series
of these sound values (or "sound samples") direct to a nominated
file.  However, using SMP:, you can have these values placed into a
buffer in your dataspace, that you can analyse directly.

You can either analyse these ALAW encoded values directly, or you can
have SMP: return a buffer of decoded values - where the decoded
values are 13 bit numbers in the range -4096 to +4096.

Once you have opened a channel to SMP:, you can use code as simple as
    p_read(pcb,&buf[0],len);
to read 'len' consecutive samples into a buffer.  The largest allowed
value of 'len' is 4000, in which case the data in buf[] represents
half a second of sound.

The supplied test program TSMP
------------------------------
Copy TSMP.IMG into an \IMG\ directory.
Copy SYS$SAMP.LDD into the root directory on the same device.
Go to the RunImg icon, highlight "Tsmp", and press Enter.
Press Enter again to accept the initial suggestion.
You will see a continually refreshed graph of the sound going into
the microphone at that very moment.
Try whistling, talking, or making other noises.
Exit the application from the Shell when you are bored with it.

Note that after you exit TSMP, by default the SMP: device driver is
left loaded in memory (using up around 6k of RAM).  To clean this up,
run the program again, but choose Esc at the initial query alert.

If you make a very short sound, TSMP may fail to pick this up - this
is because TSMP *doesn't* monitor SMP: continuously - it spends a lot
of time drawing its graph instead.  But you could rewrite TSMP in
order to be more responsive.  Ie it could avoid making any display
until the sound reached a higher threshold.  It could improve its
performance even further by reading and analysing the ALAW encoded
values directly, which is a faster service that reading the decoded
values.

Warning: TSMP will keep your S3a switched on!  Be sure to exit it!
Also, while it's running, it will prevent any other access to the
sound system on the S3a - so no alarms will sound, etc.

The C source code and the Opl source code
-----------------------------------------
The code in TSMP.C and TSMP.OPL corresponds in all essential details.
(Except that the C code runs in "compatibility mode".)
For simplicity, I shall just describe the services of SMP: in terms
of the C interface.
(Though I have provided TWO Opl example programs, and only one in C.)

Basic pattern of use of SMP:
----------------------------
Before you can use any of the services of SMP:, you have to open a
channel to it, using p_open.  Eg

    LOCAL_C VOID OpenChannel(VOID)
        {
        INT ret;
    
        ret=p_open(&pcb,"SMP:",-1);
        if (ret)
            Panic(ret,"Failed to open channel to SMP");
        }

This writes back into pcb the "handle" (the "Pointer to the Control
Block") that you use in all subsequent p_read, p_iow, p_close (etc)
calls to SMP:.

Before you can open a channel to SMP:, however, you have to ensure
that the device driver SYS$SAMP.LDD has been loaded into memory.   Eg

    LOCAL_C VOID LoadDriver(VOID)
        {
        INT ret;
        TEXT buf[P_FNAMESIZE];
    
        p_fparse("\\sys$samp.LDD",DatCommandPtr,&buf[0],NULL);
        ret=p_loadldd(&buf[0]);
        if (ret && ret!=E_FILE_EXIST)
            Panic(ret,"Failed to load SYS$SAMP.LDD");
        }

SMP: keeps recording samples internally all the time
----------------------------------------------------
SMP: can have up to four open channels, from four different
applications at the same time.

Whenever there is at least one open channel to SMP:, it keeps
recording sample values internally.  These values then get written
into applications' buffers when requested.

SMP: only keeps half a second's worth of samples, however.

Three modes of using SMP:
-------------------------
In the default mode ("P_SAMPLES_SINCE_LAST"), you ask for a fixed
number of samples, and you at once get given either this number of
samples, or as many as have been collected since the last time you
requested samples (if this is less).

In the mode P_SAMPLES_FROM_LAST, you definitely get given the number
of samples you ask for, starting from the last time you requested
samples, and the call only completes when the required number of
samples is ready.

In the mode P_SAMPLES_FROM_NOW, the same applies, but the samples
only start from "now".

How to change the mode
----------------------
You use the P_FSET function.  Thus
    p_iow3(pcb,P_FSET,mode);

You can also sense the mode, using
    WORD mode;
    p_iow3(pcb,P_FSENSE,&mode);

Reading and "super-reading"
---------------------------
The call
    p_read(pcb,&buf[0],len);
is just equivalent, as always, to
    p_iow4(pcb,P_FREAD,&buf[0],&len);

Here, len is the number of WORDs of data in buf[].

If you prefer, you can use P_FRSUPER instead of P_FREAD, to read a
number of BYTEs of ALAW encoded data - as discussed earlier.  In this
case, there is no utility function p_read - you have to use the p_iow
(or p_ioc etc) call.

In either P_FREAD or P_FRSUPER, the number of samples actually
delivered is written back to the 'len' variable.

Note that only a single P_FREAD or P_FRSUPER per open channel should
be pending at any one time.

Technical note
--------------
The "ALAW encoded data" provided by P_FRSUPER isn't exactly the same
as provided by the raw CODEC hardware.  The value is inverted and
then "magic xor-red" (to XOR all the even bits), before being
returned to the application.  This means that a louder sound will
give rise to a larger value.

(In contrast, the bytes written to file by services such as
p_recordsoundw are *exactly* as provided by the CODEC.)

Synchronous and asynchronous calls
----------------------------------
Like all device drivers, SMP: supports asynchronous versions of its
services as well as synchronous ones.  And you can P_FCANCEL an
outstanding asynchronous request.

Special utility conversion services
-----------------------------------
SMP: also provides a couple of conversion services, to convert a
buffer of WORDs of decoded data into another buffer of BYTEs of ALAW
encoded data, and vice versa.  These services both use the P_CONVERT
struct, defined in the header file SAMPLER.H:

    typedef struct /* Conversion structure */
        {
        VOID *Source;
        VOID *Destination;
        } P_CONVERT;

For example,

    BYTE aLaw[4];
    WORD Data[4];
    P_CONVERT Conversion;

    Len=4;
    Conversion.Source=&Data[0];
    Conversion.Destination=&aLaw[0];
    p_iow4(pcb,P_FCONVERT_TO,&Conversion,&Len);
    Conversion.Source=&aLaw[0];
    Conversion.Destination=&Data[0];
    p_iow4(pcb,P_FCONVERT_FROM,&Conversion,&Len);

Values of some #define's (for Opl programmers)
----------------------------------------------
#define P_FREAD   1
#define P_FCANCEL 4
#define P_FSET    7
#define P_FSENSE  8
#define P_FRSUPER 13

An example program using "super-reading"
----------------------------------------
Have a look at TSMP2.OPL.  Try to figure out what happens when you
run it, and then run it and see if you were right.

Some of the limitations in the performance in TSMP2.OPL are due to
time wasted scanning the buffer for large values.  This time could be
cut down by programming in C, or by creating some machine code and
accessing that using USR.

Disclaimer
----------
Psion will not be held responsible for any loss of data as a result
of use of SYS$SAMP.LDD, which is still to an extent in an experimental
phase.

SYS$SAMP.LDD is not an official Psion product.

It has not yet been exhaustively tested, and cannot be relied upon eg
not to reset your S3a occasionally.

Distribution
------------
You can freely distribute the file TSMP10.ZIP, but all the files in
it should be kept together.

These notes prepared
--------------------
By David Wood, Psion Product Development, 24th March 1994.
Revised, 6th April 1994 (TSMP2.OPL added.)
All comments, bug reports etc welcome by EMAIL (eg CiX) ONLY.
