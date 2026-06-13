TimeLog 2.10
============
TimeLog is a tool for the Psion Series 3a which is intended to help you 
keeping track of your time spent for any specific reason. You can enter
records consisting of a begin time, duration or an end time and assign them
to a category. In a category view the time records are summarized and you
are enabled to define a specific cost per hour. You can sort the entries by a
number of criterias and export them to an ascii file. The list of records
can then be printed as a list at any given date range.


INSTALLATION
============
To install TimeLog, copy the file Timelog.app to any \App directory.
Then install the application as usual using the "application / install"
menu command.


USAGE
=====
TimeLog is based on two views of the data: the TIME view shows a sorted list
of the entered timed records and the CATEGORY view, which shows a sorted
list of the categories to summarize the records. Usually you will enter all
the different types what you are spending your time for, and then you will
enter the related timed records as you are working for something or someone.

Please note the context sensitive help mechanism!

Keyboard usage
--------------
Move the record selection by using the cursor up/down/page up/page down
and Home/End keys.
Shift the display to the left/right by using the cursor left/right keys.
Remove an entry by pressing the Delete key.

File/New file
-------------
Create a new file.

File/Open file
--------------
Open an existing (*.tlg) file.

File/Save as...
---------------
Save the current database under a new name.
Select type "Text" for producing an export to an ascii file. The character
string "Field quote" encloses the single fields, the character string
"Field delimiter" will be put inbetween the fields. So if you select a
comma as the delimiter string and a quotation mark as the quote string, a
file fragment could look like:
"19.3.95","12:30","13:50",,,"Client XY","Special support"
There are two special character sequences: <TAB> will be replaced by a 
tabulation character, and <CR> will be replaced by a carriage return line
feed sequence.
The default directory of an exported database will be the \Tlg directory.

File/Merge in
-------------
(Only in the category view) Merge the categories of a file into the current
database.

File/Compress
-------------
Compress the current database after record changes or deletions to save 
space on the disk.

File/Codeword
-------------
Protect the current database for any unauthorized access by using a codeword.

Entry/New Entry
---------------
Enter the data of a record.
Time view: The field "Time details" can be set to "No" to insert day 
specific remark records.
Category view: If you change the item "Unassigned" to "No", this 
category will summarize all those entries which are not assigned to any 
category. There can only be one record of this type.

Entry/Edit entry
----------------
Change the data of a selected record.

Entry/Delete entry
------------------
Delete the selected entry from the current database.
A category record can only be removed, if there is no timed record which
is referring to it.

Entry/Split entry
-----------------
Split the selected timed record into two consequtive parts with possibly
different categories or descriptions.

Timestamp
---------
Make a time update to the selected record.
In the category view, a timestamp will insert a timed record having the
actual date, the actual begin time and the selected category.
In the time view, a timestamp will update the selected timed record to
the current end time.

View/Sort
---------
Sort the actual view by specific keys.

View/Freeze
-----------
Keep the selected column of the actual view always visible.

View/Report
-----------
Calculate and show a short summary of a given date range.

Special/Setup
-------------
Make application preference selections.
- Enter registration info;
- Make display setting;
- Make exit method settings;
- Select the prefered record entry / update shortcuts;
- Change the category name identifier.

ABOUT THE PROGRAMM
==================
This application is shareware, so if you are using it, please register it!
No additional fee should be charged for it.
The package is to be kept unmodified and as it is.


REGISTRATION
============
TimeLog 2.10 is not free! If you are using it, please register with the
author.

There are two different levels of registration possible:
Registration for the Standard version for USD 20,00 removes the nagging 
screens, enables the sorting dialog and the print and print preview 
functions.
Registration for the Advanced version for USD 30,00 enables the codeword 
access protection and the export capability.
For USD 10,00 you can change the registration level from Standard to
Advanced or trade in an earlier version of TimeLog for a Advanced 
version.
Registered users of any earlier version will receive a free registration 
code for a Standard version of TimeLog 2.10!
Registration can be made by cash, international money order or by using
the CompuServe SWREG service (Standard ID = 6215, Advanced ID = 6214, 
Upgrade ID = 6216).


HISTORY
=======
Up to version 2.0 TimeLog was implemented using OPL. Since applications
which are written in OPL are restricted in many aspects, TimeLog 2.10 was
the first one which was completely recoded in object oriented C.

Version 2.10:
* First release coded in C
* TimeLog now uses only a single *.tlg file, which makes copying, deletion
  and file manipulation much easier.
* Installation is simplified by using only one file.
* Printing now uses the standard mechanisms (setup, preview, ...)
* All the setup info (excluding registration info) now is held in the 
  database itself.
* Now it is possible to sort on a number of criterias.
* Records which are not (yet) assigned to any category can be summarized
  within a special category.


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

June 1995, Vienna, Austria

