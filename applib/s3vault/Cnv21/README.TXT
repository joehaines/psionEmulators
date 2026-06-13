Convert 2.00
============
Convert is a tool for the Psion Series 3a to make all kind of conversions.
You can use its predefined conversion tables, you can extend them easily,
and you can even write your own conversion tables.


INSTALLATION
============
To install Convert, copy the file Convert.app to any \App directory.
If you want to use the predefined conversion tables, copy the *.cnv files
into a \Cnv directory on any drive.
Then install the application as usual using the "application / install"
menu command.


USAGE
=====
Convert uses a special table view to handle its conversion tasks. Each
line takes the following data items:

Unit name - is used as an identifier or label for an unit.

Unit abbreviation - is a unique abbreviation for the related unit.

Formula - is the conversion function, written in the form:
               <unit1>=<unit2><operator><operand>

There are two different types of data items:

Base unit - the first table element to be entered is called the base unit,
which means that it can not refer to any existing unit.

Unit - each one of the other items can refer to any other units by means of
formulas.


Using existing conversion tables
--------------------------------
To use an existing conversion table select any entry by using the cursor keys,
simply enter a number followed by an ENTER. After calculating the conversions
the result of the calculation will update the other table entries. That's it!

Defining a new conversion table
-------------------------------
Start a new file from the system menu or by selecting the "New File" menu
command. Press SHIFT ENTER to launch the Base unit dialog. Provide a name
for the new base unit and a (meaningfull) abbreviation. For example:
Unit name: Metre
Unit abbreviation: m
After updating, you can enter any number of additional entries, providing
a name and a related formula, for example:
Unit name: Centimetre
Formula: cm=m*100
IMPORTANT - the formula reflects the numbers relation, not the units one.
So if you have a value of 3 metres, you will come to the value of the
centimetres by multiplying 3 by 100 = 300 cm!


Please note the context sensitive help mechanism!


Keyboard usage
--------------
Move the record selection by using the cursor up/down/page up/page
down and Home/End keys.

Shift the display to the left/right by using the cursor left/right keys.

Remove an entry by pressing the Delete key.


File/New file
-------------
Create a new file.


File/Open file
--------------
Open an existing (*.cnv) file.


File/Compress
-------------
Compress the current database after record changes or deletions to
save space on the disk.


Entry/New entry
---------------
Enter the data of a record.


Entry/Edit entry
----------------
Change the data of a selected record.


Entry/Delete entry
------------------
Delete the selected entry from the current database.


Special/Setup
-------------
Make application preference selections.

- Enter registration info;

- Make exit method settings;

- Change the number of displayed digits.



ABOUT THE PROGRAMM
==================
This application is shareware, so if you are using it, please register it!

No additional fee should be charged for it.

The package is to be kept unmodified and as it is.


REGISTRATION
============
Convert 2.00 is not free! If you are using it, please register with the author.

After registration the nag screens will be removed.

Registered users of any earlier version will receive a free registration code!

Registration is 15 USD and can be made by cash, international money order or
by using the CompuServe SWREG service.


HISTORY
=======
Up to version 1.4 Convert was implemented using OPL. Since applications
which are written in OPL are restricted in many aspects, Convert 2.00 was
the first one which was completely recoded in object oriented C.


Version 2.00:
* First release coded in C
* Installation is simplified by using only one file.
* All the setup info (excluding registration info) now is held in the
  database itself.


DISCLAIMER
==========
Although the program has been tested extensively, there is no guarantee that
it is error free.

Make backup copies from your important data!


The author will not be liable for any damage or consequential loss caused
by any error, defect or failure of the application.


ABOUT THE AUTHOR
================
If you have any questions or suggestions regarding TimeLog, please do not
hesitate to contact the author:


Thomas Nemec
Gladbeckstrasse 1/12a/10
2320 Schwechat

Austria

CompuServe 100024,3461
Internet 100024.3461@compuserve.com


August 1995, Vienna, Austria
