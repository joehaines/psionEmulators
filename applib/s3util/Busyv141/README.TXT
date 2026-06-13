BusyView 1.41.

NOTE:
-----
Please note that the author is on holiday in the following 3 weeks:
June 28 - July 5 1997
July 7 - July 12 1997
Aug 2 - Aug 9 1997

If you register during this weeks, please be patient. You'll get
your registration number as soon as possible.
Thanks.

What is BusyView:
-----------------
BusyView is an extension for the built in Agenda application. It adds an extra
view to your appointments. It shows 4 (2 on the Siena) weeks at a time, and shows when you are
busy (hence the name). It optionally shows untimed entries as well. An
excellent overview is the result. And above all, it does it at blinding speed.
Starting the program, reading an agenda file of 24 Kb, interpreting the file
and displaying the results, all in about 2 seconds! (Written in C)
It runs on Psion S3a, S3c and Siena.

BusyView is shareware:
----------------------
This means that you can use BusyView for 3 weeks. If you like it, register it.
If you don't like it, remove it from your system.
To evaluate properly a new system has been built into BusyView.
This enables you to test all functionality of BusyView for 3 weeks.
No nagging, no disabled functionality. It all works! For three weeks.
Then most navigating options will be disabled and you are reminded to register
BusyView. It will stop working after 6 weeks.
This means you will have a full working versions for three weeks. You don't
have to pay to see all options work. No surprises. You know exactly what you are
going to pay for.
It is however NOT timelocked to a specific date. It starts counting after you
install BusyView.
(WARNING: do not start tampering with this system by changing the system date
as it might start nagging immediately. You can only recover from that situation
by registering BusyView! If you do have to reset your system, set the
system clock before starting BusyView)

To register you have a three options:
$ = US dollars.
1. Compuserve : GO SWREG and search on 'BusyView' or use ID=13715.
					 ($25)
2. Internet   : http://www.swregnet.com/482p.htm.
					 ($25 + $5 handling)
3. Send cash  : Send $25 or 40 Dutch guilders (or Euro Cheque in
					 Dutch currency (NLG)) to:
					 Lieuwe de Vries
					 Taling 11
					 9101 ZG Dokkum
					 Netherlands
					 State (at least) your name, address and/or email address.

After registering you will receive a registration code which can easily be
entered without reinstalling the application.
Registration gives you the right to support (by email only) and updates up to
version 1.99.

Installing:
-----------
Make sure your system time and date are properly set!
Unzip the file you received. Copy BusyView.app in any \APP\ directory.

Make a new subdirectory \APP\BUSYVIEW\ in the directory where you copied
BusyView. Put the resource file which matches your Psion language in this
directory. The resource files are named busyv??.rzc where ?? is the number
of the language.
Currently supported (or being created at the moment) are:
  03 = German
  18 = Dutch
If you use English as your choice, no separate resource files are needed.
The English resource file is compiled into the program.
That's all. When starting for the first time BusyView will make a small
ini file on the internal drive (LOC::M:\OPD\BusyView.ini). No more files
are created by BusyView.
After copying the file, install the application in the systemscreen.
A list of agenda files appears under the BusyView icon.

To let BusyView 'learn' a few things from Agenda, start Agenda first and
then start BusyView, both using the same agenda file.
You only have to do this the first time you use it. After that Agenda
can be started from BusyView. This is of special importance if you use
a new file list (alias) for Agenda.

WARNING:
After installing and starting BusyView, you should go to the preferences
and check the agenda keys:

Agenda view key :
Used when using psion-g in BusyView to set the view in the
Agenda application.
The letter you have to enter is the capital letter you find in the Agenda
application in the diamond menu behind the view of your choice.
If you want the week view, and the diamond menu in Agenda shows :
Week view : shift-psion-W, you should enter the (capital) W as the agenda
view key.

Agenda jump key:
Used to switch to the selected date in Agenda. You find the option
in the search menu of Agenda. In the UK and Dutch version this is
psion-j, so enter a 'j'. The German version uses psion-g. You should
then enter 'g' as the Agenda jump key.


BusyView and agenda files:
--------------------------
BusyView shows all types of agenda entries. BusyView only reads agenda files.
It will not modify your files.
If Agenda changes the file (adding a new entry) BusyView will reread the
file automatically when you switch to BusyView so your view is always up to
date.
The following events in the psion will cause BusyView to update its display:

1. System date is changed (manually or at midnight).
2. System settings for time, date or start of week have changed.
3. Agenda file changed.

Using BusyView:
---------------

Starting:
Select an agenda file from the list and press ENTER.
The first time BusyView is started it sets the viewdate to today. The view
always starts at the 'start of week' day you assigned in the time application.
If Agenda is using the same file, it is closed first and restarted after
reading the file.
Default values for time and date representation are set in the Time
application.

The view:
The view consists of 4 week blocks. Inside the blocks you can see your timed
appointments as black blocks. Overlapping time blocks are in 'gray'.
Alongside the blocks are time indicators.

In the top right of the screen you can see the text of the timed appointment(s).
Optionally untimed items from the agenda are also shown there. They are always
last in the list. Step through the list using the '+' and '-' keys. If there
is more than one item an indicator shows how much there are and which is the
current one.
Timed appointments are only shown in the list when the cursor is on the black
block which indicates the appointment. Untimed items are always shown on the
day they occur (optional). Daynumbers are inversed if they have untimed item(s)
and if the inversion is set in the preferences.

The top left shows the month and year and the current day.

Navigating through the view:
Use the TAB key to goto the next timed appointment. Use shift TAB to goto the
previous. Control TAB goes to the next overlapping time block.

Use cursor keys to goto next/previous day or hour. This will shift the view
1 week if necessary or the viewtime.

End/Home key shifts the view 1 week.

Control left/right shifts the view 4 weeks.

Pressing the space key will bring you to the current system time and date in the
view. Pressing Shift space will bring you to the first appointment AFTER the
current system time and date (if available).

ENTER will give you a list of all timed and untimed(optional) items of the
current day. (It shows the first 12 lines)
Alarm times are shown in square brackets. The date of the alarm is also
displayed if it differs from the item date.


Pressing the first letter of a dayname will bring you to the next day starting
with that letter. It will shift the view 1 week if necessary. Shift dayletter
will do the reverse.

Pressing the keys 1-9, 0(10), *(11) and /(12) will bring you to the next
occurence of that month. Use SHIFT to jump to previous occurence. 

Pressing ESC will show you the last used 4 week view.

PGUP/PGDN shifts between the early/default and late view. The early view
shows all appointments from midnight to three in the afternoon. The late view
from 9 in the morning to midnight. The default view can be set in the
preferences.


Use psion G to goto Agenda, using the current file. If Agenda it not started
it will do that for you. If Agenda is started but is using another file it will
instruct Agenda to load the file you use in BusyView. Then it jumps to the
date you selected in BusyView using the view you chose in the preferences.
Finally it brings Agenda to the foreground.


General:
The easiest way to use BusyView is to assign it an application button like
Control Agenda. This allows you to switch easily between BusyView and Agenda.
You can also set a hot key in the preferences.

Preferences:
------------
Show untimed entries: If set to 'yes' it will display untimed entries and
anniversaries. Todo items have their own preference setting.

Show crossed out items: Set to 'yes' it will display all crossed out items
except todo items if they use the agenda preferences.

Show todo items up to prio: 'Use Agenda preferences' takes the Agenda
preferences for each todo list as described in 'Show in other views'.
Choose 'Never show' to never view them in BusyView.
Choose a priority to show todo items up to the specified priority. This
is valid for all todo lists.

Hot key: Enter a lower case letter here to assign a hot key to BusyView.
This letter is used in conjunction with Shift-Psion to bring BusyView to
the foreground wherever you are. If you enter a 'p' here, pressing
Shift-Psion-p will bring BusyView to the foreground. This system will
have unexpected results if the hot key is already used as a hot key or as
an application key. You should look for a unique hot key!

Press ENTER to show the next preference screen. This 3 new options allows you
to set when the daynumbers are inverted. You can choose 'Todo items', 
'Untimed items'and 'Anniversaries' or any combination.
The fourth options allows you to display the letter, used in Agenda for 
display in the year view, in the block of the timed item. Currently letters
for other types op items cannot be displayed.


Limits of BusyView:
-------------------
1. Maximum (theoretical) size of the agenda file is 512 Kb.
	If you have a big agenda file, you should split it.
	Use a separate file for every (half) year.
	It saves you memory when running Agenda and BusyView.
	It's also a lot faster.
2. Maximum number of items is 512. Every timed or untimed agenda entry
	counts for one. A daily repeat for 4 weeks, counts for 28 items.
3. Maximum number of items in top right of screen is 50.

Memory usage:
-------------
Installing BusyView takes about 32 Kb. Starting it takes about 50 Kb plus the
size of the agenda file you use.

How to reach the author:
------------------------
If you find any bugs or have some remarks, please let me know. Support on
the program however is limited to the registered users.
Email: 100121.3102@compuserve.com.
Homepage: http://ourworld.compuserve.com/homepages/platodva/psion.htm.


