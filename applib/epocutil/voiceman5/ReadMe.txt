Welcome to VoiceMan5 - a freeware Voice Notes manager for your Series 5/5mx.

What is VoiceMan5?
==================
As someone who drives quite a bit in my job, I find the Psion's Voice Notes / Recorder
facility very handy (once you get over the strangeness of talking to your Psion!) for
capturing "to-do's" and ideas as I'm driving along.  At least, it works very well so
long as you want to review the Voice Notes you've made as soon as you next switch your
Psion on.

Very often however, I want to do something else when I open my machine again and so
Voice Notes gets sent to the background whilst I browse my Agenda, look up a phone
number, etc.

The problem is that I then invariably forget to review or action the Voice Notes I
made when I next switch the machine on.  What I wanted was an unobtrusive program
that reminded me about pending Voice Notes every time I switched my Psion on again...

VoiceMan5 is designed to check this for you.  It's a small program which checks to see
if you've any Voice Notes recorded on your Psion when you switch your machine on.
It's very small and consumes little power or system memory and so can be left to run
in the background all the time.


What else can it do?
====================
VoiceMan5 can also change the default drive used by Voice Notes to record from c: to
d: (and visa versa).  This can be used in conjunction with the Standard Folder
location (specified in the System preferences) to record your Voice Note(s) to
wherever you want them.  The advantage of recording to the d: drive (i.e. compact
flash) is that it gets around the Series 5's 8Mb internal memory limit (16Mb on a
5mx).


Using VoiceMan5
===============
When you run VoiceMan5 for the first time it'll scan your Psion's C:\ drive¹ to see
if you have any Voice Notes recorded².  If you don't, it'll goto the background and
wait until you next turn the machine on - when it'll scan³ again for Voice Notes.

If it finds recorded Voice Note(s)², it will present you with a number of options:-
* 'Quit VoiceMan5' - Quits the program
* 'Settings' - Takes you to the Settings Menu where you can display the 'About'
   screen, run this 'Help' file, change the 'Voice Notes Drive', or choose when
   VoiceMan5's alert should be 'active' (what days, times, etc.)
* 'Ignore' - Sends VoiceMan5 to the background until you turn the machine on again
* 'Play Note(s)' - Takes you to the Record app. with your Voice Note(s) loaded and
   ready to play

There is no Menu structure in VoiceMan5 simply because I didn't really think it
necessary (and it'd just make the program bigger).  If there is a demand for it
however, I'll create a simple one.  You can quit VoiceMan5 either by selecting the
'Quit' option when the program comes to the foreground or by closing the program from
the Open files / programs list (Ctrl+J in the System screen).  VoiceMan5 will
automatically close and re-open itself when backing up using PsiWin (version 2.3 or
greater).

Notes:
¹  You can change the drive that VoiceMan5 scans on startup.  Click on the 'Settings'
option to do this.  It will also give you the option to copy or move an existing Voice
notes file when you change drives.
²  The program will ignore any Voice Note less than 500 bytes in size.  This is to
allow for the use of the 'Erase' capability in the Record/Voice Notes app. which
effectively 'empties' the Voice Notes file of any sound information - but doesn't
actually delete the file (leaving a small 'stub' file instead).
³  Just make sure that the Standard folder set in the System screen (Menu/Tools
Preferences) is where your last Voice Note was recorded.  (It will be by default
- this is just in case you've moved it!)


Compatibility
=============
VoiceMan5 has been tested on both an 8Mb Series 5 a 16Mb 5mx and an Ericsson MC218.


Installation
============
VoiceMan5.zip contains 5 files.  ReadMe.txt (i.e. this file!), VM5Changes.txt,
VoiceMan5.sis, Systinfo.sis, and SysRAM1.sis.  VoiceMan5.sis, Systinfo.sis, and
SysRAM1.sis install using the standard EPOC installer (either from your PC or
on the Psion itself - see Psion's own web site for details).  NB: These latest
(ER5) versions of Systinfo.sis and SysRAM1.sis must be installed for VoiceMan5
to work - make sure that you don't have any old (ER3) versions on your machine.


"Incompatible opx version" during installation
==============================================
If you should get this error when you're installing Systinfo.sis or SysRAM1.sis
(which contain their respective opx files) then one of two possible things could
be the problem:-

1) You've got an older version of Sysram1.opx or Systinfo.opx on your machine
causing conflict with the new one.  Symbian released a new version of the
Sysram1.opx file for ER5/5mx compatibility.  The new file will work fine on the
Geofox/S5 as well as the 5mx - but all of the machines appear to get confused if
there's a copy of the old sysram1.opx on the machine somewhere as well as the
new one.

Hence I'd recomend checking for old versions of both files either in c:/system/opx/
or d:/system/opx/ before installing the new versions.  BTW, old apps. will work
fine with these newer versions.

2) That you've got a program already running that's using either of these opx's
when you try to install them (e.g. Macro5).  Close down running programs before
the installation.


Standard disclaimer
===================
Whilst I have tested VoiceMan5 quite carefully, I cannot and do not give - or imply
- any sort of warranty with it.  Nor will I accept responsibility for any data
lost or damage caused as a result of using VoiceMan5 (this definitely includes car
crashes!)...  I.e. use at your own risk!


Updates
=======
Any updates will be via the Pscience5 web site at:
http://ourworld.compuserve.com/homepages/martin_guthrie


Best regards,
Martin Guthrie, July 2000
(martin_guthrie@csi.com)
