PUR Version 1.30F
_______________________________________________________________________________

PUR is the PowerUp Recorder for the Psion Series 3a. I know there are other
programs than can do recordings, even the built-in 'recorder' application can
do this. But - have you tried using them to record a short memo while driving
a car? Well, I do not have three hands... So PUR was born.

IMPORTANT NOTE: If you use PUR while driving or during any other activity that
                requires your full attentiveness you do this on your own risk!

PUR started as a modified version of one of the (former freeware?) power-up-
sound-playing programs (but there's hardly anything left of it and I really
can't remember which one it was). I liked the star trek tricorder sound on
switching the psion on, but during meetings it's annoying. So I changed it so
that if I switched the Psion on and hold the Psion key down, it kept quiet
(optionally you can switch this behaviour the other way round). You can
download some sample sounds from my homepage.

To record a short memo press ESC to switch your Psion on and keep the key down,
until a short beep notifies you that the recording function is ready to start.
Take your finger off the ESC key and the recording starts until a predefined
time is over or ESC is pressed again. After that, the Psion switches off
automatically and is ready to be put back in your pocket.
PUR will *not* play the memo(s) unless you tell it so (unlike an other program
does on switching on).

Other features:

 - zoomable list
 - Power-up soundfile selectable
 - the beeps that notify you that a recording starts/ends still will sound
   even if you forgot to switch beeps on
 - key-clicks are turned off during recordings
 - you can record directly from PUR screen (press SPACE - and don't wait for
   the second beep!)
 - PUR will refuse recording if the device is not of RAM type (either internal
   or A/B)
 - If recording to the selected SSD fails, your memos can automatically be
   recorded to the internal drive (optional)
 - max. recording time is selectable and will be reduced if storage space is
   tight. However, a small amount of disk space (adjustable) will be kept free,
   so that you have always some memory to keep the whole thing working
 - Play the memo directly from the system screen by 'opening' one of the listed
   files like you would do with other applications (to switch to PUR without
   playing a memo select the bold printed entry "Pur" which actually is not
   a filename but the application's name)
 - Lock memos against accidental deleting (uses RO file attribute)
 - Sorted lists

To install PUR copy the files to your Psion as follows:

  \APP\PUR.OPA         (the program, mandatory)
  \APP\PUR\PUR.FON     (special characters, mandatory)
  \APP\PUR\PUR*.FON    (special characters, optional but recommended)
  \APP\PUR\PUR.HLP     (the help file, optional)

Note: You do not need my library file "\OPD\BWB-LIB.OVL" any more, please
      delete it if you had a previous version installed.

The files need not be put on the same drive, however they must be available
at program start and while the program is running. Although the help file
is optional, I recommend installing and reading it thoroughly. Later you
can remove it if you like. The supplied small font files (*.FON) contain
some special characters. If you really do need this less than one KB more
free disk space, you may omit FROM THOSE WITH NUMBERS IN THEIR NAME any or
all of them: the numbers refer to the zooming level, i. e. PUR0.FON is the
smallest, PUR3.FON the largest font. PUR.FON may not be omitted!

Start PUR by selecting *any* file under the PUR-icon (if you already have
memos in \PUR\ they will be shown beneath the icon) and press ENTER. If later
you want to switch to PUR from the system screen, chose the bold printed entry
"Pur" under the icon. If you select a different entry, the corresponding memo
will be played immediately.

The following files/directories will be created:
  M:\APP\PUR\PUR.INI   (the configuration settings)
  ?:\PUR\              (your memos go in here)

PUR.INI can be moved to A: or B: afterwards, but should not be put on a Flash
SSD if you want to use the option to automatically save your settings.

If you are upgrading from an earlier version of PUR please check your settings
and re-save them (i. e. leave the dialogs with <ENTER>, not <ESC>), since the
INI-file contents may have changed for this version and the file so might have
to be rewritten.

The known problems:
(which appear not to be likely to go away in the next time)

- IF you have your password switched off AND you have set the system screen
  update option to 'automatic' AND you switched your Psion off with the
  system screen in foreground, THEN you will have to keep ESC down quite a
  while before you get the notification beep for the recording, because PUR
  will not get the power-on event until the system screen has finished
  updating. So I suggest that you either do always use a password or do not use
  the automatic system screen update option (which is the default) or make sure
  that you don't switch off with the system screen in foreground ;-) *or* you
  give me some advice how I could handle this a little bit smarter.
- for the same reason it can be quite some time before PUR gets to foreground
  if a memo is present
- You can't play a memo by selecting a file from the icon's list in system
  screen if you have chosen to open multiple files using ENTER instead of
  SHIFT-ENTER, because this would cause the system screen to launch a new
  instance of PUR (which is not allowed for several reasons)
- Since PUR is a type 2 application it is possible to create a new list (alias)
  for PUR. However I have not provided support for PUR being aliased and
  therefore the behaviour of PUR launched via an alias might not be as
  expected. Tell me if you have a very good reason why I should add support
  for PUR-aliases
- The larger you chose the default memo length the poorer is the responding
  time of PUR concerning actual start of recording after the confirmation beep.
  This is because at the start of recording the complete memo file is created,
  it is truncated after the recording is done. Creating a file for e. g. 60
  recording seconds (60 * 8KB = 480KB) needs some time ;-) So 10-20 seconds
  should normally be enough, if you need to take longer memos, allow one or two
  seconds for the file to be created after the confirmation beep.

--- SHAREWARE -----------------------------------------------------------------
After having spent so many additional programming hours, I decided to release
PUR as ShareWare, which means that you are allowed to test this software for a
reasonable amount of time. If you finished testing and want to keep using it,
you have to register by sending me the equivalent of DM 20 in any well known
currency (preferrably DEM, USD, GBP) in cash. You may round to a suitable
amount - I won't complain if it evaluated to DM 15 (and neither if it's more)!
You will in turn get a registration code via email, fax or snail-mail, which
allows access to the currently disabled features of this program.
Registered users will recieve updates via email as soon as they are available.
If you cannot handle email, we'll find a different solution - just ask.
-------------------------------------------------------------------------------

--- LEGAL STUFF ---------------------------------------------------------------
You have the one and only right to use this program for the intended purpose.
You may not in any way reverse translate the program. You may distribute the
program provided that you keep all supplied files together and do not charge
for the program. You may charge a reasonable amount for the distribution, e. g.
for copying floppy disks. YOU MAY NOT DISTRIBUTE THE REGISTRATION MODULE
(PUR.REG) which you recieved upon registration. Although this program has been
carefully tested I can not guarantee that it works as desired. I AM NOT LIABLE
FOR ANY PERSONAL OR MATERIAL HARM CAUSED DURING THE USE OF THIS PROGRAM. In
particular, I have to recommend not to use PUR while driving since this will
restrict your necessary attentiveness. USE OF PUR IS AT YOUR OWN RISK!
-------------------------------------------------------------------------------

Feel free to contact me if you have problems or suggestions, even if you have
not registered, and please report all bugs you find.

Have fun!

_______________________________________________________________________________
TO-DO's:

  You are kindly requested to comment on all of the following, since they are
  currently just ideas, not plans, and the more people comment the better is
  the chance that they get implemented!

  - allow adding textual notes to memos (1-3 lines)
  - pause function
  - trim function (automatic/manual) (cut off silent parts at start and end)
  - compress function (cut out silent parts in the middle)
  - what else???

_________________________________________\____________________Brendan W. Breede
                                          \
D-28325 Bremen                             \
Graubuendener Strasse 77                    \
+49-421-400104 (fax+voice)                   \
brendan@informatik.uni-bremen.de              \
http://www.informatik.uni-bremen.de/~brendan/  \
________________________________________________\______________________________
[ Life is like a Ferrari: too fast - but so what, you can't afford it anyway! ]
