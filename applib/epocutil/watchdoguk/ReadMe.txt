1.  Copyright  
2.  Overview 
2.1 The Problems 
2.2 The solution 
3.  Installation
4.  Quick start 
5.  History 

--------------------------------------------------------------------------------

1. Copyright
Version 1.04 (c) Michael Franzen, 2002
michael.franzen@eplus-online.de
Watchdog is freeware, Copyright 2001, 2002 by Michael Franzen. Redistribution of
Watchdog is permitted as long as it is done in form of the original SIS-file.
Usage is on your own risk. The author is not liable for problems which are 
caused directly or indirectly by this program.

--------------------------------------------------------------------------------

2. Overview
2.1 The Problems:
- Battery usage: The system's display of the power consumption is unreliable, 
  since it doesn't count the Psion's off-time. But the internal real-time 
  clock and the memory even drains battery current when the Psion is "off". 
  Furthermore, the system does not estimate the remaining on-time or 
  standby-time. And the build in battery alert doesn't work for rechargeable 
  batteries (at least NiCd and NiMH).
- Alarms: The system does not allow to restrict the duration of alarms. They 
  ring as long as they are cancelled by the user. Without user intervention, 
  alarms will go on until the batteries are empty.

2.2 The solution:
Watchdog supervises at least in one minute intervals the power consumption and 
the state of the alarm server.
The power consumption observed is used to estimate the remaining on-time and 
standby time as well as the remaining battery charge. Furthermore, the 
measured values are used to calculate the battery-capacity which will be 
stored in accompanying battery files. The accuracy of the capacity estimation 
will grow with the number of usage cycles of the batteries. 
The observation of the alarm-server leads to a automatic cancellation of 
ringing alarms after a configurable time duration.
Since Watchdog runs only for a short time every minute if the device is turned 
on, the additional power consumption caused by the program is negligible.

--------------------------------------------------------------------------------

3. Installation
Provided you have PSIWIN installed and your PSION is connected to your PC via 
the serial link, double-clocking the file "Watchdog.sis" is all you have to do 
to install Watchdog. Note that there are separate SIS-files for the english,
 the german and the dutch version. It is important to choose the same language 
as your EPOC-Version uses since Watchdog uses language specific shortcuts.
If you own a Series 5 classic you have to install also the file DATEOPX.SIS 
using the same method. 
Beside the Watchdog files the SIS-file also installs two more OPX-files if they 
are not already installed: Sysram1.opx and System.opx. All three are copyrighted
by Symbian and all three are available only in English. Redistribution is not 
allowed unless you use the SIS-file version as they can be found on the PSION 
web site.
If you have an older version of Watchdog running on your machine, just close it
and install the new version above that. It is not necessary to uninstall the
older version first. Prior to starting the new version the file 
"System/Apps/Watchdog/Watchdog.ini" has to be deleted. After the new version 
was started, please have a look on the configuration and change it as desired.
However, if you are using the "default"-profile for quite some battery-cycles 
now, it is a good idea to rename that one 
(System/Apps/Watchdog/BatPf/default.pbf) prior to installing the new version.
Otherwise you will loose the statistics already collected. (Be careful to 
choose a name which ends with .bpf!) After starting the new version you then 
should switch to this renamed profile via the dialog "Battery/Select profile".

--------------------------------------------------------------------------------

4. Quick start
Normal operation of Watchdog is not very complicated.
The best occasion to start Watchdog for the first time is right after you 
inserted a fresh (or fresh recharged) set of batteries. Watchdog will then 
present a dialog in which you have to choose a battery-profile. By pressing 
the "New"-button you proceed to a dialog, which allows to enter some values 
for a new battery profile: A name and the capacity (which is normally printed 
on the battery in case of rechargeable devices). Alternatively you can just accept the "Default" profile presented in the very first dialog. This profile 
is based on the template "StdAkku" and is suited for rechargeable batteries 
with about 1100 mAh capacity. Watchdog generates the new profile based on the 
template "StdAkku". If you use normal batteries, you should press the 
"Template"-button before entering anything else. Select then the template 
"StdBat" and quit the dialog by pressing "OK". This template-dialog allows, by the way, to choose between all templates which are stored in the folder "System\Apps\Watchdog\PatPf" and base your new profile on every pre-learned 
template which you can find for your battery (and machine) type.
Now you only have to leave the "New battery-profile"-dialog by pressing "OK", 
choose the newly generated profile in the reappearing "Select battery-profile"-
dialog, and terminate the whole process by pressing "OK" again. That's it!
For completion of your first impression of Watchdog it would be helpful to have 
a look at the topics "The Display", "Warning-Messages" and "Battery insertion" 
in the online help. 
Enjoy!

Note for users of non Series5-type equipment:
Since I do not now to much about the battery management on your type of
machine, battery observation is turned of by default after installation. 
Watchdog will start up without presenting the dialogs mentioned above and the
Watchdog window will just show the program's icon. However, the alarm 
observation should work.
If you own a battery powered device (e.g. Osaris, GeoFox), it is probably worth 
trying out the battery observation, albeit it will be not very accurate in the beginning. (But it will learn!). It can be enabled in the dialog "Battery/Select profile".

--------------------------------------------------------------------------------

5. History
Version 1.0 (1.12.2001):
  - Initial Version

Version 1.01 (13.01.2002):
  - Watchdog is now installable on drive D:
  - Added support for MC 218
  - Corrected 5mx startup bug
  - updated help file
  - added readme.txt file (this file) to the zip-archive
  - added default-profile

Version 1.02 (03.02.2002):
  - rudimentary support for non Series 5 machines (at least the 
    alarm handling should work now on these machines)
  - it is now possible to disable the battery observation. (So if both alarm and 
    battery observation are switched off the program does nothing useful any 
    more :-))
  - the Watchdog window can now be moved to any corner of the display
  - handling of silent alarms was made slightly more consistent
  - the quick-start section from the help-file was moved entirely to this file.

Version 1.03 (10.02.2002):
  - corrected interference with password/personal information screen
  - split up the app into two separated SIS files (English/German)

Version 1.04 (20.04.2002):
  - Watchdog now behaves well when switching time zones or daylight saving
  - deleted "silent alarm duration" in Alarm menu (it was too confusing)
  - new option in alarm menu: Alarm handling. Watchdog now can switch off
    the machine immediately when an alarm is detected.
  - Watchdog now detects when empty batteries are inserted instead of new ones
  - (internal) initialisation somewhat optimised

Version 1.05 (22.06.2002):
  - version with dutch menus added (thanks to Zegert van Eijk)
  - corrected bug which showed empty batteries when EPOC reported very low 
    power consumption

Version 1.06 (14.07.2002):
  - Partly adapted help file for dutch version
  - New automatic statistic update: Instead of displaying the "mismatch" 
    warning, Watchdog now starts collecting new data automatically while 
    continuing with old values as long as the new statistic isn't valid.
  - Snooze option added in "Alarm/Configuration"
  - Default value for "minimal normal Power consumption" for 5mx was lowered 
    to 144 mAh.
  - Watchdog now continues to work well even after clocks were synchronised to 
    the PC.
  - Batteries are no longer treated as new if they occasionally lose contact
  - New command "Batteries exchanged"
  - Removed command "Battery/Add Record"

