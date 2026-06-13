AGENDA2.OPX, version 1.13
(C) 1998-1999 Twiddlebit Software

CONTACT
-------

Note that this OPX is provided on an "as is" basis with no guarantee
of support.

For the latest version see the following www page:

	http://www.twiddlebit.com

CONTENTS
--------

agenda2.sis - redistributable SIS file for installation of agenda2.opx only

agenda2.opx - install into \System\Opx directory
agenda2.oxh - install into \System\Opl directory
agenda2.txh - plain text version of agenda2.oxh

agenda2_demo.opl - sample OPL code

agenda2_wins.opx - WINS emulator version (rename & install as above)

INTRODUCTION
------------

This AGENDA2.OPX allows an OPL programmer to read and write data in an
Psion EPOC32 Agenda file.

Note that there is a separate AGENDA.OPX supplied by Psion which allows
day entries to be written to an Agenda. This newer OPX provides the same
capabilities but in addition supports all other agenda entry types, i.e.
day entries, events, todos, anniversaries and todo lists.

We wrote this in order to enable us to support the transfer of todo entries
from our shareware application Plan5. Check out how Plan5 works if you want
to see the AGENDA2.OPX in action. We have released AGENDA2.OPX into the public
domain as freeware for you to use in your own applications.

When distributing the software please make use of the supplied AGENDA2.SIS
file for installation to avoid problems with old versions being installed
over new ones.

If you have the Psion EPOC32 C++ SDK and would like a copy of the C++
source code for the OPX then drop me an email.

WARNING
-------

If you don't write your OPL code correctly to use the supplied OPX it is
very likely that you will either crash your program or corrupt your Agenda
file. So you should thoroughly test your code on a scratch Agenda file
before using or distributing code based upon the AGENDA2.OPX.

The author of AGENDA2.OPX takes no responsability for loss caused by use
of this software.

OVERVIEW
--------

Use of this OPX involves firstly opening an existing agenda file.
Creation of a new file is not supported. To open a file use:

	AgnOpen:(file$)

You need to ensure that the file is not open in Agenda before using
AgnOpen. See the demo OPL code for details of how to automatically
close and reopen Agenda. When finished close the file using:

	AgnClose:

There are 4 basic entries that can be read/written to an Agenda:

	appointments/day entry's
	events
	todo's
	anniversaries

There are also todo-lists within which todo's reside.

Creation of entries involves the following steps:

1. first creating an entry in memory and obtaining a handle to
it, e.g.

	appt& = AgnEnNewAppt&:

2. setting properties on the entry, e.g. start and end times

	AgnEnSetText:(appt&,"Text")
	AgnApSetStartTime:(appt&,1998,12,25,13,0)

3. adding the entry to the file, i.e.

	AgnAdd&:(appt&)

4. finally freeing the entry in memory

	AgnEnFree:(appt&)

It is important that these steps are followed in order to avoid
memory leaks, panics and errors in your code.

Most of the functions require a memory handle returned by one
of the AgnEnNew* family of functions, the AgnFirstEntry& or
AgnNextEntry& functions, or the AgnFetch& function. If any of
these are called then it is important to call AgnEnFree at the
end.

Some functions take an id parameter which identifies the object
in the agenda file, e.g. AgnFetch&. This id can be obtained when
the entry is added using AgnAdd& or for an existing entry
which has been retrieved from the file use AgnEnGetId&.

OPX FUNCTIONS
-------------

The complete Agenda API is as follows:

To open and close the agenda file:

	AgnOpen:(filename$)
	AgnClose:

To scan through all the entries in an agenda use:

	AgnFirstEntry&:
	AgnNextEntry&:

To create a new entry in memory use one of:

	AgnEnNewAppt&:
	AgnEnNewTodo&:
	AgnEnNewEvent&:
	AgnEnNewAnniv&:

To read/write entries in the file use:

	AgnAdd&:(entry&)
	AgnModify:(entry&)
	AgnDelete:(entry&)
	AgnFetch&:(id&)

To set properties of any of the 4 types of entry use:

	AgnEnSetText:(entry&,text$)
	AgnEnSetSymbol:(entry&,symbol$)
	AgnEnSetAlarm:(entry&,dayswarning&,hour&,minute&,alarm$)
	AgnEnSetCrossOut:(entry&,flag%)
	AgnEnSetTentative:(entry&,flag%)

With AgnEnSetSymbol, pass an empty string to remove the symbol from an entry.
If you use AgnEnSetAlarm:(entry&,-1,-1,-1,"") then the alarm will be disabled.
The alarm$ parameter should be something like "Chimes", "Rings",... 
For AgnEnSetCrossOut and AgnEnSetTentative, flag%=1 or 0.

To retrieve properties of any of the 4 types of entry use:

	AgnEnGetId&:(entry&)
	AgnEnGetType%:(entry&)
	AgnEnGetText$:(entry&)
	AgnEnGetSymbol$:(entry&)
	AgnEnGetAlarm$:(entry&,BYREF dayswarning&,BYREF hour&,BYREF minute&)
	AgnEnGetCrossOut%:(entry&)
	AgnEnGetTentative%:(entry&)
	AgnEnGetStatus%:(entry&)

With AgnEnGetText$ don't expect to be able to retrieve embeded objects such
as pictures! If an entry doesn't have an alarm then dayswarning&=-1,
hour&=-1 and minute&=-1. The AgnEnGetType% function will tell you the type
of an entry as follows:

	0=appointment/day entry
	1=todo
	2=event
	3=anniversay

For AgnEnGetCrossOut% and AgnEnGetTentative% the return value is
either 1 or 0. AgnEnGetStatus%: can be used to determine if an entry
is marked as open (0), private (1) or restricted (2).

To free an entry in memory following a call to AgnEnNew*&, AgnFetch&,
AgnFirstEntry& or AgnNextEntry&:

	AgnEnFree:(entry&)

To set/get properties of appointment/day entries entries:

	AgnApSetStartTime:(appt&,year&,month&,day&,hour&,minute&)
	AgnApSetEndTime:(appt&,year&,month&,day&,hour&,minute&)
	AgnApGetStartTime:(appt&,BYREF year&,BYREF month&,BYREF day&,BYREF hour&,BYREF minute&)
	AgnApGetEndTime:(appt&,BYREF year&,BYREF month&,BYREF day&,BYREF hour&,BYREF minute&)

To set/get properties of todo entries:

	AgnTdAt&:(list&,index&)
	AgnTdSetList:(todo&,list&)
	AgnTdSetPriority:(todo&,priority&)
	AgnTdSetDueDate:(todo&,year&,month&,day&)
	AgnTdSetDuration:(todo&,days&)
	AgnTdGetList&:(todo&)
	AgnTdGetPriority&:(todo&)
	AgnTdGetDueDate:(todo&,BYREF year&,BYREF month&,BYREF day&)
	AgnTdGetDuration&:(todo&) 

The AgnTdAt& function can be used to loop over the todo's in a
todo list. This requires a handle on the todo list. Use AgnTdSetList to set
which list a todo belongs using the todo list id. With AgnTdSetPriority
the priority parameter should in the range 1-9. With AgnTdSetDueDate
if all the parameters are zero (except todo& of course) then the todo
will be made undated.

To set/get properties of events:

	AgnEvSetStartDate:(event&,year&,month&,day&)
	AgnEvSetEndDate:(event&,year&,month&,day&)
	AgnEvGetStartDate:(event&,BYREF year&,BYREF month&,BYREF day&)
	AgnEvGetEndDate:(event&,BYREF year&,BYREF month&,BYREF day&)

To set/get properties of anniversaries:

	AgnAnSetDate:(anniv&,year&,month&,dayofmonth&)
	AgnAnSetShow:(anniv&,flag%)
	AgnAnGetDate:(anniv&,BYREF year&,BYREF month&,BYREF dayofmonth&)
	AgnAnGetShow%:(anniv&)

There are a number of alternatve ways of setting an entry using Agenda. The
OPX AgnAnSetDate function assumes that it is an anniversary with a base year
which repeats yearly forever. For the show get/set functions above flag
parameter takes the following values:

		0=none
		1=base year
		2=elapsed years
		3=both

To read/write todo lists in the file use:

	AgnLiAdd&:(list&)
	AgnLiModify:(list&)
	AgnLiDelete:(list&)
	AgnLiFetch&:(id&)

To create a new todo list in memory use:

	AgnLiNew&:

To loop over the todo lists in an Agenda use:

	AgnLiAt&:(index&)

To set/get properties of todo lists:

	AgnLiSetTitle:(list&,name$)
	AgnLiSetOrder:(list&,order%)
	AgnLiSetViewDisplay:(list&,display%,hour&,minute&)
	AgnLiGetId&:(list&)
	AgnLiGetTitle$:(list&)
	AgnLiGetOrder%:(list&)
	AgnLiGetViewDisplay%:(list&,BYREF hour&,BYREF minute&)
	AgnLiChangePosition:(oldPos&, newPos&)

The order% parameter takes the following values:

		0=manual
		1=by date
		2=by priority

Use the AgnLiSetDisplayView function to control whether todo
entries are displayed in the other views, if display%=1 then
they are shown, 0 means they aren't. If todo's are displayed
then the hour and minute parameter controls the time slot they
are displayed at.

To free an entry in memory following a call to AgnLiNew&, AgnLiFetch&
or AgnLiAt&:

	AgnLiFree:(list&)