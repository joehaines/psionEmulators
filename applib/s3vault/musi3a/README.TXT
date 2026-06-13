                     IMPORTANT INFORMATION
                     =====================

     !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
     !  Before unzipping the application files in PRODUCT.ZIP  !
     !  please read LICENS.TXT.                                !
     !  You must not unzip PRODUCT.ZIP only if you do not      !
     !  agree with the contents of LICENS.TXT                  !                                      !
     !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!


     Installation
     ------------
     Please see INSTALL.TXT.


         -------------------------------------
         !           Warning !               !
         ! The unregistered version will nag !
         ! you a bit and will also occcupy   !
         ! more memory on your internal disk !
         -------------------------------------

Registration
------------
You can register through Compuserve or RegNet.
  MusiPoc LITE                US$19.95
  MusiPoc PROF                US$39.95

CompuServe: GO SWREG.
   Author: Berthold Daum
   CompuServe ID: [100026,3365]
   MusiPoc LITE:
      Program Title:     MUSIPOC V4.1 LITE
      Program name:      MUSIC3a.zip
      Registration ID:   3709
   MusiPoc PROF:
      Register for MusiPoc LITE(above)
     PLUS
      Program title:	 MUSIPOC V4.1 PROF
      Program name:      MUSICP.zip
      RegNo:             11858

RegNet - The Registration Network:
    MusiPoc LITE:
       RegNet #:  1485
    MusiPoc PROF
       RegNet #:  1486
    RegNet can be reached on the World Wide Web at the following URL:
        http://www.swregnet.com
    or by calling 1 800 WWW2REG (1 800 999-2734) or (805) 288-1827.

--------------------------------
Entering the registration number
--------------------------------

   Press MENU, / , ENTER, ENTER,
   then enter first name and the registration number.

   Note: The entered name must be part of the machines owner name.
         The registration number must be issued on the entered name.

Updates
-------

Updated versions can be downloaded from the CompuServe PSION forum or from

http://ourworld.compuserve.com/homepages/lichtbild


Author contact
--------------
lichtbild@compuserve.com

                 MusiPoc Version 4.1 - Release Notes
                 ===================================

Program Description
-------------------

An editor for musical tunes.
Includes features such as:
 Graphical editor with advanced music editing facilities.
 Tune length of up to 4000 notes and breaks.
 One voice, two voices, or one voice plus metronom.
 Tunes are stored as SND-files.
 Subroutine to play tunes out of applications or programs.
 Full documentation as help files and/or online manual.

Platforms
---------

 This program runs on PSION S3a and ACORN PocketBook II computers.

 S3a-Emulator:
  Tunes can be edited (and printed!).
  To make use of the larger screen run the emulator with a 640x480 screen.
  Tunes cannot be played - the emulator doesn't support sound cards.


Applications
------------

 Collect musical ideas, create sound files for applications
 or for HyperPoc.


Requirements
------------

 The minimal amount of disc space needed is approx. 68k. (LITE)  88k (PROF)
 The maximal amount of disc space needed is approx. 125k (LITE) 150k (PROF)

 Unregistered versions need 16k more disk space on the internal disk!

Known problems
--------------

MusiPoc can lock up when a melody is played and the computer
switches off during playing due to low battery.

  Remedy: Change the batteries, switch on, press the SYSTEM key,
  then kill MusiPoc.

MIDI-Import MusiPoc PROF: Restrictions

  - Currently songs with SMPTE timing are not imported.
  - Songs with a master track > 16kB are not imported.
  - Polyphonic channels are serialised.
  - Very long songs usually end up with "No system memory".
    This is caused by the limited segment size of 64 kB - independent
    of the actual amount of free RAM.
    MusiPoc will truncate the song.

New in Version 4.21
-------------------
Entering notes by names:
 A new option (Other Preferences) allows to use the Shift key when
 entering notes.
 Enter the name of a note in lowercase to move upwards, or in upper
 case (Shift key down) to move downwards. This works, too, when
 modyfying a note.


New in Version 4.20
-------------------
-   Stretch option in Preferences allows control horizontal distance
    between notes.

-   MusiPoc PROF: New Lyrics function (PSION-Shift-Y) allows to
    edit lyrics. MusiPoc invokes WORD to enter text.
    There are two ways of synchronizing text with tune:

    1. by measure
       This is the easier way.
       To synchronize text with measure bars:
       		TAB
       		soft hyphen (depends on keyboard layout, see PSION user manual, WORD)
       		line break  (Shift-Enter)
 
    2. by note
       This is much harder to edit, but you can work on a syllable
       by syllable basis. The text always runs with voice 1.
       Use the following characters for note synchronisation:
       		blank (SpaceBar)
       		hyphen (-)
       		soft hyphen (depends on keyboard layout, see PSION user manual, WORD)
       		line break (Shift-Enter)
 
       To suppress synchronisation for a space or hyphen use:
       		protected space (CTRL-SpaceBar)
       		hard hyphen (depends on keyboard layout, PSION see user manual)

    MusiPoc determines automatically which synchronisation method is used:
    If it finds at least one TAB character in the text synchronisation
    by measure(1) is used.

    A paragraph break (ENTER) finishes the lyrics and starts the comments.
    You can also use Header and Footer in WORD for additional information,
    like copyright, or title.

    Press PSION-X when finished to return to MusiPoc. MusiPoc will redraw
    the tune with the text.

    Lyrics are stored in separate file (.wrd) in the same folder as the
    tune. For instance, the lyrics file for CURACA.SND is CURACA.WRD.
 
    You may want to select a wider stretch for a tune with lyrics (see above).

    Lyrics can be exported to and imported from MIDI
    (always in the track associated with voice 1)

-   Support for RIFF (.rmi) files added. MusiPoc PROF can now importRIFF-Midi files.


New in Version 4.17
-------------------
-   MIDI-file export in MusiPoc PROF: Generated files are now
    optimized for size resulting in smaller files.

New in Version 4.10
-------------------
-   MusiPoc can now handle up to 16 tracks within one tune. To assign voice 1
    or voice 2 to any track or to define the number voices press PSION-V.
    Voices can be edited, printed, and played.
    To delete a track press PSION-Shift-D.

-   Assymetrical triplets are now supported.

-   Option "Show keypad" in "Other Preferences".
    Shows a tiny piano keypad on the left side of the screen where
    the current note is highlighted.

-   The diamond key is no longer used to switch between voices.
    Instead use the up/down cursor keys to switch between voice 1 and voice 2.

-   The keyboard can now be switched between alpha mode and piano mode.
    To switch between the two modes press the diamond key.
    In piano mode the keys represent piano keys (as in the Acquire function).
    The note duration is modified through CONTROL left/right cursor key.

    Note: The lowest note (fis) cannot be entered in piano mode through
      the left shift key, because this key is reserved for other purposes.
      Instead enter a "g" and use CONTROL-cursor-down to push it one halftone
      down.
      The TAB key and ESC key are assigned to notes in piano mode.
      To use TAB and ESC in their original meaning use CNTRL-TAB and CNTRL-ESC.

-   New printing options
    - Printing in landscape format (depending on printer model)
    - Number of copies
    - Paper format

-   MusiPoc PROF released. This version has export and import facilities for
    MIDI. You can import and export from/to Type 0 and Type 1 standard midi
    format (SMF). During export you can assign MusiPoc tracks to MIDI tracks
    and channels and you can assign instruments to MusiPoc tracks. This
    export setup will be stored with the MusiPoc tune, too.

    To export a tune invoke "Save as" then select MIDI. After specifying
    the output file and the MIDI file type (0 or 1), a table pops up.
    Type in the MIDI channel numbers. You can map more than one MusiPoc
    track to one MIDI track (with identical or different channel numbers)
    or you can supress the export of a MusiPoc track by deleting the
    channel number with the DEL key.
    Press MENU to access other functions like assigning a MIDI program,
    or adjusting volume and pan for a track.

    To import a MIDI tune, simply open a (.MID) file. MIDI files with SMPTE-
    timing or polyphonic channels do not import in the present version.
    Please see the manual for details on importing MIDI files.



New in Version 4.02
-------------------
-   A title line can be specified for printouts.
-   Margins can be specified for printouts.

New in Version 4.01
-------------------
-   The screen representation of short breaks has been adapted to the
    de facto standard.

New in Version 4.00
-------------------

-   Accidentals
    Accidentals are now written in front of a note.
    Only if you change one note and there is not enough space
    in front of the note, the accidental will be written above the note.
    With the next redraw (PSION-*) the accidental will appear in front of the note.

    You can now define explicite accidentals to indiviual notes.
    Press PSION-E to edit a note and select an accidental.
    "auto" is the normal mode - sharp accidentals are used in sharp key signatures
    and flat accidentals are used in flat key signatures.

-   Staccato and Marcato.
    Press PSION-K to make a note or selection legato, staccato, or marcato.

    Note: Marcato is displayed as the note without any attributes.
          Legato notes have a slur bow to the next note.
          Staccato notes have a dot below or above the note.

    You can also press "-" for legato, "." for staccato, "," for marcato.

    The default mode is legato, so your existing tunes will appear with
    slur bows.

    Note: Slurring is different from tied notes. A tied note is one
          entity extending over one or more bars or with an odd duration.
          To define a note with any duration use the edit function PSION-E.

    Staccato and marcato notes need twice as much memory space as legato
    notes, because internally they are represented as a sounding note
    plus a break. (For staccato the sounding note is very short and the
    break is the dominant part, for marcato the sounding note is dominant
    and the break is just 1/32.

    Note: The S3a sound system quantifies durations in 1/32. This makes
          it impossible to apply staccato and marcato to 1/32 notes.
          (Also to the first and third note of a 1/16 triplet)

-   Musical terms.
	Insert text terms like "Adagio", "Diminuendo" into your tune
    using PSION-Shift-I. This feature is glossary based - when you press
    PSION-Shift-I a window will pop up for selection fo the appropriate term.

    The glossary contains about 160 predefined terms but you can add
    new entries to the end of the glossary by pressing the TAB key while
    the glossary is visible or via PSION-Shift-A. You cannot delete
    entries from the glossary.

    Note: The glossary is a .dbf file. It must not be modified using the
          DATA application, as this would change the order of records. This
          would lead to unexpected results.
          If the glossary is not installed, tunes will be displayed without
          text terms.

-   Preferences.
    The Preferences have been split into
    Tune Preferences and Other Preferences

-   Acquire function (electronic organ)
    MusiPoc will display a keyboard. The top 4 rows of keys act as keys
    of a mini electronic organ. As you play, the display shows the key
    movement and the resulting score is written to the clipboard.
    Sampling speed:
    	Define a lower value to make the input slower than the replay.
    Smoothing:
        Rounds note and rest durations to
          ...,1/8,3/16,1/4,3/8,1/2,3/4,...
    Metronom:
        Metronom ticks with the interval defined in Preferences

    When playing you can change the playing mode with the diamond key:

    	- Disconnected: Note sounds as long a key is pressed
    	- Connected: Note sounds as long as the next key is pressed.

    The MENU key quits the Acquire mode.

-   Tonal system
    The tonal system can be selected in Tune Preferences.
    The default tonal system is the tempered system.

    Some other systems are available:
        C-natural, E-flat-natural, B-flat-natural, F-natural.

    These systems are useful when emulating scores for certain wind instruments.

    You can define your own tonal systems (for indian or medieval music for instance)
    by adding entries to the file

      \APP\MUSIPOC\SCALES.DBF

    Each record contains the name of the tonal system (<= 20 characters)
    and the frequencies of the top 12 notes.


-   Transposition
	You can now define transposition values in steps of 1/100 half tones
    to match the pitch exactly to other instruments.

-   Triad function
	A new function (PSION-Shift-T) extends the current note into a triad.
    A shortcut is "<" for triad upwards and ">" for triad downwards.

-   Faster and better printing.
	High quality print mode ("Better") prints four times slower than the fast mode
    but with twice the resolution.

-   MusiPoc now releases the sound device when it is moved to the background
    or the machine is switched off. This allows Alarms to ring even if MusiPoc
    is still running.

-   The Import function has been removed. To import an S3-tune simply use the
    Open function, press TAB and select the tune from the file selection list.



Documentation
-------------

MusiPoc.WRD is an online user manual that can be opened with the WORD application.

Documentation of TUNPLAY and sound file format : TUNPLAY.DOC













