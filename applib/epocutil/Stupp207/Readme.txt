StartUpPlus for the Series 5(Version 2.07 - August 1st, 1997)
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! CAUTION >  users of previous versions should delete the parameter file StartUp+ from the standard folder !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
Introduction 
The Series 5 is a amazing palmtop. You can start as many applications as your system memory allows, you can store thousands of files in hundreds of directories, you can work hours on it even with the backlight. I do it in such a way. until I realized that four megabytes of the ram are filled with data from opened applications. If I leave the Series 5 for a moment (it happens sometimes) and restart it later, I need 
a) to get some information about the current resources 
b) to do some housekeeping - in the same way that I needed this in the good old time with the batches of paper on my desk. 
So I decided to automate that procedure with a small utility. which should give me some warnings when I start the Series 5 (this version) and which should do automatically some predefined tasks for me (future versions).

The current versions contains following functions :
displays a warning if the batteries (main and backup) are getting weak.
displays a warning if the free memory on the ram or on the compact flash is getting smaller than a predefined value.
turns the backlight on at startup between predefined times (optional).
turn the S5 as a night clock between given times.
check the parameter when the program comes in foreground.
plays a sound file if a warning condition is encountered (optional).
automatically starts standard applications (agenda, sheet, data and word) with a given file (optional)
stores the parameter in a file for the next sessions.
use pen control to select an option from the main screen
Hardware requirements  
A Psion Series 5 and a cable to connect it to the PC (for the installation).
Condition of use 
This program is freeware. Feel free to distribute the unmodified ZIP file.
Files  
StartupPlus.app
StartupPlus.aif
Readme Startup Plus (this file in S5 Word format)
Readme.txt (this file in RTF format)
Installation (with PsiWin 2.x)  
connect your Series 5 to your PC using the docking cable.
select Remote Link on the Tools menu in the system screen of your Series 5.
Go to the folder \System\Apps (either on C or on D).
select Create New Folder on the task bar.
Enter "StartUpPlus".
unzip StartupPlus on your PC.
Copy the files StartUpPlus.app  StartUpPlus.aif and Readme Startup from the PC directory you unzipped the file into to the new created folder StartUpPlus on the Series 5.

Setting up / How to use 
Start the program for the task bar.
The standard parameters are displayed.
You may now change the parameter. The parameter are active until you change them or you close StartUpPlus. 

Each time you start the Series 5 the battery level for the main battery and for the backup battery are checked, the free space on C and D are checked again the values you have entered as parameter and if anything is wrong StartUpPlus goes to foreground and displays the warnings.

Menu and Toolbar 

Close (Menu File / Ctrl-e / Toolbar)
Leave the StartUpPlus application. The parameters you have entered are saved. You do not need to reenter them the next time you start StartUpPlus.

About (Menu File / Ctrl-z)

self explanatory.

Show Toolbar (Menu View / Ctrl-t)

self explanatory.

Clear Screen (Menu View / Ctrl-c)

self explanatory.

Show Status (Menu View / Ctrl-u)

Displays the current values of the four parameter which are checked at startup. (Main batteries, backup battery, free space on C and free space on). If one of the value is reported as result of a warning condition the symbols "<<<" are displayed on the right part of the screen.
You must press ENTER to go on.


Show Options (Menu View / Ctrl-o)

Displays the current options of StartupPlus (automatic backlight, Sound, free space on C, free space on D). From this screen you can use the pen to select an option to be changed: just tip on the corresponding line on the screen.

Backlight (Menu Preferences / Ctrl-b / Toolbar)

Start Time (evening) and End Time (morning)The backlight will be automatically turned on at Startup between this times if the next option is not checked.
Disable automatic backlightCheck this box, if you do not need the automatic backlight function at start up (default)

Sound (Menu Preferences / Ctrl-a / Toolbar)

Choose the sound file to be played at startup when warnings are displayed

SoundFile to be played (only sound files are displayed)
FolderChoose the folder where the file is stored
DriveEnter C (Ram), Z (ROM) or D (Compact flash)
Sound alarm if warningCheck the box if this function should be activated
RAM (C) (Menu Preferences / Ctrl-c / Toolbar)

Enter a value (in kilobytes). If the free space in RAM goes under this value the warning condition for that parameter will be activated.

Flash (D) (Menu Preferences / Ctrl-F)

Enter a value (in kilobytes). If the free space on the compact flash  goes under this value the warning condition for that parameter will be activated. If you enter a value bigger than 0, the warning condition at startup will be activated if no flash disk is present in drive D.

Nightclock (Menu Preferences / Ctrl-J)

With this option you can define a time slot in the night. If you start the s5 between the given time, StartUpPlus will go to foreground and displays a clock. If you press "o" StartUpPlus switches to the normal mode. If you press another key the S5 will switch off.

OPEN (Menu Applications)

You can here define applications/documents, which are started at power on (if they are not already started. At the moment you can select one document for each of the following applications
- Agenda, Word, Sheet and Data

List (Menu Application)

displays a list of the document which will be opened at power on and gives the possibility to change these options.

A few words about the parameter file.

The parameter are stored in a file named STARTUP+ in the standard folder. (you can define the name of the standard folder. (You can change the standard folder from the System Menu (Tools/Preferences  Ctrl-k). It will takes 1 Kb. 
If you want the file in an other folder do the following steps :
Close the application Startup+
Go to the system folder
cut the file Startup+
go to the folder to which you want to have the file
paste it
double-click the file or press enter to start StartUp+ from that document (It is important to not to start the program from the extra bar.)

Limitations and bugs 
This is my first program on the EPOC32 platform.. It is in a relatively early state. It has only been tested on the American model (8 MB). Please report any bug to the author. Since this program is licensed free of charge, I provide absolutely no warranty of any kind, either expressed or implied. The entire risk as to the quality and performances of this program is with you. 
Improvements for future versions 
I am still developing the program. I want to introduce the following new features :
storing parameter on a file between sessions
starting given applications and files if not already started
stopping given applications and files if already started
using current sunrise and sunset time for the backlight option
improved design of the display option and display setup (with pen facility to select the options)
Multilingual version (German and French)
etc., etc.   comments and suggestions are always welcome
History 
July 17th 1997 - Version 1.00
July  27th 1997 - Version 1.02menus and toolbarautomatic backlight : optionalsound at startup : optional
August 6th1997 - Version 2.05parameter fileMore pen controlApplication startup at power onsome minor corrections
August 15th 1997 o Version 2.07New function : Night clock
Comments and Feedback 
Please report all bugs, wishes. comments and suggestions to the author at following address:
  <Michael J. Ayguesparsse> michel_a@bigfoot.com
 