               DiveTime Version 4.01a
               ======================
               (c)1999 D R Cunningham


Introduction
============

DiveTime is a program written for the Psion 3a/c 
intended to simplify dive logging, especially dives 
made using a dive computer or dive timer. This is 
achieved by reducing the amount of information that 
has to be remembered. Additional information is 
calculated as required. Dive details can be 
displayed using either metric or imperial units as 
required.

An import facility is included for downloading data 
directly from an Aladin Dive Computer or from 
existing DataTrak files.

DiveTime also calculates air consumption, either 
from a dive profile or directly from a dive 
computer. Auxiliary information such as dive 
conditions, type of dive, activities and remarks 
can also be logged.

Previously logged dives are stored in a database 
allowing Total Dive Times, Average Air Consumption 
and Surface Intervals to be calculated. Air 
consumption trends can also be determined using a 
graphical display.

For dives downloaded from a computer, a dive 
profile display is available, including a choice 
of physiological profile and alerts.

Full information on how to use DiveTime is included 
in the comprehensive help file and it is strongly 
recommended that each of the help sections are read 
thoroughly. For further information on the Aladin 
interface, refer to the file Aladin.txt.


Installation
============

DiveTime comprises the following files:

    Dtime401.opa    (Program file)
    Divetime.pic    (Main dive display background)
    Divetime.rsc    (Optional Help file)
    Divetime.wdr    (Default Printer Setup file)
    Aladin.opo      (Aladin Computer Interface)
    Aladin1.pic     (Profile display graphics)
    Aladin2.pic     (Profile display graphics)
    Example.log     (Optional Example Log file)
    Example.pro     (Optional Example Profile file)
    Readme.txt      (This file)
    Aladin.txt      (Additional information on the 
                    Aladin interface) 
    Psicable.txt    (Additional information on the 
                    Psion serial interface)

To install, copy these files to the following 
locations:

    Dtime401.opa    Any Drive    \APP\
    Divetime.pic    Any Drive    \APP\DIVETIME\
    Divetime.rsc    Any Drive    \APP\DIVETIME\
    Aladin.opo      Any Drive    \APP\DIVETIME\
    Aladin1.pic     Any Drive    \APP\DIVETIME\
    Aladin2.pic     Any Drive    \APP\DIVETIME\
    Divetime.wdr    Internal     \OPD\
    Example.log     Any Drive*   \DIVE\
    Example.pro     Any Drive*   \DIVE\

* Log files will work from any drive but I strongly 
  suggest using either Internal or RAM SSDs due to 
  the number of file rewrites that will occur 
  during editing. New log files will normally be 
  created on the Internal drive.

Install the icon on the system screen using Psion-I 
(or the equivalent on non-English language machines).


Upgrading
=========

To upgrade DiveTime from an earlier version, 
uninstall the DiveTime icon from the system screen 
(Psion /) and delete all the files installed for 
the earlier version. Do not delete personal log 
files or any other files created by DiveTime.

Install DiveTime as described above. Personal log 
files created by earlier versions will be 
automatically updated when they are opened by 
DiveTime.


Registering DiveTime
====================

DiveTime is Shareware, if you find this program 
useful, you are asked to register it. The current 
price is GBP6.00, USD10.00 or EUR12.00 (or the 
equivalent in "Euro" currencies). The Aladin 
interface is an additional GBP4.00, USD6.00 or 
EUR8.00.

Currency (rounded up in notes) or cheques (in GBP 
only made payable to "D R Cunningham") should be 
sent to:

    D R Cunningham
    6 Windmill Court
    Sea Road
    Winchelsea Beach
    East Sussex, TN36 4LD

Please include your name, exactly as it appears in 
first line of your Psion Owner information 
(including any additional text), and address. If 
you have an email address include this as well for 
a speedier reply. Also state the version of 
DiveTime that you are using and where it was 
obtained (eg Friend, 3-Lib, Internet etc).

Until this program is registered, dive logs will be 
limited to a maximum of ten entries and the Aladin 
import functions will be disabled. Otherwise the 
program is fully functional.

Registering DiveTime is accomplished by selecting 
"Register" from the "Special" menu and entering the 
code supplied at the prompt. Registering the Aladin 
interface is accomplished by selecting "Register" 
from the "Dive", "Import" sub-menu and entering the 
code supplied at the prompt.

The latest version of DiveTime can be downloaded 
from:
        www.banana.demon.co.uk/dcunningham


DISCLAIMER
==========

This software is supplied "as is". The author 
disclaims all warranties, expressed or implied, 
including, without limitation, the warranties of 
merchantability and of fitness for any purpose.

The author assumes no liability for injury, loss or 
damages, direct or consequential, which may result 
from the use of this software.


Other Information
=================

If you have any comments, suggestions or complaints 
please contact me at the above address or by email 
at:
    dcunningham@compd.com

I cannot promise to include all suggestions but I 
will endeavour to fix all bugs notified to me. Bug 
fixes and upgrades will be free to all registered 
users.


Acknowledgements
================

DiveTime was written and compiled largely on the 
Psion 3c using Andy Clarkson's excellent OPP,
available from:

    http://ourworld.compuserve.com/homepages/andyc/

The main programme is based around the asynchronous 
event manager by Andrew Gregory and others, 
available from:

    http://www.unidata.com.au/psion/toolbox/


Background
==========

DiveTime started as two spreadsheets, one for air 
consumption and the other for calculating dive 
times after the dive (similar to "Append"). These
worked well but I decided to write an OPL program 
as an exercise in programming on the Psion.

I have used this program successfully on a fairly 
intensive diving holiday and without showing any 
"buggy" behaviour and I am very pleased with it's 
operation

I added the Aladin interface so that I could view 
my dive profiles immediately after a dive and not 
have to wait until I could get to a PC (usually 
after the holiday). After much surfing of the net 
I found enough details of the data formats used 
to write the interface (for a list of the 
references see Aladin.txt).

This interface is still relatively untested yet 
(hence the beta release) since I have not been 
able to analyse a variety of downloads. If you 
experience problems analysing the download data 
could you please send the saved download file 
together with the DataTrak log created from it 
(or at least some idea of the dives contained) 
and the type of dive computer used.


History
=======

Version 1.05a - First released version

Version 2.01a - Functionally identical to 1.05a but
    rewritten to use the "Programmers Toolkit" (see 
    "Acknowledgements" above). Not released.

Version 2.03a - Additional dive information now 
    logged and switch to symbolic presentation of 
    the main dive details.

Version 2.03b - Minor fix to Help command.

Version 3.01b - (beta release) Re-arrangement of 
    menu options. Addition of Aladin interface and 
    dive profile display.

Version 3.01c - (beta release) Correct decoding of
    Nitrox downloads. Display of Nitrox mix and 
    manual entry of Nitrox mix.
    
Version 3.01d - (beta release) Minor correction to
    Nitrox mixture entry.

Version 3.01e - (beta release) Removal of initial 
    credit (version can still be found from "About" 
    under the "Special" menu (Psion +)

Version 3.02a - Addition of imperial units display, 
    corrections to air consumption graph display.

Version 3.02b - Correction to Average Air Cons 
    calculations

Version 4.01a - Change of Dive Log formnat from OPL 
    database format to binary file. This overcomes 
    the problems of updating the whole file, even 
    for minor edits.
