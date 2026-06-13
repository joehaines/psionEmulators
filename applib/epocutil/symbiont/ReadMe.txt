SymBiont 

V1.01
Automatic Restart of Documents/Applications after a Backup, Automatic Activation of Remote Link to PC When External Power is Plugged In, Sound Control, Date/Day Calculation, HEX/Decimal Conversion, Unit & Currency Conversion, Measuring Ruler, Battery Monitor ...

By Steve Beadle
steve_beadle@iee.org
http://www.biblet.freeserve.co.uk

What does it do?
This program is a 'super-utility' which performs many useful functions.

Installation
SymBiont requires Macro5 - Get Macro5 from any decent web site such as PsionKing (www.psionking.com).  Install Macro5 using its own method.  If you *really* don't need Macro5, then you can just install the Macro5 Library (Macro5Lib.SIS).

To install Symbiont run the SIS file.  During installation the OPX 'Systinfo' will be installed.

SymBiont should then appear as an Extras icon.

How does it work?
At the heart of this program is a routine which runs checks every few seconds.  This interval can be configured, the default is 15 seconds.

The following functions can be performed at the stated time interval (all are individually enabled):

If a (selectable) file is open, closed and open again, then conclude that a backuup has been done, and open all files in a folder (selectable).

If external power is present then enable the PC link and vice-versa.

If external power is present then disable all sound and vice-versa.

Monitor main battery voltage and issue warning messages and beeps.

Utilities are accessed from a menu and will pause the timed actions until the utilities are finished with.

The Display
When in the foreground, SymBiont uses a small window overlaid on the current application.  The window shows the battery voltage and some configuration settings.


Controlling SymBiont
The menu key (and silkscreen button) allows you to setup SymBiont, control link, sound and restart, and use its utilities.

When the setup option is chosen to allow SymBiont to exit on restart (not the default option), SymBiont will appear to be a little slower to react to the menu key.  For this option a bit more CPU (power) will be consumed, and the SymBiont window will react to pen clicks and turn the PC link on and off.

File Menu Options
Restart Now - Execute the restart.

Link On/Off - Change the PC link from its current state.

Sound On - Enable all sound, including keyclicks etc. if previously enabled.

Sound Off - Disable all sound.

Background (Esc) - send SymBiont into background.

Close - Exit SymBiont.

Utilities Menu Options
All these menu items will pause SymBiont during their use.

Units - Convert between selected units.  The first conversions are customisable, allowing you to use them for currency rates; press Ctrl-C to set up the names of the two currencies and the conversion rate.

Days - Works out the number of days between two dates and the date a given number of days after or before another date.  Enter two fields and position the cursor over the result field and press enter.  Use Tab on the date fields to see a calendar view.  Press Esc to exit.

Hex - Converts between hex and decimal (16 bit).  Enter a value (in the hex or decimal field) and press return.  Press Esc to exit.

ASCII - Converts a single character code.  Enter a character or a number in the fields and press return.  The other field will be evaluated.  Press Esc to exit.

Ruler - Displays a measuring ruler - you can select the units in cm, inches or pixels.  Also you can press Ctrl-C to 'calibrate' an alternative scaling.

Tools Menu Options
Restart Settings - sets the backup restart behaviour - see below.

Other Settings - sets other options - see below.

Help - Open the SymBiont help file.

About - Version information.

Restart Settings
This program restarts documents and applications which have been stopped by PsiWin backup but which PsiWin cannot restart as they are not built-in.  i.e. Data and Agenda get restarted but Macro5 or BatCheck don't.  It does this with no user intervention.

SymBiont monitors a selected file and detects when that file is closed and opened.

Restart documents after backup
Tick this box to enable the monitoring and restart function.

File to monitor during backup
Choose an internal application file you ALWAYS keep open, e.g. Agenda or Data files.

Folder of documents to restart
Files to be restarted are specified in a directory. You can use shortcuts from NoMore, FileLink or Start5 to start programs like CronTab.  Files ending in an underscore will not be started.  All files in a sub-folder of the specified folder called Startup will also be opened.

I set it to open files in my \Documents directory and keep other shortcuts in a subfolder called Startup to keep them out of the way.

Other Settings
Allow Exit for Backup
This needs to be UNchecked for the backup restart function to operate.  Even if you are not using SymBiont for this function you are advised not to allow SymBiont to exit.

The first time you do a backup after installing SymBiont you need to stop SymBiont so that its files are backed up.  After that SymBiont can be left to run permanently.

Check Interval (seconds)
Time period between checks of external power, battery, link and monitored file.  Do this too often and you waste power.  Not often enough would be inconvenient if backup completes too fast or you sit waiting for the link to be enabled after plugging in power.

Enable Link if External Power Present
This program turns the external link on or off according to the state of the external power source.  It is useful when you mainly use the remote link when you are at your desk with the external power plug in at the same time.  It does all this with no user intervention.

When started up SymBiont will set the link state according to the current state of the power input.

The latest versions of MessageSuite programs disable the link automatically but do not reenable it again when you finish with them.  PowerLink gets round this annoyance if, when you use MessageSuite 'mobile' you plug back into your PC to backup or transfer files.

Disable Sound if External Power Present
As above, this option is controlled by the external power.  It can be useful when you do not want sound to disturb others when you are at your desk.  This option is NOT enabled by default.

Battery Level Warning
This can be set to one of four options:
Disabled - report voltage to SymBiont screen every minute or so but do not warn on level.
No Sound - When voltage crosses 2.3V and 2.1V, SymBiont will come into the foreground for a couple of seconds to warn you.
Quiet - As above, but make a discreet beep when giving the warning.
Loud - As above with a louder beep.

Acknowledgements
To: Pascal Nicolas for his excellent Macro5 utility, required for this program.
http://www.geocities.com/SiliconValley/Pines/1215

Copyright
This program has been written and is owned by Steve Beadle. It may be distributed freely as long as it is not altered or sold. This program is SmileWare, which means that if you use it, please send me a :-) .
I am supporting my City Centre Methodist Church, which is deeply in debt as a result of having (by law) to keep its buildings in good order.  We are a community which is serving a unique community in various ways and I would be grateful to receive any donations towards our £50,000 debt.  If you like my software and find it useful then please visit www.biblet.freeserve.co.uk/bsmc to find out more and perhaps to make a donation.

Thank you to all of you who send me comments, new ideas, and :-)

Disclaimer
While this program has been extensively tested I will not be held responsible for any problems it may cause (sleepless nights, flat batteries etc).  Please forward any comments you may have to:
	steve_beadle@iee.org
Updated software is available via:
	http://www.biblet.freeserve.co.uk

History
V1.01	First Public Release


Steve.