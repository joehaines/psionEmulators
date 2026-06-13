				 
			    README.TXT

		    SOUND PRESSURE LEVEL CALCULATOR
		WITH SPEAKER DATABASES AND CONVERSIONS

			 Version 1.01

	  (C) Copyright Colt Tech, 1995

This document contains notes for the SPL Calculator
Version 1.01 for Psion 3A.

------------------------
DISCLAIMER
------------------------
As the information contained within this program is based on 
manufacturers published data, the Author takes no responsibility for 
its' authenticity or the accuracy of any calculations based on 
said data

The disk and programmes contained in it have been virus checked at
source. However, the Author takes no responsibility for any bugs or viruses
which may arise from using this program and the program is run at the
operators own risk.


------------------------
How to Use This Document
------------------------
To view README.TXT on screen in Windows Notepad, maximize the Notepad 
window.

To print README.TXT, open it in Windows Write, Microsoft Word, or another 
word processor.  Then select the entire document and format the text in 
10-point Courier before printing.

     *******************************************************************
     Read Part 1 - Software Installation Information - before installing.
     *******************************************************************

========
Contents
========

Part    Description
----    -----------
 1      Software Installation Information
 2      About the SPL Calculator
 3      SPL Calculator
 4      Speaker review, mods and entry
 5      Printing to file
 6      Cable line loss calculator
 7      Cable data modifications
 8      Printing
 9      Conversions
10      Shareware registration

=========================================
Part 1: Software Installation Information
=========================================

Both these programmes need  approximately 70K free memory
to run.

PATools comprises TWO applications;
	Sound Pressure Level Calculator
	Cable line loss calculator
Both or either of these programmes can be loaded onto the PSION
and some of the files are shared. It is possible to create a PATools
GROUP  with  PATool.IBX as the icon (see the Psion handbook for more
details).

To install either of the PATools create a PATOOLS subdirectory off the
APP directory where the program is to be loaded. Load the following files 
into yourPsion 3A in the appropriate directory:

For the SPL Programme only install the following

	     FILE       SIZE       DIRECTORY
	README.TXT                                      This file
	Speakers.OPA    12.9K   \APP            The SPL prog
	PATool.IBX      600     \APP            The program icon
	Cabinet.DBF     3K      \APP\PATOOLS    Database
	Horn.DBF        3K      \APP\PATOOLS    Database
	Ceiling.DBF     3K      \APP\PATOOLS    Database
	REORDER.OPO     300     \APP\PATOOLS    Re-order
	Conv.OPO        7K      \APP\PATOOLS    Conversions
	Database.OPO    2K      \APP\PATOOLS    Database handling

For the Cable loss calculator only install the following

	     FILE       SIZE       DIRECTORY
	README.TXT                              This file
	Cable.OPA       12.4K   \APP            The Line Loss prog
	PATool.IBX      600     \APP            The program icon
	Cable.DBF       400     \APP\PATOOLS    Cable database
	REORDER.OPO     300     \APP\PATOOLS    Re-order
	Conv.OPO        7K      \APP\PATOOLS    Conversions
	Database.OPO    2K      \APP\PATOOLS    Database handling

For BOTH install the following

	     FILE       SIZE       DIRECTORY
	README.TXT                              This file
	Speakers.OPA    12.9K   \APP            The SPL prog
	Cable.OPA       12.4K   \APP            The Line Loss prog
	PATool.IBX      600     \APP            The program icon
	Cabinet.DBF     3K     \APP\PATOOLS     Database
	Horn.DBF        3K      \APP\PATOOLS    Database
	Ceiling.DBF     3K      \APP\PATOOLS    Database
	Cable.DBF       400     \APP\PATOOLS    Cable database
	REORDER.OPO     300     \APP\PATOOLS    Re-order
	Conv.OPO        7K      \APP\PATOOLS    Conversions
	Database.OPO    2K      \APP\PATOOLS    Database handling

You need 44K of free space on the disk for the full installation

======================
Part 2: About the SPL Calculator
======================
----------------------------------------------------
Overview
----------------------------------------------------
The SPL calculator is a tool to aid designers of Public Address 
installations by calculating sound pressure levels for 
a range of speakers.

For the SPL Calculations three databases are provided (for Horns/
Projectors, Cabinets/Columns and Ceiling/Suspension) each containing 
the following information;
	
	Part number
	Speaker name
	Tappings available (up to 6)
	Sensitivity (dB @ 1W1m)
	Description
	Manufacturer

The databases are opened by selecting from the MENU

	Horns   <Psi H>
	Ceiling <Psi C>
	Cabinet <Psi B>

The user calls up an item from the databases provided and uses these 
to form the basis of the calculations. The results can 
be exported to a text file.
 
====================================================
Part 3: Sound Pressure Level Calculator
====================================================
-----------------------
The Screen
-----------------------
The SPL Calculator screen can be split into two areas;
	Data Input
	Results

-----------------------
Data Input
-----------------------
Speaker database. Three databases are provided (for Horns/
Projectors <PSION H>, Cabinets/Columns <PSION B> and Ceiling/Suspension
<PSION C>).

Speaker part no.  This is shows the part number of the selected speaker
with the Manufacturers name (if supplied). To select a new speaker use
<Psion S>

Description.  The description associated with the accessed speaker

Sensitivity.    The sensitivity is given in dB @ 1W1m. This data 
is supplied from the database. The figure can be updated in the 
calculator screen without affecting the database.
To enter a different sensitivity to that supplied in the database
use <PSION V>

Distance to subject (r).  Enter the straight line distance in meters 
to the listener (ie apply Pythagorous if the subject is at an angle). 
The results are automatically re-calculated if this figure is 
changed. Enter the distance using the DATA menu <PSION D>

Required SPL at r.  Enter the Sound Pressure Level required at the 
listener position. Enter the SPL using the DATA menu <PSION D>

Off axis angle. Enter the off axis angle of the listener. A general 
angular response has been allocated to each speaker group as follows;
	
	Horn;
		30 degrees      +0dB
		60 degrees      +6dB
		90 degrees      +14dB
		120 degrees     +16dB
		150 degrees     +20dB
		180 degrees     +20dB
	Ceiling
		30 degrees      +1dB
		60 degrees      +5dB
		90 degrees      +8dB
		120 degrees     +12dB
		150 degrees     +15dB
		180 degrees     +15dB
	Cabinet
		30 degrees      +2dB
		60 degrees      +5dB
		90 degrees      +8dB
		120 degrees     +13dB
		150 degrees     +15dB
		180 degrees     +15dB

IMPORTANT;  THESE FIGURES ARE EXTREMELY GENERAL AND SHOULD ONLY 
BE TAKEN AS AN INDICATION. CHECK THE MANUFACTURERS DATA SHEET FOR 
ACCURATE INFORMATION.

Enter the angle using the DATA menu <PSION D>


-----------------------
Results
-----------------------

SPL @ 1m.  The Sound Pressure Level 1m from the speaker needed to 
achieve the SPL at the listener.

Theoretical tap.   The tapping required to achieve the SPL at 
the listener.

Actual Tap.   The closest tapping available on the speaker 
selected.  This figure can be manually set (for example is non standard 
speaker is being used). 

Actual SPL.    The SPL at the listener achieved with 
the speaker set at the ACTUAL tap.


====================================================
Part 4: Speaker data review, modification and entry
====================================================

Speaker data is contained in the following files
	Cabinet.DBF             \APP\PATOOLS
	Horn.DBF                \APP\PATOOLS
	Ceiling.DBF             \APP\PATOOLS

Review, modify or add speakers in the DATA application.

There are a number of rules associated with the databases:

	The programme does not allow blank text fields (ie
	Description or Manufacturer. Use a <SPACE>

	TAP 1 is the HIGHEST available tap down to the LOWEST in
	TAP 6.

	ALL the TAP fields must be used. If there are less than 6 taps
	the last fields MUST contain the LOWEST tap.

A small application is included in the programme called ReOrder
<PSION L> which puts the selected database into NAME order. This is
useful as any changes to the databases messes the order.


=============================================================
Part 5: Printing  to File <PSION P>
=============================================================

The PRINT>FILE option allows the user to print to a TEXT file.

The calculator allows the user to save the results 
into a file which can be read in Word. Once opened the file is 
ammended each time it is printed to allowing multiple results 
to be saved.

IMPORTANT;  If a file already exists when the calculator is started it
will generate a new file with the same name and NOT ammend it.

=============================================================
Part 6: Cable Line loss calculator 
=============================================================

The Line Loss Calculator works out the loss at each speaker on a NON LOOP
BACK speaker circuit. It bases the calculation on cable attenuation, 
average speaker tapping and spacing.

The calculator uses a database of cable characteristics which can 
be updated.

The screen comprises a number of areas
	Cable spec (single and dual cable options)
	System Data
	Results

-----------------------
Cable spec
-----------------------
If a dual cable solution in required use the TWO CABLE <PSION T>
option. This brings up two cable selections

Cable Cross Section;   To call up a standard cable from the data 
choose CABLE <PSION C> from the menu. The calculate function is
carried out automatically on each selection

Impedence;  Calling a standard cable inserts the figure. This
figure can be changed manually using the IMPEDENCE <PSION I> from
the menu
-----------------------
DATA <PSION D>
-----------------------

Distance to first speaker/end of first cable;  Enter the distance 
from the amplifier in meters. 

(If TWO CABLE option chosen). Distance to first speaker;   This is the distance
in meters from the end of the first cable to the first speaker.

Average speaker tapping;  Entered in Watts. 

Total number of speakers

Average speaker spacing;  Distance in meters between speakers (not
including the distance to the first speaker).

Amplifier line voltage <PSION V>

-----------------------
Results
-----------------------
The main results box gives the loss at the speaker furthest from the
amplifier. Also given is the new effective power of the speaker
(ie the loss of the line has the same effect as reducing the tapping
at the speaker)

=============================================================
Part 7: Cable data modification and entry
=============================================================

The Cable Data is kept in a standard database which can be updated with
the Psion DATA application. The database should be Re-Ordered <PSION L>
when changed

=============================================================
Part 8: Printing <PSION P>
=============================================================

The print option allows the user to print to a file.

The calculator allows the user to save the results 
into a file which can be read in WORD or imported into 
most Word Processors.  Once opened the file is ammended each 
time it is printed to allowing multiple results to be saved.

IMPORTANT;  If a file already exists when the calculator is started it
will generate a new file with the same name and not ammend it.

-----------------------
Cable Line Loss Calculator
-----------------------
Prints the results as seen on the screen Plus the line loss list.

Loss list;   This is a list of the losses at all the speakers on the line
(starting at the speaker closest to the amplifier). 

=============================================================
Part 9: Conversions
=============================================================

Some useful conversions are available from the Conversions
menu <PSION N>. This brings up the menu. To return to the
tapping calculator select Return <PSION U>
The conversions include;
    Length (m > cm > inches > feet > yards)
    Area (m sq >  sq feet > sq yards)
    Volume (m cu>  cu feet > cu yards)
    Acoustics (RASTI > %ALcons)

Some useful standards are available from the
Conversion menu including;
    Absorbtion of  Materials
    Common Reverberation Times

=============================================================
Part 10: Shareware Registration
=============================================================

PA Tools V1.01 1995 from Colt Tech is SHAREWARE, not free 
software. If you continue to use PA Tools you MUST register. 

Registration removes the "Pain" screen, removes the limit 
on the no of items loaded from the databases  (set at 20 for 
unregistered users) and allows printing of results to file.

It is as simple as entering a special key-code at the Register
menu <PSION R>

To register send 10 Pounds Stirling to;
I.Penson,42 The Colts,Bishops Stortford,
Herts,CM23 4DL, ENGLAND


