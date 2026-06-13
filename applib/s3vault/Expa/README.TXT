                             
                       EXPENSES  V. 1.0

                             By
                     Julius K. Bjornsson
                         72500,456
                      julkb@rhi.hi.is


Introduction:
--------------
Here is my first OPL application for the S3, called EXPENSES.  This is a 
simple application which keeps track of some types of expenses,
i.e. money, distances (milage) and hours.  I wrote it to keep track of 
these things for myself, in such a way that I could keep all related records
together in one file.  I therefore keep one file for each activity, or each 
job, and can then print out a summary for each type of activity for certain
periods of time.  


Installation:
-------------
The archive contains three files:

		EXPE.OPA	Program   
		SAMPLE.EXP	Sample datafile
		READ.ME  	(this file)

In order to install the program, you must create a \exp directory on any 
drive, and copy the EXPE.OPA file to any \APP directory.  Thereafter you can
install the program onto the system screen in the usual way, with APPS->
INSTALL APPLICATION.

Use:
----
All data files should appear under the program icon, and can be started
in the usual PSION manner, by simply choosing the file to use.  Only one 
datafile can be open at a time, as the application is a TYPE 2 OPA.

The program starts with an intro (about) screen, which is removed by pressing
ENTER, and then the main menu appears.  This menu has the following structure:


FILE
 NEW FILE     Create new datafile
 OPEN FILE    Open an existing file.  A status line shows which file 
              is currently open.
 SAVE AS      Save a datafile under a new name, or copy it from e.g. 
              a flash card to RAM.  (this is really a compress).
 DELETE FILE  Delete a datafile. (only closed files).

EDIT
NEW RECORD    Enter a new record.  This opens a dialog where you can enter
              some explanatory text, date and choose the type of data you
              want to register.  This is done by using the arrows or by
              using TAB to open a small menu.  Ths choices are money,
              distances and hours.
              Finally the amount can be entered, as a floating point number,
              and here it is possible to enter negative as well as positive
              numbers.
   UPDATE     With this option you can choose a record to edit.  When this
              option is chosen, a browse window opens, where you can browse
              through all the records in the file.  The browse window opens
              with the first record current, but you can move in the window
              with the up and down arrows, PgUp and PgDn, in addition to being
              able to jump to the beginning and the end of the file, with HOME
              and END.  It is also possible to jump about in the file pro-
              portionally, by using the 1,2,3,4,5,6,7,8,9 keys, where e.g. the
              1 key positions you below approximately 10% of all records in 
              the file, the 9 key positions you approximately below 90% of all
              records and the 5 key positions you in the middle of the file.
              It is thus possible to quickly jump all over the file, even if
              if has become big.  When you have found the record you want to 
              edit, simply highlight it, and press enter, and the editing
              dialog will open.  The status line shows the current record 
              number.
   REMOVE     This option removes the chosen record, via the browser.

BROWSE	      This option in the top menu does not do anything than let
              you browse the file.

REPORTS
   SUMMARY    This is the main report of the system, allowing you to
              generate a simple list of records for a certain period
              of time and of a certain type.  In addition you can choose
              whether the reports goes to the screen or directly to a
              printer, which must then of course be connected and ready.
              Otherwise you will get an error message.  If you choose 
              to view the report on-screen then the text fields will
              be trimmed to 15 characters, but generating the report
              directly to the printer, will show all 30 characters of
              text allowed in the input.  The report additionally generates
              a summary of the type of data chosen.

SPECIAL
   ABOUT      This option show some info about the program/author.
   SETTINGS   Here you can choose which units of distance and currency
              you want to use.  This will be saved in a small file on 
              your \EXP directory, called exp.cnf, which the program
              checks for each time it starts.

EXIT          Finally, this is where you go, when you have had enough of
              the program.

    

     Finally.  All shortcut keys on the menus should work directly, and I 
     hope that the program is reasonably well-behaved, as I have tried to
     follow the rules for a type 2 OPA.

     Any suggestions for improvements, and better ways of doing things,would
     be appreciated, along with any bug reports.
     
    Planned enhancements.
    
    1.  Speed up the browse window.
    2.  An export module, in order to export files or parts of them to
        e.g. the spreadsheet or the database. (work in progress)


                Written in ICELAND, in the middle of a winter storm

                Julius K. Bjornsson
                Nordurtun 9
                225 Bessastadahreppur
                ICELAND

                CIS:72500,456



