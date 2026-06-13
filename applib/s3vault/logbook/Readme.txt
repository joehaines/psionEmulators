LOGBOOK v1.0 (c) Dr.C.T.Hopkins May 1999
Contact me at Gasdoc@hotmail.com 

INTRODUCTION
REGISTRATION
INSTALLATION
USING THE PROGRAM
BASIC OPERATION
NORMAL VIEW
FILE VIEW
KNOWN PROBLEMS

INTRODUCTION

Logbook is an Anaesthetic logbook, which allows a record to be kept of all patients you have 
Anaesthetised. Entries are stored on a database and can be organised by the program to produce 
printed reports of your activities.

It was written by myself to provide a means of keeping track of my anaesthetics and stores 
information in the format I first started using on paper.

It has been used extensively by myself over the last 3 years on both a Series 3a and 3c. 
It will recognise which machine it is running on and adjust printer communications 
appropriately. I have not had occasion to test it on a Series 3mx but there is a good chance 
that it will work. If anyone does try it please let me Know. (Contact at Gasdoc@hotmail.com ) 

It does not allow you to store any patient details that can be tracked back to a patient's 
notes, and therefore keeps patient confidentiality. You are reminded that you MUST respect the 
Data Protection Act.

I accept no responsibility for any damage to or loss of files caused as a result of the program.



REGISRTATION

This program is Shareware but use of the program is unrestricted from the outset. 
You are welcome to distribute the program, but must do so in the form of the original ZIP file. 
You may make no changes to it. 

You can register this program for FREE by sending an Email to me at gasdoc@hotmail.com . 
Registration will provide you with a code which will disable the nag screens and also provide
me with some feedback as to how many people are interested in it. On registration I will try to
provide as much support as I can, also I am very open to constructive criticism so I can improve 
future versions. So please take the short time to register.



INSTALLATION 

The program is distributed as a zip file. This can be Unzipped by double clicking on the file 
Icon. The unzipped program consists of 5 files. This Readme file, An example blank data base in 
the format the program uses (Logbook.dbf) and 3 Application files (Logbook.opa, Graph.opa and 
View.opa)

Logbook.dbf may be copied into your \DAT\ directory on any drive.
Logbook.opa, Graph.opa and View.opa may be copied to the same \APP\ directory on the Internal 
drive. The files Graph.opa and View.opa must be in the \APP\ directory of the Internal drive 
but Logbook.opa may be on any drive.



Next you must go to the System screen on your Psion and select Install from the Apps Menu. 
When the dialog box appears use the cursor arrows to select Logbook.opa and press Enter. 
Logbook will now be ready to use and the program Icon should have been added to the System
screen.

Whereas Graph.opa and View.opa are also similar files to Logbook.opa they donot need to be 
installed and simply need to sit in you \APP\ directory on the same drive as Logbook.opa.

When the program is run by Clicking on the Icon the program will make a file in your \OPD\ 
directory where it stores it's default values.




USING THE PROGRAM

When the program is run for the first time an introductory screen appears to confirm that 
you agree to the terms of usage. If you accept you are taken to a dialog box which will ask 
you to set the default values.

First you must select the default file which you will use. This is the file that you copied 
into Your \DAT\ directory called Logbook.dbf. You must use this file in the first instance , 
but you can later copy it and rename it when it starts to get full. I use a new file for every 
year. The default file Logbook must be selected on whichever drive you copied it to otherwise an 
error will be returned when you try to use the program. Next you are asked to set a reference
prefix. What this means is that one of the things that you are able to record for a patient is 
a reference number. This number should NOT be the hospital number but an independent number that cannot be linked to the patient. I use an automatic numbering system that uses an alphabetic prefix followed by an increasing number based on the number the record represents in the data file.

Lastly set a default printer driver. This is restricted at the moment to Epson, Cannon and a General driver. The only difference being that a general driver does not allow you to print to organiser paper. On pressing enter you proceed to the main program.



BASIC OPERATION

On running the program you are sent to the basic background screen. There are two basic modes, The Normal view and the File view. These modes are interchangeable by pressing the diamond key. The Normal view is used to enter a new record and for making tables from these records. The File view is used to view the records that have been entered and to search them for a particular entry.



NORMAL VIEW

The main options are accessed by pressing the menu key or by pressing a HOT KEY. On the left of the menu is the file menu. This consists of the options CHANGE FILE, LIMIT FILE, NEW RECORD, CHANGE CURRENT RECORD, SAVE RECORD, SAVE AND CREATE NEW and EXIT.

NEW RECORD

This sends you to a series of boxes where you enter the details for your patients. Some options are already there for you to select whilst Operation name, for example, has to be entered by you. If you select Yes for adverse events then you are taken to more menus to select up to three adverse events and their consequences. Once you have made all your selections they are displayed on the main screen. You now select SAVE from the file menu or SAVE AND CREATE NEW which saves the record to your database and automatically starts NEW RECORD.

CHANGE RECORD

This allows you to make any changes to the current record before saving.

CHANGE FILE 

This simply changes the current file.

LIMIT FILE

This is a primitive auditing function that allows you to limit your data file by searching for files which meet certain criteria only. The search is saved to another file so that you can then limit this file and, in doing so, make successive searches to limit your original search.


TABLES 

This section of the menu allows you to Create tables of Results based on your data file. I would recommend using one file for each year to allow full year searches and limit the time to create tables. I have taken SUPERVISION to include those cases where supervision was in theatre itself or the suite.

GRAPHS 

Three simple graphs are able to be produced based on your activity.

SPECIAL menu allows you to change the original PREFERENCES or to make printed records.

PRINTING

The tables but not the graphs can be printed from here. Make sure that the printer is connected correctly. For the Series 3c version only the machine will wait for up to 30 seconds for you to check connections after a CHECK ON LINE error has been created. The 3a version exits if the printer was not connected.
You cannot print to organiser paper with the general driver.



FILE VIEW

This allows you to browse the entries you have already made one by one. Also the various events and procedures that you may have entered in the NORMAL VIEW are coded before saving to your file, so the file view allows you to view the original text you saved by selecting either LIST EVENTS or LIST PROCEDURES. Simple non case specific searches can also be made. Wild cards are supported.


HELP

A very basic help section can be found by pressing the HELP key. It is very limited at present but will be expanded upon in the future.




KNOWN PROBLEMS

There is a very limited error handling in the program so some errors may default to the Psion's own system.
There is a very limited help section at present.
Only Epson and Cannon are supported printers for all printing options at present. If you wish to include more then email me gasdoc@hotmail.com and I will try to accommodate.
The program is three opa files at present but will be condensed to one in the future.

Thank you for taking this interest in my program which represents many many hours of work. Please register the program if you intend to keep it by sending an email to gasdoc@hotmail.com 
