		    BIKLOG5    V 3.2
*****************************************************************
* For the Psion Series 5/5mx and REVO(+) and Series 7/netBook  *
*****************************************************************

(Copyright, Jaap Laméris. May 15, 2003)

CONTENTS
- INTRODUCTION
- BACKGROUND
- INSTALLATION
- UPGRADE FROM OLDER VERSIONS
- USAGE
- FUTURE IMPROVEMENTS
- ACKNOWLEDGEMENTS
- REGISTRATION
- DISCLAIMER
- HISTORY



INTRODUCTION
============

BIKLOG5 is a program made for bicyclists who like to keep track 
of their bicycle rides during time. It is not meant to be used 
during the ride as a cycle computer but as an
extension of it to store the cycle computer data and as
a tool to analyse the bicycle rides over time.

With some renaming of the definitions of the type of bicycles
used and riding type, it may be used even for thriathletes by
renaming the bicycle names in "swimming", "biking", and "running"
and adapting the bicycle ride names into appropriate terms like
"training", "race", "intervals" etc.

Version 3.1 is a major upgrade, incorporating 7 additional fields, 
some of them to be defined by the user. BIKLOG5, however, is capable 
to process database files generated with older versions. However, 
in order to be able to use the new features, it is required to update 
these database file(s), using the upgrade option within the menu.

Version 3.2 is a minor upgrade, enabling Series 7/netBook users full graphical use of BIKLOG5, in addition to a coloured icon on the system screen or extrabar.

BACKGROUND
===========

This program is based on the original program Biklog3,
which was my first major attempt at programming in OPL.
It was then completely developed on my Series 3a with the
aid of the S3a emulator. When I later purchased a S5,
I ported it to the Series 5 and extended it with the
additional capabilities of the S5 (namely, pen operation,
printing capabilities and a better database management).
In version V2.1 I have included some more options,
more statistical output and a way to keep track of the
rides ridden versus a goal set for # of rides and bicycle
distance set for the year. In version, 2.4, I have
updated it to the REVO (+), taking into account the smaller
screen.

I have tried to make the program to be user friendly as far
as possible, and have released it after a long period of
using it myself (during more than 5 years).

This program is freeware, not because I don't need the
additional income or fancy the merits of this program, -
I spent a large amount of time and effort in creating and
debugging this program -, but charging for something I wrote
as a hobby and for fun is not my style. Further by making it
freeware (or better said postcardware, see the section
REGISTERING) I will not be forced to spent considerable time
in maintaining and supporting it just for the sake of getting
a small additional income. That doesn't mean that I will not
support it anymore. As an user of BIKLOG5 myself and as a
perfectionist by nature I will keep improving the program and
supporting any users of it. Another reason to release it as
freeware are some bad experiences in the past with shareware
(PSION and PC) programs for which I paid the registration,
after which the author did not support it any more, released
it as freeware or kept charging for updates despite his
earlier promises... .


INSTALLATION
============
Below is a list if files supplied in the distribution ZIP
file (The file biklg532.ZIP) and the directories
they should be placed in after the installation.

biklog5.sis                         SIS installation file
editor.zip                          Zipped SIS installation 
                                    file for Symbian editor
biklog5_readme.txt                  this file
biklog5_leesmij.txt                 this file in Dutch
biklog5_liesmich.txt                this file in German
biklog5_lisezmoi.txt                this file in French

If using Psiwin 2.x within windows then just click on the biklog5.sis file. 
The program will appear in the "normal" Extras Bar after the installation. 
Alternatively, you may copy the BIKLOG5.SIS to your EPOC machine and double tab it. If installing from somewhere other than Windows then you will need to obtain INS_TE.EXE and install that on your EPOC32 machine. This will enable.SIS files to be decoded using the EPOC32 machine.

If you don´t have installed the Symbian Text editor on your machine and you want to use the new diary file function within Biklog5, you have also install the Editor installation file editor.sis, packed in the editor.zip file!


After installation you should have on your machine the following files:
* = C or D (or E for the series 7/ netBook users)
*:\system\apps\biklog5\biklog5.APP       Main application file.
*:\system\apps\biklog5\biklog5.AIF
*:\system\apps\biklog5\biklog5.MBM       graphics file
*:\system\apps\biklog5\biklog5.HLP       BIKLOG help file.
*:\system\apps\biklog5\biklog5.rsc       BIKLOG resource file 
c:\system\apps\biklog5\biklog.ini        default configuration file
c:\system\apps\biklog5\biklog5.ini       ini file with last used filename
*:\system\apps\biklog5\efm.mbm           graphics support file

c:\biklog\example                        example database
c:\system\apps\biklog5\example.ini       example configuration file
c:\system\apps\biklog5\example_d.txt     example diary file

*:\system\opm\ini.opm                    ini.opm (support file)
*:\system\opm\iqgrid.opm                 iqgrid.opm (support file)
*:\system\opx\gprinter.opx               support OPX file for printing
*:\system\opx\nmpd.rsc                   support RSC file for NMPD module
*:\system\opl\nmpd.oxh                   support OHX file for NMPD module
*:\system\opl\nmpdconstants.oxh          support OHX file for NMPD module
*:\system\opm\readwrite.opm              support OPM file for READWRITE module


Except for the *.ini files, all other files may be located on the
C or D drive. The database files may be located in any (sub)directory,
(from V2.2 on). The directory "*:\biklog\ " is recommended to be the
location for the database file.




USAGE
=====

Most of the information you may need on how to use
BIKLOG5 is included on the on-line help.

Here is a short description of all hot-key presses

Ctrl-O    <Open existing file...> Open or switch to an existing file
           - note that BIKLOG5 will automatically open the last used database file,
           unless one has double tabbed on the icon of a different database.

Ctrl-N    <Create new file...> create a new file
	    - After entering the name and location of the new file,
	   you will be asked to enter the names of up to four
	   bicycles in your household, and the type of bicycle rides
	   you want to tag to your rides (like commute, training,
	   race, clubride). Maximal five ride definitions are possible.
           The user can also input the names and units of up to 4 additional
           user-defined datafields and one logical datafield type (Yes/No or 
           On/Off), plus up to 6 different conditions in the condition field
           (eg. to characterize the weather or ride condition). These values 
           can also later defined in the preference option.

Ctrl-C    <Close file> close the current file
	   - Only needed if you want to delete the current file.

Shft-Ctrl-D    <Delete file...> delete a file

Shft-Ctrl-S    <Sort file > sort current file
	   - This will sort the records according to ascending date. If the 
           start time field has been tickled on in the preferences, 
           the start time will be used as a secondary sorting field.
	   The plotting option assumes that the records are sorted by date!
	   If you get strange effects with the graphs, just do a sort.

Shft-Ctrl-Y    <export file...> export database to text file. 
           Two different formats can be choosen:
           (1) using EPOC format for date and start time fields, or
           (2) Date and start time as literal values (15/10/2002, 7:45 or 7.75) The
               same is true for the fields such as bikename, ride event and condition.
           The second option is recommended if you want to use the data in third-party
           programs like spreadsheets or database for further analysis or reporting. 
           The first option is more suited if you intend to import the data after 
           modification into a (new) Biklog database. EPOC date format is the number 
           of days after 1-1-1900,while EPOC time is expressed as the number of 
           seconds after midnight.
           Fields are seperated by a user choosen field seperator code (, ; or tab). 

Shft-Ctrl-I    <import text file> import text file into BIKLOG5 database file. 
           The user has the option to make a new database file or to merge the data 
           with an existing database file. Input file must be a text file with fields 
           seperated by a user defined field seperator in any of the two format 
           described for the export option. See also the specific rules mentioned in 
           the IMPORT description in the Help file.

Ctrl-E    <Close> close (exit) BIKLOG5

Ctrl-A    <Add...> add a record
	   - Here you enter the date, bicycle name, bicycle ride type, distance,
	   time or average speed and a short descriptions (max. 40 characters)
	   of the ride. 
           - If choosen in the Preferences, the user can additionally define :
           (1) the start time, 
           (2) the contents of 4 additional real fields (to be defined in preferences),
           (3) a logical field (yes/no) (to be defined in the preferences),
           (4) one of 6 choices of the condition field (to be defined in preferences),
           (5) up to 255 characters text to be included in a diary file.
           (By default the current date and the values of the last
	   entered record will be entered. You may choose to enter average speed
	   or time. Either one is sufficient.

Shft-Ctrl-A    <Edit...> edit or modify a record 
           The user will have afterwards the option to edit a record.

Shft-Ctrl-E    <Erase...> erase a record

Ctrl-F    <Find...> find a string in the records
	   -Use this option to find a string in the memo field of one or more
	   records

Ctrl-t    <Table...> 
	   - Here you will see a listing of all rides present in the database,
	   according to the filter settings set after entering this mode
	   (time period and bicycle type/bicycle event/condition/logical field).  
           All the filtered records will be listed line by line. Use the cursor 
           up, down, and the PgUp or PgDn keys to move or scroll up or down, 
           respectively.
	   HOME or END will move to the first or last record. ESC will exit the
	   scan mode, while "s" will present some statistics of the data record
	   in the database. "n" will show all data of the ride in a seperate window.
           Any other key will invoke a listing of possible actions.


Ctrl-G    <Graphs...> Plot up to 10 different plots per bicycle or ride event
	   -You will be prompted to enter starting and ending date of the time
	   period, and the time unit along the x-axis (day, week or months). 
           Additional filters are possible depending on the condition and
           logical field defined. By default all records are included.
	   By default the starting date will be January 1th of the year of the
	   first record, and the ending date will be the current date.
           For databases, starting in previous years, the starting date will be 
           either the starting date of the file or the date of the last record 
           minus 400 days. (for internal memory reasons).
 
	   Cumulative distance plots will be presented for each of the available
	   bicycles (with more than 0 km) in the database plus all bicycles
	   together. Use the up and down cursor keys to cycle through the plots,
	   ESC will return to the menu. Each other key will invoke a listing
	   of possible actions.

Shft-Ctrl-G  <print Graphs...> print the graphical output using the standard Psion 
           printing options. This option will only appear and work, when Graphs have
	   been made.

Ctrl-s    <Statistics...> Statistical description of rides
	   -Eight tables will show you some common statistics (# of rides,
	   cumulative and mean distances and duration for each bicycle and
	   ride type in the database, absolute and relative to the total
	   bike rides). Additionally the total distance ridden and average 
           speed per month and bicycle type is shown. The next table shows 
           a cross-plot of total distances as function of bicycle and ride 
           activity, while the last table will present some statistics of the 
           rides (longest, shortest, fastest and slowest). Additionally some 
           data on the progress against the goals and the forecast at the end 
           of the year are presented.
            Possible filters to be applied are : time period, type of bicycle, 
           ride event, condition type and logical condition. For the last two 
           parameters, it is also possible just mark all conditions and all 
           logical conditions (yes + No). These last options are the default 
           ones.

Shft-Ctrl-T  <Print stats...> print the statistical output using the standard 
           Psion printing options. This option will only appear and work, when 
           the option statictics have been performed.

Ctrl-d    <edit diary> edit diary file 
           run the Symbian editor to edit the contents of the diary file

Shft-Ctrl-F  <Forecast> Forecast
           calculates the expected number of rides and total distance on any 
           date defined by the user.*

Ctrl-V    <Preferences...> Preferences settings
	   -use this option to customise the bicycle names, ride definitions,
	   unit of distance (kilometer or mile), and the goals in terms of
	   number of kilometers or miles and the number of rides set in a year.
	   A message wil show up when each goal has been reached.
           *Additionally, depending on the version of the current database, the 
           names and units of the custom datafields, the name of the logical 
           condition field, and the six definitions of the ´condition´filed 
           can be defined under the custom tab blades. Under the Misc tab page, 
           the user can define the visibility of the start time, the custom 
           fields and of the logical condition field. 

Ctrl-U    <Upgrade older databases> UPGRADE from older versions
	   This option allows the user to convert an existing database into the
           new extended database format of version, where 7 new datafields are 
           added for each record.  A backup file with the extension ".bak" 
           will be created in the original directory.

Ctrl-B    <Show toolbar> toggles the toolbar

Ctrl-I    <About Biklog5...> Info on program

Shft-Ctrl-H   <Help on Biklog5...> Help function

Not in menu:
Ctrl-h    Change the definitions of the header and footer in the Table output.

In addition the printing options let you invoke the regular Psion 5
printing mode of the tabular, graphics and statistical data. Please note that 
you must have invoked first the TABLE, graphical or statistical mode before 
to be able to print the tabular, graphical or statistical data.


I do recommend that you use the online help, at least until
you become familiar with BIKLOG5.


FUTURE IMPROVEMENTS
===================


-- other languages
-- Adaption for thriathletes (is already possible in rudimentory form by
   renaming the bicycle names in "swimming", "biking", and "running" and
   adapting the bicycle ride names into appropriate terms like "training",
   "race", "intervals" etc.
-- Anything you as user will suggest that is worthwhile and shared at least
   by one other user!

ACKNOWLEDGEMENTS
================

Thanks to Jezar for providing the Help generator, Neil K Bee (neil@bee.net)
for the use of one of his icons, Damien Lewis and PocketIQ for the use of the
IQGRID.opm, RMRsoft for their RMRRSG program and all other PSION programmers, 
especially Steve Litchfield and Al Richey with their RMREVENT program, who gave 
me examples of good working programs. A special thanks to Martin Zeddies and 
Philippe Bricka, without their assistance the German and French versions 
would not have existed.
Also, last but not least the users of this program, who gave me valuable 
tips and experiences.


REGISTRATION
============

WHY REGISTER ?

Although BIKLOG5 is freeware, I would be very happy if you let me know that
you are going to use my programme. First of all to satisfy my curiosity, after
putting numerous hours and effort in writing and debugging this program.
Secondly it will encourage me to continue further with this program and
extend it if you indicate a need for this. Thirdly I will be able to keep
you informed on next releases of BIKLOG5, if you registrate it by me.


HOW DO I REGISTER ?
Preferably you can send your name, adress and/or Email adres
by Email to:

Jaap Lameris
j.lameris@hccnet.nl

Else you can send a postcard to:

Jaap Laméris
Richel 10
8303 KX EMMELOORD
Netherlands.

Please include in your registration note,  the version number you got,
your postal adress and Email adress.

Additional information on BIKLOG5 will be also available on my homepage:

http://home.hccnetnl/j.lameris/mypsion.htm


DISCLAIMER
==========
Use this software at your own risk. I will not be held
responsible for any loss of data and/or any damage caused
to users or hardware caused by this software.



COPYRIGHT AND CONDITIONS
========================
I reserve copyright on this material, any reproduction in
part or whole is strictly prohibited. Additionally,
reverse translation of any of files included in the file
"BIKLG532.ZIP" is strictly prohibited.

You are permitted to distribute the file "BIKLG532.ZIP",
as widely as you see fit, under one condition. That
condition being; That the file is not in any way altered,
and is sent on in an unchanged state.



FEEDBACK
========
If you have any comments or suggestions for me I would be
pleased to hear from you ! You can mail me at the above
address or you can email me at these email addresses;

		 j.lameris@hccnet.nl
			  or
		   lameris-molhoek@hetnet.nl

Now get on and start bicycling !!!


HISTORY
=======
Version 3.2 (May 15, 2003)
-- minor upgrade to enable full colour use for Series 7/netBook owners.

Version 3.1 (March 15, 2003)
-- French and Geman versions updated to version 3.0
-- small bug fix for Series7/netBook owners fixed
-- small bugs in input and output fixed.

Version 3.0 (January 2, 2003) (Only English and Dutch versions 
             at this moment)
-- extended database with 7 additional fields:
      4 real fields to be defined by user
      1 logical field to be defined by user (Yes/On or On/Off)
      1 ´condition´ field with 6 choices to be defined by user 
      (e.g. to characterize weather or terrain conditions)
      start time
-- user can block presence of additional fields in output (to be defined
   in preferences)
-- diary file system for longer ride annotations (max. 255 characters)
-- compatible with pre version 3.0 database files
-- forecasting function now implemented in menu
-- import function
-- output for Series 7/netBook screen format adapted
-- more statistical options
-- sorting now also on date + start time
-- filtering for TABLE, Graphical and Statistics extended
-- some small bugs fixed.
 

Version 2.8 (April 21, 2002):
-- Dutch, German and French versions
-- more statistics added
-- export option added
-- better menu structure
-- forecasting function added in statistics
-- minor bugs fixes

Version 2.7 (December 12, 2001):
-- multiple plot options added :
   cumulative distance,
   total distance or time,
   mean distance or time, and
   average speed;
-- plots for bicycle events or bicycle type,
-- for non-cumulative plots choice of barcharts or spike plots,
-- minor bugs in closing/opening files fixed.

Version 2.6 (August 14, 2001):
-- entry of ride durance now also in seconds (hh:mm:ss instead of hh:mm).
   This also is presented in the SCAN mode
-- minor bugs fixed (e.g. "file xxxx is not a valid file")
-- Last defined goals of # of km/mi and # of rides set in preferences
-- Statistics can now also be printed
-- minor bugs in printing fixed

Version 2.5 (April 16, 2001):
-- better printing possibilities, automatically set paper format
-- printing of graphical output
-- number of decimal points in output selectable
-- minor bugs fixed
-- more foolproof

Version 2.41 (November 16, 2000):
-- minor format change in format
-- (latest) sysram1.opx added in installation file

Version 2.4 (November 5, 2000):
--much improved SCAN mode with zooming capabilities
--REVO compatible
--better printing format
--last file remembered
--Statistics of current file in default screen

Version 2.3 (Jan. 16, 2000):
The duration of single rides in the SCAN and STATISTICS modes is now being
expressed as hours:minutes format. The datafile generated by BIKLOG5 is now a
BIKLOG5 document, e.g. the datafile can be opened automatically in BIKLOG5 by
double tabbing on the icon of the database file. An upgrade module is provided
in BIKLOG5 to convert databases generated with earlier versions to BIKLOG5
document. A backup file with extension ".bak" will be generated.

Version 2.2.1 (Dec. 21, 1999):
Minor bugs fixed. It is now possible to use any directory name to place your
datafile. Also it is now possible to enter riding time in hours + minutes.

Version 2.1 (Nov. '99):
Major upgrade from original version.

Version 2.0 (Aug. '99), original version, not published in public domain:
Based on the original Series 3 version (Biklog3 V 1.1).



