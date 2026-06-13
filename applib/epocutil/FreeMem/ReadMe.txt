FreeMem v1.10
-------------

The memory taken by the system seems
to increase with time as programs
and documents are opened and closed.

In fact, there is no memory leaks but
EPOC caches procedures and does not deallocate
all the memory when an application is closed in
case it is opened again soon.
This saves time and reduces the fragmentation.

However the memory gets fragmented with time
and it reduces performances (large memory
segments are spread over serveral cells).

FreeMem defragments the memory and
reclaims all the unused segments.

FreeMem is also available as a macro for Macro5
or as a batch for CronTab (frees up the memory
but does not display a report).


Installation:
-------------
1 - Install the package FreeMem.sis as you do with
    any SIS file : double-click on it either on
    the PC or on the Series 5.
2 - If you don't already have it, install also SystInfo.sis


Usage:
------
Run FreeMem from System or the Extras-bar.
Once the memory is cleaned up, FreeMem displays a report
with the memory used before, the memory used after, and
the memory freed.


Planned enhancements :
----------------------
None.


Known issues :
--------------
None.


Copyright :
-----------
This program has been written by Pascal NICOLAS.
It may be distributed freely as long as it is
not altered or sold. This program is a SmileWare,
which means that if you use it, you "MUST" send me a :-)

At this point, many many thanks and :-) to MattM who
gave me the trick on how to clean up the memory.

Pascal NICOLAS
Email : pnicolas@geocities.com
Latest version at http://www.geocities.com/SiliconValley/Pines/1215