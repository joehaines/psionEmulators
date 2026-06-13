Hello psion/series3

Here are some OPL programs, stolen from the manual. Most of them are
for reference - to remind you how something odd is done - or for use
as the foundations of other programs. Some of them are useless,
really. Have a look at them before you waste precious SSD space.

A few are hacked versions of old MC or Organiser programs, as can be
seen in their less-than-perfect User Interfaces.

I cart them around on a subdirectory of \OPL, to stop the System
screen going berserk.

In late 1991 I did a slightly revised version of the Programming
Manual. It contained four/five new programs, all of which are
included in this ZIP. They are EX_ANIMA, EX_ALARM, EX_LABEL, and
EX_CIRC1/EX_CIRC2. Documentation for these is included in this file.

There are two other programs in here:

    ORG2AGN converts LZ diaries to Series 3 format (I think Jamie may
    already have sent this up)

    EX_DTMF dials a DTMF number.


I've taken to adding the following into the odd piece of
documentation, and here seems a good place too:

    Psion PLC disclaims any liability for any damage or loss arising
    directly or indirectly from the use of the products described herein.

...and so do I.

Please tell me if any of this is useful, or if there's something else
you want which I might be able to lay my hands on.

Nick Healey
29/1/92



EX_ANIMA: animation example
===========================

This program requires five bitmap files - one.pic to five.pic. (This
version gets them from the Psion logo in the ROM.) A better example
would have each of the 5 differing slightly - they might, for example,
be five `snapshots' of a running human figure, each with the legs at
a different point in their cycle.

The program copies each bitmap into a window of its own, then makes
each window visible in turn, each time slightly further across the
screen.

To make bitmap files, first draw the pattern you want with any of the
graphics drawing commands. (Use gLINEBY 0,0 to draw single dots.)
When the pattern is complete, use gSAVEBIT to make the bitmap file.


EX_LABEL: inserting a new line in a database
============================================

If you insert a new label in a database, the entries will no longer
match up with the labels. To save having to use the `Update' option
on every entry, to insert a suitable blank line in each one, you can
use this program to do this for the entire data file.

The Database allows you to use as many lines (fields) as you want in
an entry (record); OPL can only access 32 fields. This program only
lets you insert a new field in the first 16 fields, although you can
adapt the program simply to check up to 31 fields.

If, in the Database, you enter a line longer than 255 characters, it
is stored as two fields. This program correctly handles any such fields.

The program checks that the 17th field is blank, as it will be
overwritten by what was the 16th field. If a long entry has a 17th
field, and it contains text, the program skips this entry. The rest
of longer entries - even if there are more than 32 fields - will be
unchanged.

If you insert a new field at a position below the last label, the
Database will not show it, even when using `Update'.

The maximum record length in OPL is 1022 characters. The OPEN command
will raise a `Record too large' error if the file contains a record
longer than this.


EX_CIRC1/EX_CIRC2: circles
==========================

These are two example programs for drawing circles - the first hollow,
the second filled.


EX_ALARM: Alarm example - IOA to ALM:
=====================================

The ALM: device provides access to alarms. When writing
to it with IOW or IOA, you can use these two functions:

*) function=1 - only the date (and no time) is shown on the screen
when the alarm rings - e.g. Thu 5 Sep

*) function=2 - the day and time are shown - e.g. Thu 11:54

In either case, you must pass these two arguments:

*) An array of 2 long integers - the first is the time for the alarm
to go off, and the second is the time for which it is due. Both are
given in seconds since midnight a.m. on 1/1/1970.

*) A message, as a ZERO-TERMINATED string of up to 64 characters.

This procedure asks for the information for an alarm, and sets it (as
type 2 - day and time to be shown when the alarm rings). If you press
the Time button, and this is the next alarm to ring, it is shown as a
`RunOpl' alarm.

Use this as a whole program itself - do NOT call it from another
procedure.

The Series 3 allows up to 20 alarms to be pending. If you run this 
program more than once, you should always allow for four Time alarms, 
and one for each Agenda file you use.

