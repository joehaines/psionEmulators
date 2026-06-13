Introduction: 
------------
Event Convert is a program designed to reformat the "List" view of calendar
items from Agenda into a full page one month calendar. It works by taking a text
file generated from Agenda and reformatting the events into a calendar
"template" file. The calendar file can then be opened by Sheet and printed in
landscape orientation to any supported printer in any supported font (within
reason).

Updates will be posted on the CompuServe PALMA forum. Event Convert V1.1 runs 
on Psion 3a palmtops and the Psion 3a Emulator.

** Note: The current version V1.1 does not print the Day and Date in every cell,
only those days that have events.



Disclaimer:
-----------
Use Event Convert at your own risk. No warranty is given nor implied. This
program is Freeware. The author reserves ALL RIGHTS. Reverse translation of this
program is forbidden.

Copyright 1995 Calvin Grier 76330.3425@compuserve.com The author acknowledges
the help of Matt Millar in creating  Event Convert. Special thanks to David 
Stewart Price for help with the AGN2LIS.MCR macro.

If you find this program usefull please drop me an email message to let me know.



Installation: 
------------- 
The file EVENTCNV.ZIP contains the following files:

 EVENTC11.OPO - Psion 3a OPL object file
 README.TXT   - this file
 FILE_ID.DIZ  - short description
 TEMPLT.SPR   - Sheet file template for 1 month calendar: 9 rows/day
 TEMPLT2.SPR  - Sheet file template for 1 month calendar: 8 rows/day
 PSIWIN_T.SPR - Sheet file template for 1 month calendar: Psion Print
 AGN2LIS.MCR  - MacSys macro system file for creating a calendar file

 1- Copy EVENTCNV.OPO to any \OPO directory on either M:, A:, or B:
 2- Copy *.SPR to the \SPR directory on your default disk.
 3- Open TEMPLT.SPR from Sheet. Select the correct printer device and driver
 4- Change "Font 1" to 8 point from the View menu. Use 7 point w/ Psion Print
 5- Try printing. If TEMPLT doesn't print right, try TEMPLT2 or PSIWIN_T.SPR.
 6- Once printing works well, save the template and exit Sheet.
 7- Repeat 3-7 for each addl. printer; use unique template filenames for each
 8- You may now start generating lists from Agenda.



Instructions: 
------------- 
The most challenging part of using Event Convert is to print (to a file) the
correct information from Agenda. After a month's worth of Agenda items
have been printed to "\AGN\P.LIS", Event Convert copies a Sheet template file 
and transfers the events into it. Printing is done from Sheet. Multiple printers 
can be accommodated by creating slightly different Sheet templates.

Below is the procedure for creating a one month calendar:

1 - Create a List: 
From Agenda select Print Setup. Set Margins to 0,0,0,0. Delete any text
appearing in the Header or Footer fields. Select "General" for the Printer Model
and "File" for Printer Device. The filename should be "P.LIS" in the AGN
directory of the default disk. Press <Enter> to close the window. DO NOT 
DEVIATE from this print setup. Otherwise Event Convert won't find your list.
If an AGN directory does not exist on the default disk you must create it.

From Agenda select Print. It is simplest to select "This Month" or "Last Month"
for Range of Entries. However if you want a different month, the start and end
dates must be entered manually. The remaining screen options are at your
discretion. Printing is quick, and overwrites any previous list. You can look at
the text file from Word if desired, but remember not to save any changes.

2 - Run Event Convert: 
Locate the "Run OPL" icon from your system icon menu (it looks like a gray bubble
with OPL written in it). Highlight EVENTCNV file and press <Enter>. The program
simply asks for the output file, template file, and the number of rows
available for each day. Base the number of rows per day on the template selected.

The Sheet template TEMPLT.SPR that comes with Event Convert is configured for a
maximum of 9 rows per day. Column width and the number of rows per day can be
altered to fit on various paper sizes. In the Zip file I have also included 
TEMPLT2.SPR with only 8 rows per day and PSIWIN_T.SPR for users of Psion Print.

3 - Run Sheet:
Open the output SPR file created by Event Convert. Sheet lets you examine and
change the calendar before printing. Troubleshooting printing of the calendar
should be done with the template file, since all changes to the template will be
reflected in the actual calendars. Select Print from the Special menu. Select
the range Worksheet and press <Enter>.

If you have trouble printing:
 - make sure the printer supports Landscape printing
 - check the calendar from Sheet before printing - does it look right?
 - make sure Font 1 is set to a 7 or 8 point monospaced font like such as Courier
 - check the printer model and the printer device selections
 - try TEMPLT2 and make sure to select 8 events per day in Event Convert
 - see Hints/Tips


Hints/Tips:
-------
A macro for use with MacSys by Tom Dolbilin has been included. It is commented 
to make customization easier. This is highly recommended for frequent users.

If you have trouble getting the column borders to line up, or want to allow for
long Agenda entries: choose Custom for Page Size when printing to P.LIS from
Agenda. Make the page width narrow, about 7.5 cm, and Agenda will word-wrap your
long entries.



Have fun!

