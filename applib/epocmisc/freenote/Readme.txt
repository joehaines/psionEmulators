FreeNote for EPOC32
version 3.04  31 August 2003
Copyright (C) UK 1999-2003 Philip Bister

INTRODUCTION
FreeNote is a place to store snippets of information that
you may wish to refer to at a later date.

FreeNote allows the user to create files, so that all
notes that are related (your work for instance) are kept
together.

Within each file, notes are stored under tab headings,
to help keep everything filed neatly. Fourteen tabs
are available for each file.

Users can set up own labels for the tabs, and set a 
lifespan limit for the file so that FreeNote can 
automatically delete old notes.  Automatic deletion can
be switched off if not required, and of course, users can
manually delete notes and edit them as well!

FreeNote features zoom for the note display window (4 levels)
and paragraphs and tabs are also shown. Each note has a 
maximum size of 980 characters.

The currently displayed note can be sent to your printer
- and printed just as its shown on the screen!

FreeNote also now safely backs up your last opened file.
Although you may never need to resort to a backup, its
a useful feature to have - just in case anything goes wrong.

Zoom settings are saved from one session to another.

SUITABLE MACHINES
FreeNote 3.04 runs on all the following EPOC 
powered computers:
Revo, Revo Plus, Series 5, Series 5mx, Series 7, netBook, 
netPad, Ericsson MC218, Diamond Mako, Osaris.

FREENOTE FILE CONTENTS
The archive file contains the following files:
FreeNote.sis
Readme.txt

INSTALLATION INSTRUCTIONS
Copy FreeNote.sis to any folder on your EPOC computer.
Double tap on the FreeNote.sis file.  This will start the
automatic installation process.
When installed, FreeNote will be accessible from the 
Extras bar.

IF YOU ARE UPGRADING FROM v2.xx
Version 3.00 and upwards uses a different database 
structure from previous versions. 
Therefore if you wish to view your existing notes in v3.xx,
they will have to be transferred with the FnUpGrade program.
FnUpgrade is available from my web site.

COPYRIGHT
FreeNote is Copyright (C) UK 1999-2003 Philip Bister.
All rights reserved.  
Reverse engineering/translation is prohibited.

DISTRIBUTION
You are free to distribute this program providing all the
original files remain intact and unmodified.

DISCLAIMER
This software is supplied 'as is'.  Every effort has been 
made to ensure that this software is free from errors.
However, no software author can guarantee it.
The author takes no responsibility as to the suitability
of this products intended purpose or for the accuracy of 
the data contained within it, or for any consequences or 
damages which may occur as a result of using this program.
**YOU MUST USE THIS SOFTWARE AT YOUR OWN RISK.**

ACKNOWLEDGEMENTS
With thanks to:
Steve Litchfield, for distributing FreeNote via the 
Palmtop/3-Lib CD ROM.
Al Richey, author of S5Event v1.2
Richard Smedley, author of SafeOPL32
Kevin for testing FreeNote on the Osaris and Mako computers.
Without Kevin's help, FreeNote would not be the program
it is today.
Jan Gustafsson and Itamar Engelsman for helping to improve
FreeNote by testing and suggesting improvements.
Mark Fitzpatrick for releasing Clipboard v1.00
Roger Muggleton for releasing Multi demo


CONTACT
If you have any points concerning FreeNote, I can be
contacted by e-mail:
philipbister@ukonline.co.uk

The latest version and my other free software can be 
downloaded from my web site:
http://www.philipbister.ukonline.co.uk

VERSION HISTORY
v3.04 31 Aug 2003
• Introduced an extra set of 7 tabs which can be accessed
by Ctrl+Up and Ctrl+Dn
• Automatic sorting can now be switched on/off.
• A small improvement to the menu text (tool menu card)

v3.03 12 July 2003
• Sliding index page indicator bar was not calculating
correctly - this is now corrected.
• Index page marker array is now being correctly reset.
• Book Mark is now being correctly reset.

v3.02 15 June 2003
• Improved index page indicator control code.

v3.01 11 May 2003
• Users can now select a path to backup to, and FreeNote
only backs up when the data has changed.
• Improved note headers from note body text.
• Fixed problem of text window not 'scrolling' properly
when running on a Series 5 (or compatiable).
• Added an extra zoom level to text window (now 5 levels)
• Index page indicators. Modified the code slightly to
give better visual indicators.

v3.00 27 April 2003
• A number of users asked if the note size could be
increased. As a result, FreeNote now stores notes up to
980 characters in length.
• Ctrl+Y has been removed from the dialogs. now just Y

v2.27 06 April 2003
• Previous versions occasionally suffered from a corrupt
user file which prevented the program opening. This now
should not happen, as FreeNote will 'self repair' this error.
• The sound can now be toggled on and off and the settings
saved from one work session to another.

v2.26 01 April 2003
• The previous version had a divide by zero error which
occured when opening an empty database or a database without
data filed on all tabs. This has been corrected.
• Ctrl+Tab now highlights the first tab, instead of alternating
between the first and last.

v2.25 29 Mar 2003
• Exit help bug now finally fixed.
• Text now 'cleans itself' when opening a new file.
• Tab labels saved when creating or opening a file.
• A new convertor for note headers has been incorporated
into this version, so that tabs or newline characters are
not shown in headers.
• Program 'stands on new note' when a new note is created.
• New keyboard control: Ctril+tab  to alternate between
tab1 and tab7.
• Find functions added to menu, and the key press Escape
close find mode.
• label change bug fixed.
• Page indicator flags and floating index page indicator
added to this version.
• Printer font sizes are now set according to zoom level.
• Changes to clipboard: FreeNote now has an internal
clipboard and an EPOC clipboard for importing text.

v2.24 01 Mar 2003
• Finally fixed the bug of tab 7 border not drawing.
• Improved tab borders
• TextReader can now cope with long text strings containing
no spaces or paragraphs.
• The printing section has been updated to reflect changes
in TextReader
• Headers only now accepted (problem with v2.23 only)
• FreeNote now saves as much as possible of a text that is
over-quota.
• Improvements to code for detecting hot-keys
• Several hot-keys have changed
• Find buttons repositioned to avoid concealing some of
the displayed note
• Note info window now introduced.
• Colour has been introduced (except Osaris)

v2.23 02 Feb 2003
• Corrected a small bug to stop FreeNote from asking if
the user wanted to exit program whenever the help file
was exited.
• Program 'froze' when 'End Find' button was tapped.
This has now been fixed, and the program now goes back to
the beginning of the file.
• Program now makes use of the EPOC clipboard rather than
it's own clipboard.
• Problem fixed with border on tab 7 not drawing properly.
• Shift+tab has now been added to move tab highlight from
right to left.
• Week number added to date window.
• Change of hot-keys:
Control +s to save a new note, Control+c to copy a note
• New code for New note and Edit note, allowing the program
to respond to paragraphs and tabs.
• A brand new text reader has been written for FreeNote
allowing the program to correctly align text for paragraphs
and tabs.
• Four levels of zoom added for text window, plus new hotkeys:
Shift+up or down cursor keys to flip up or down a text page.
• Zoom level is saved from one session to another.
• Osaris users:
Text screen width increased to 60% of screen width.
• The maximum number of characters per note has been
increased to 255 for all EPOC machines.
• The printing section of FreeNote has been partly
rewritten. The program now prints the currently displayed
note, and just how it is displayed on screen!

v2.22 Dec 2002
• Added error code to prevent overflow problems when 
	copying from a text file.
• FreeNote now automatically creates a backup of the last
	opened file.

v2.21 Nov 2002
• Find notes feature added. 
• Fixed open file command bug.
• Tab labels now display correctly after using open file 
  command.
• Expanded note window now updates automatically after new file 
  has been opened.
• Edit note error has been fixed.

v2.2 Nov 2002 beta test version
• Configured to run on the Series 5 classic and Osaris.
• Improvements to text line width in expanded note window
  and some changes to dialogs.
• Fixed a small defect of the expanded note window not 
  re-drawing after using edit tab label dialog.

v2.1 Jly 2002
• Improved user interface making program easier to use.
• New code for handling note selection in index screen.
• New code for handling file names.
• Better dialog code to ensure they will display properly 
  on 1/4, 1/2 and VGA screens.
• Various bug fixes.

v2.0 Feb 2000 First EPOC32 version.

v1.1 Oct 1999 First release for Psion Series 3a/3c/3mx.
