CronTab v0.90
-------------

CronTab runs programs and macros at scheduled time,
allowing backups, compression of files, defragmentation
of disks and memory or any other batch at regular
intervals (e.g. during the night).

When the machine is off, CronTab wakes it up, runs the
batch and switches it off.

CronTab features also the possibiliy to run alarms
that will stop after a specified number of sounds,
saving the batteries when someone is not around
to cancel it.


Installation:
-------------
Install the packages CronTab.sis and Macro5Lib.sis
as you do with all SIS files : double-click on them either
on the PC (if PsiWin 2.1 is installed) or on the Series 5
(if there is an Add/Remove icon in the configuration panel).
If you don't already have it, install SysRam1.sis.


Usage:
------
The usage of ConTab is quite similar to the
regular setting of alarms.

You can choose to execute a program or a macro,
to sound an alarm or to backup and compress a folder.

The repeat method 'Set by macro' is meant to be used
with macros only. In this case the macro sets itself
the next execution by putting in the variable NextTime&
the time (in seconds from 01/01/1970) of the next wake up.
If NextTim&=0, the job is disable. You can use this option
to make the scheduling of jobs dependent of external
factors or to set up unregular intervals (e.g. every hours
from 8h00 to 18h00).


Known issues :
--------------
CronTab uses the temporary UID &FEDCBA96.


Copyright :
-----------
This program has been written by Pascal NICOLAS.
It may be distributed freely as long as it is
not altered or sold. This program is a SmileWare,
which means that if you use it, you "MUST" send me a :-)

At this point, many many thanks and :-) to MattM who
helped me on how to trigger alarms.

Pascal NICOLAS
Email : pnicolas@geocities.com
Latest version and a huge library of macros
at http://www.geocities.com/SiliconValley/Pines/1215