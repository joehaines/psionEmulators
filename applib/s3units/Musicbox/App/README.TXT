
Welcome to Musicbox v1.5
------------------------


Introduction
------------
This musician's utility provides:
     a metronome
     a piano keyboard
     a note sounder
     a tempo measurer
     a timer

Please distribute this shareware freely to others.
Do send me any comments or suggestions; my address is given below.


Installation
------------
Suggested file structure:
Required files:
     \APP\Musicbox.opa            This is the main program
Optional files:
     \APP\MUSICBOX\Musicbox.rsc   This contains text for the help
screens
     \APP\MUSICBOX\Tock.wve       Sample sound files for the metronome
     \APP\MUSICBOX\Click.wve      ditto

If you are short of memory, the optional files can be removed.

Files created:
     M:\OPD\MUSICBOX.INI      Your customised settings, saved on exit
     M:\OPD\TEMP.MU$              Copy of last tune to be used;
preserved on exit
     \MBX\tunename.TUN            By default, recorded tune files


Usage
-----
Most of the program functions are fairly obvious; do browse through
all the menu options. Help screens explain all.


A few things that may not be obvious:

In the metronome section, you can change the metronome speed by just
typing the number (eg 66 or 125). No need to press enter.

When the metronome is off, you can measure the speed of some music by
pressing Enter in time to the rhythm. The speed indicator sets itself
to the speed. "How many times do I press it", I hear you ask: doesn't
matter. Twice starts it, the more you press the more accurate it
becomes.

To hear a note, press that key letter. Shift and Control sharpens and
flattens the note. A double press holds the note constantly until you
press it again to stop it.

If you've recorded a tune in the piano section, you play it back by
pressing any cursor for each note. The rhythm is not stored.


Low battery problem
-------------------
Due to a bug(?) in the Psion operating system, the program can stop
working if you are playing a sound when the batteries are low. You
must go to the system screen, highlight "Musicbox" and press Delete to
terminate the program.

(For the technically minded: Anyone know a solution to this? If the
voltage falls *during* an asynchronous call to the 2-voice tune
function, it hangs the calling process)


Metronome
---------
Space turns metronome on and off
Change speed by typing number (nothing is shown on screen). No need to
press Enter

Use cursor keys to change speed:
Left, slower: Right, faster;
Psion-Left, much slower; Psion-Right much faster
You can adjust speed without stopping metronome

Hold down Enter to pause metronome
Next click happens as key is released
Use this to advance or delay next click

Delete turns sound on and off

'Set metronome preferences' customises metronome's appearance:
'Blinkers' for flashing arrows
'Slider' for progressive sliding line
'Sound type' for style of metronome click
Use simple beep (Choose pitch and length of beep) or select pre-
recorded wave file ('tock' is provided)


Piano
-----
Top two rows of keyboard play black & white piano keys

'Set piano preferences' customises piano settings:
'Sound style' chooses what piano sounds like
'Sustain' makes note sound for a little longer after key is released.
(Good for legato)
'Transpose' moves pitch up or down in semitones (eg -4 for down major
3rd)
'Bottom note' sets which note is played by far left key (Tab)
 'Show keyboard letters' for letter to be shown on screen

Use 'Pitch' for finer tuning if needed


Tempo measurer
--------------
To measure a speed, turn off metronome and tap Enter at speed to be
measured
Metronome shows speed


Recording
---------
Rhythm is not recorded, only notes
Tune is played back by tapping lower cursor keys (Left, Down, Right)
in rhythm required

'Record new tune' starts recording
To stop: Esc or 'Record new tune' again
'Up' deletes last note
'Add to current tune' starts recording, but notes are added to end of
tune currently in memory

'Play from start' or Esc or Space goes back to beginning of tune
When playing, 'Up' reverses back through tune

Use 'Save tune' to save tune for later use, giving it a name
Use 'Load tune' to select previously saved tune for playing
Select 'Yes' to add tune to end of one currently in memory

Use 'Transpose' in 'Set piano preferences' to play back tune in
different key


Timer
-----
Tab switches timer on and off
'Reset' puts clock back to 0:00
Press Tab twice in quick succession to reset timer

'Adjust' lets you start timer from predetermined time
'Lap' halts displayed time, whilst leaving timer running in
background. Use 'Lap' again to redisplay current time

'Format' selects whether hours are shown, or just minutes (eg 2:00:00
or 120:00)


Note sounder
------------
Press a letter to play that note
Control+letter adds flat (b) to note
Shift+letter adds sharp (#) to note
Note sounds until key is released

Pressing key twice quickly makes note sound continuously. Press note
again or Esc to release
The overall pitch of all the notes can be changed with 'Pitch'. Enter
the frequency (Hz) of a middle A (eg 440 for normal, 415 for baroque
etc)

(Pitch is unaffected by transpose setting in piano section)


General
-------
All settings are preserved on exit
'Reset parameters' restores metronome to default settings

No sound:
Is volume turned down or off?
Does the wave file selected for the click still exist?
Are the batteries low? This will make the sound switch off


History
-------
v1.4      Nov 96
          First public release

v1.5      Jun 97
          Changing metronome speed made easier
          Full volume control implemented
          Piano recording improved
          Menus reorganised
          A few small bugs fixed


Shareware registration
----------------------
To become a registered user please send œ10 (GBP) (or  equivalent
major foreign currency) to

John Neale
62 Long Ashton Rd
Bristol BS18 9LE
Tel +44 (0)1275 392336

Please tell me the version number and where you obtaind your copy
from. I will send you a code number which will remove the nag screen
on entry and exit.

Why should I bother?
- You'll be treating me fairly
- You'll have a clear conscience
- I'll send you future improved versions
- Trust is what makes the shareware system work. Why else did I make
this software available?
