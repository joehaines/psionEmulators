MISCELLANEOUS UTILITIES
======================

INTRODUCTION
I created a number of utility programs in OPL which I find useful, and have
one or two from other people which I use, and I figured that there must be
some way to have these available as stand-alone programs, and from any other
calling program or application for the sake of convenience, and also in order
to make maximum use of restricted memory space. If I wanted to be able to use
exactly the same routines in other programs then all I wanted to have do is
call the code which I had already created.
So I set about converting them so that I can do exactly that, and the result
is the undernoted group of utilities together with a small application which
makes accessing them easy.

The utilities themselves are as follows:

Alarm		Alarm creation tool (virtually straight from the User Guide).
Days		Program which calculates the date for any given year from the
		offset number of days ináthat year (e.g. 1 January = 1,
		1 February = 32, etc).
Distance	Converts miles to/from kilometres.
DTMF		Phone dialler which uses an optional data file to supply the
		telephone number to be dialled, defaulting to my home
		telephone number if none supplied - more notes below).
Easter		Calculates the date of Easter Monday for any given year.
Fuel		Calculates fuel consumption in MPG/Litres per 100 Km given
		distance in miles & litres of fuel.
Height		Converts inches to/from centimetres.
Liquids		Converts pints to/from litres.
Lottery		Picks 6 random numbers every time.
Shifts		Calculates my work's shift roster for any given week for any
		date in that week from 3-9-79 until 2-9-2009. These dates are
		significant to me! I may consider tailoring it to anyone
		else's shift pattern for a suitable fee - there is a lot of
		work involved.
Sort		Database sorting utility which uses John Boyce's sorting code
		(more detailed notes below).
Temp		Converts degrees Fahrenheit to/from degrees Celsius.
Weight		Converts pounds to/from kilograms.

Each program can be run directly from the RunOPL application, or can be
called from another OPO or APP. I have created a simple application called
MISC which gets details of all the available programs from a database file
called MISC.ODT, which resides in the OPD directory. So, adding or removing
utilities is straightforward. In order to use a program from the MISC
application it must have a procedure called "Start:" in it, because this is
what MISC calls to start off every program which it calls.
The DTMF & SORT utilities are somewhat different from the rest, and I will
expand on these individually.

DTMF
This program requires a Psion with DTMF tone dialling, so will not work on a
Siena, although it will not crash one if it is run, it just doesn't do
anything. I have included the source code so that anyone who wishes to make
use of it can amend it to include their own default 'phone number, rather
than mine. It also show the way I have set it up my programs to work with
MISC. As can be seen, the optional data file is called DTMF.ODB, is looked
for in the OPD directory, and consists of 1 record with 2 fields. The first
field contains the dialling code, and the second contains the number. If you
wish to use this facility in your own programs to dial a particular number
then your program must create that file for DTMF to read.
The DTMF program adds a prefix, which I have set to "9" (change this if
required), so that for users within a private exchange it will dial for an
outside line prior to dialling the number. The program offers the option of
including or excluding either or both the prefix and the code.

SORT
The front end is based on the sample which John Boyce supplies in OPL format.
I used to use my own home-grown file sorting utility, until I found John
Boyce's. His code runs like a hare compared to my own tortoise-like offering.
Why re-invent the wheel? I don't believe that I have the right to distribute
his program, so this requires to be obtained from the same source which this
came from. I have passed this group of utilities to Steve Litchfield's 3-Lib,
and the sorting utility can be obtained lawfully from there.
In any case I have included the amended source code for this program also,
should it be required. The changes from John's source code are few, & only
to make it compatible with MISC. This could be converted to use an optional
file, similar to DTMF, without too much difficulty.

MISC
This is the application which calls all of the above programs. As I said
earlier it makes use of a data file (MISC.ODT) which contains details of the
programs which can be called by it. There is a record for each program, and
each record consists of 2 fields. The first field contains the name to be
displayed, and the second contains the filename of the OPO file which is
used by MISC to call the program. By editing this file (or creating a new
one if you like) the user can tailor MISC to suit him or herself, including
only the utilities wanted, or adding new ones to suit. Again, I have included
source code for this so that it can be fully tailored to suit. If anyone
wishes to do so, then the programs themselves can be moved to a different
directory entirely, with only minor changes to the code for MISC.

INSTRUCTIONS FOR USE
Copy the files into the following directories on your Psion computer:

File			Destination directory
----			---------------------
MISC.OPA		Any "APP" directory if more than one drive
MISC.ODT		"OPD" directory within default drive
*.OPO			"OPO" directory within default drive
*.OPL			"OPL" directory (if required)
SPANNER.ICO		"OPD" directory within default drive (if required)

I have included the icon which I used, so that if anyone decides to make any
changes then he or she can do so and retain the same icon on the screen if
desired.

COPYRIGHT
As this is entirely my own work, except for the code for the sorting program
for which I take no credit as I have explained already, then I require that
anyone who distributes or passes this on does so without any change to the
original code as supplied by me, together with all files including this file.
If anyone has any suggestion for permanent changes then please write or
e-mail me at the addresses below. Small donations of £5, or whatever is
thought an appropriate sum, would be gleefully accepted from anyone who
makes use of this collection of miscellaneous utilities, which I did not just
knock together in 2 minutes. Please contact me also if anyone wishes further
details of a shift pattern calculator - no promises, but I will consider
seriously any requests, as I have found my own very useful to me. I will
charge a fee, but it won't break the bank.

ADDRESSES
Postal	Brian J Coutts
	80 Callaghan Wynd
	Blantyre
	Glasgow G72 9RP

E-mail	brian@bjcoutts.clara.net
