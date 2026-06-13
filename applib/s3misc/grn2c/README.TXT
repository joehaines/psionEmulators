GRNotes Version 2c
==================

For the Psion 3a/c/mx
=====================

Copyright 1998, Gavin Ryalls
============================


*** Description

GRNotes is an application for keeping random notes in a structured sequence whilst allowing easy recall and cross-reference. Psion Word, Sheet or Data files may be attached to each note.

The program was developed from a desire to make the Psion perform the same job as a Filofax - not just keeping information in one place, but also organising it into logical sections. Notes records are stored in folders (think of DOS directories and sub-directories). The user can search down the folder path to retrieve groups of notes or else by searching a key field, a date, or by using a search string. There is also a shortcut facility which lets the user bookmark frequently used records or folders for immediate recall.
The program can be used to organise task lists, forward planning, contacts, projects and general reference notes. It is also very useful for grouping together related Psion app files and can serve as a launcher for the built-in apps.

*** Installation

You should have the following files which need to be copied to the indicated directories on the Psion:

grn2c.opa		\app
grnotes.rsc (help file)	\opd
grnotes.ini		\opd

You will also have to create the following directories on the drive where you installed the program files:

\NOTE
\APP\NOTE

Install from the system screen using psion-I. Select Grn2c.opa and press <Enter>.

*** General Operation

<Up-arrow> and <down-arrow> move between records. <Enter>, on a folder, will list sub-folders and notes. If pressed at a note, any linked file will be launched, or if none, a dialog will open to create a new link. <Left-arrow> lists the parent folder. <Psion-left-arrow> lists the root folder. Delete operates the same as psion-D. Tab displays a note's contents.

Menu items:

File
Open		psion-O	Opens a new file
Exit		psion-X	Closes program

Edit
Record		psion-T	Edits a single note.
Copy		psion-C	Copies the note title for use with bring in other apps
Bring		psion-B	Creates a note using highlighted data from another app

Search
By date		psion-L	Lists all records for date selected
Find		psion-F	Finds all records containing entered string
By type		psion-Y	Lists all records of a type (see note types below)

Shortcuts
Toggle		psion-S	Adds or removes a note from the shortcut list
List		psion-I	Lists all notes on shortcut list

Records
New record	psion-E	Creates a new note
New folder	psion-R	Creates a new sub-folder in the current folder
Delete		psion-D	Deletes a note
Move		psion-V	Deletes a note from its current folder
Paste		psion-P	Inserts a note Moved from another folder

Link
Show		psion-W	Displays the name and path of a linked file
Cut		psion-U	Removes a file link from the current note

Special
Register	Registers GRNotes
Font style	Selects Roman or Swiss font
About		Shows program information

*** Note Types

If a record is created with <types> (including brackets) in the header line , a string of user-defined note types entered into the first text line will be used as a selection list when adding or editing notes. Use commas between each type. I use

"note,waiting,question,task,contact,reference,goal,done".

Where no <types> record exists, note headers are entered freeform.

*** Registration

GRNotes is Shareware. If you continue to use it, you must register. Please send a cheque for GBP10.00, payable to Gavin Ryalls at

1 Foulden Place
Dunfermline
Fife KY12 7TQ
Scotland

You will be sent a user password to remove nag screens and will be entitled to free upgrades and bug-fixes. Please report all bugs and requests to gryalls@cix.co.uk. Registration will encourage further development of the product.

*** Version History

Version 2a
* Status window control introduced
* Zoom control and choice of font styles added
* Copying between folders added
* When jumping to a linked file, the program will now test to see if that file is already open, and if so will bring it to front rather than show a 'file in use error'.
* Search by key removed as superfluous.
* Various bug fixes and tidying-up.

Version 2b
* Re-issue with .ini file

Version 2c
* When moving up the tree structure towards root, cursor now highlights parent folder.
* Item dates now included in new item and edit boxes.
*Note display key moved to <Tab> to avoid clash with backlight control on later machines.

*** Acknowledgements

GRNotes makes use of code written by other authors obtained in Cix psion/files:

Link paste server/client:
Original code written in C by Colly
Adapted and translated into OPL by Tom Dolbilin

Icon:
Created by Phil Creed

[end]