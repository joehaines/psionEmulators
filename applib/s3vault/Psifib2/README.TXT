                                PsiFibs
                             Version 2.0
                       (c)Peter Fankhauser 1996
                       fankhaus@darmstadt.gmd.de

                             

Description
-----------

PsiFibs is an offline replay facility for backgammon matches
played on FIBS (First International Backgammon Server). It
runs on the Psion Series 3a and on the 3a-Emulator for PCs.

PsiFibs supports a graphical display of the board, a scrolling windows
for moves and annotations, and an input line for trying out moves
before viewing them.

Further features are:
- Import facility for matches in rfibs-format
- Export of positions to the position database PsiBgp
  (shareware)
- Menue based interaction in line with the general
  guidelines for the Psion Series 3a
- Flexible means to adapt the display to one's preferences
  (switch the direction of play, toggle Black/White, change
  the size of the status-window and the annotation window,
  toggle pip-count, view annotations before or after the moves)
- Automatic simplification of the move notation
  (for better readability and to fit the moves onto the
  fairly small screen)
- Match equity calculator
- Thorpe count
- Hit count
- Online help

Extensions from Version 1.0

- Annotations are significantly improved. They can now contain up
  to 3795 characters, are automatically wrapped, and can be scrolled.
- Cube and Move annotations are distinguished.
- Some analytic tools (Match equity calculator, Thorpe count, Hit
  count) are included.
- Tab key can be used to go back exactly one move.
- Online help (optional)
- Export of positions to position database.
- Remember a bookmark for the last position on exit.
- Some bug-fixes.

Upward compatibility: PsiFibs 2.0 is upward compatible with PsiFibs 1.0,
that is bgm-files, produced with PsiFibs 1.0 can be read by PsiFibs 2.0,
but not vice versa. The reason is that PsiFibs 2.0 now represents the
match length explicity, and distinguishes between cube annotations and
move annotations. Unannotated txt-files should still import smoothly,
annotated txt-files might cause problems (e.g. when there is an annotation
before the first move).

********************************************************************
                      WARNING AND DISCLAIMER
********************************************************************

Use this software at your own risk. I won't be held responsible
for any damage to you machine/information etc. etc.

********************************************************************

Space requirements
------------------

PsiFibs: approx. 54 K
Bitmaps: approx.  8 K
Matches: approx. 2/3 of the match in rfibs-format (without annotations).
         that is an average 7pt match (around 9K) consumes about 6K
         in the format used by PsiFibs).

To run PsiFibs you need approximately 50 K free space.


Installation
------------

For PsiWin users:

   1. Copy APP\Psifibs.opa to any \APP\ directory.
   2. Copy the directory APP\psibg to the the same
      \APP directory. Note: if you have already installed
      PsiBgm or PsiBgp you need only to copy the help-file
      APP\psibg\psifibs.rsc to the \APP\psibg directory
   3. Copy the directory BGM to any the root on some
      drive of the psion. Note: For a start it suffices
      to copy only .bgm files from BGM to \BGM.

For Rcom users:
   1. In the directory where PFINST.BAT resides invoke
      PFINST drive1: drive2:, where drive1: stands for the
      drive with the program files, and drive2: stands for
      the drive with the match-files (.bgm and .txt).
      For example to install both kinds of files on internal
      drive type:
      PFINST i: i: 

On the Psion 3a (for PsiWin and Rcom users):

   1. From the System screen navigate to the place you wish to
      install the application on the System screen and press
      Psion-I.
   2. Select the drive on which Psifibs.opa resides using the left
      and right cursor keys.
   3. Select Psifibs.opa in the file list using the left and right
      cursor keys.
   4. Press Enter.

The application will install on the screen at the chosen place. An
initially empty filelist will be displayed under the PsiFibs icon.


Operation
---------

If "Mlofun" does not yet appear under the PsiFibs icon,
make a directory \BGM and copy the file mlofun.bgm to this directory.
Now "Mlofun" should show up below the Psifibs icon.
Select this and press Enter.

You see an initial board, an empty move scrolling list, an
empty imput line, a status window, and a "Goto" selector for game and
move. Press Enter to load the first game (this takes a while,
as the whole game is played through to build up the internal
data structures needed for quickly scrolling back and forth
in the game - but when you have deciphered this somewhat lengthy sentence,
the game should be loaded:)

To see Black's first move type Enter. Alternatively, you can type
in your move in to-notation, seperated by Space. For example, to
type in the move 13/11, 13/10, type 10<Space>11<Enter>. This move
is compared with Black's actual move. If your move differs, you
are notified with a beep, otherwise the move is displayed.

Now you are asked to double. Type Enter to see White's actual
cube-decision, "y" if you think it is a double, "n" if you don't think
it is a double.

In the same manner you can browse through the rest of the game.

With cursor up, cursor down you can scroll back and forth,
with psion-cursor up, psion-cursor down you can scroll back and
forth more quickly. Press Enter to stop scrolling and to go to
the selected move

At the end of the game you get a Goto-selector again to choose
some other game.

For a more details, see below:


File Handling:
--------------

Psion-O invokes a file selector for opening another file.
        Alternatively, you can open a file directly from
        the system screen under the PsiFibs Icon by pressing Enter.
        Note however, that when some dialog-box is open for another
        match you will get a message "<File-name> busy".
        In this case, go inside PsiFibs and terminate the dialog by
        pressing Esc.

Psion-Shift-I invokes a file selector for importing a match from a
        text file in rfibs-format (see below for how to get
	such matches). Select a textfile you want
        to import, and give a name for the .bgm file (without
        extension .bgm). 

        By choosing "Yes" (or "Switch") in the dialog
        "Homogenize Black and White?" Player 1 can be made playing as
        Black (or White) throughout the match.

        When importing a file, a parsing error can occur
        (for example by missing lines). In this case, the line-number
        in which the problem occured is displayed, and PsiFibs terminates.
     
Psion-X exits PsiFibs, alternatively you can also terminate PsiFibs from
        the system screen in the usual way.

Usually, files have to be located in directory "\bgm" on some drive
of the Psion 3a. Alternatively, when the link to the PC is activated,
you can select a drive on the PC (for example, REM::C), and use Tab to
invoke a a file-browser for selecting a file on the PC directly. This
may be useful, for example, to import .txt files directly from the
PC.

Files in PsiFibs-format must have the extension ".bgm", files in
rfibs-format must have the extension ".txt". You do not need to type
the extensions explicily.

Browsing:
---------

Psion-G invokes a selector for game-number and move-number. For
        browsing within a game you can also use the cursor keys as
        described above.

Enter goes forward one move.

Tab rolls back one move.

Shift-Cursor-up, Shift-Cursor-down scrolls up and down one move
Psion-Cursor-up, Psion-Cursor-down scrolls up and down eight move

Cursor-up, Cursor-down scrolls in the annotation window

Use any key (e.g. Esc) to terminate scrolling.

Psion-Shift-K switches on/off storing a bookmark for the last position
        when exiting. This may be useful, if you have the match
        files on flash and do not want to alter them.

View:
-----

Psion-L toggles the direction of play.

Psion-B toggles Black and White. 

Psion-Shift-F toggles between displaying annotations after a move
        or cube decision or before. The first is useful for replaying
        annotated games like a quiz, the latter is useful when
        storing positions to the position database.

Psion-Shift-P toggles Pips on and off.

Psion-Shift-S (and alternatively - Ctrl-Menu) changes the size of the
        status-window (to get more space for viewing moves and annotations).

Psion-A allows you to change the size of the annotation window (between
        0 and 7 lines - initially 0 lines).

Playing Options:
----------------

Psion-Shift-C switches off/on the cube (useful when the cube is dead)

Psion-Shift-Q wait for an additional Enter before changing the board
        numbering after a move.


Extras:
-------

Psion-Shift-A displays the About message.

All these options are also available via the menu-key.


Psion-Shift-T calculates a Thorpe-count. This is an adjusted pipcount
        for long or middle races. For more details see e.g. Robertie's
        "Advanced Backgammon".

Psion-H count the number of hits.

Psion-M calculates match equities on the basis of the table by Kit Woolsey.
        Win percentages give the overall win expectation, gammon
        percentages give the gammons. It suffices to give the
        win percentage for only one player. Double points and take points
        are calculated by adjusting the given gammon percentages
        proportionally.

Entering Moves:
---------------

Moves are entered in to-notation, separated by Space. That is,
you just have to type in the points where you want the checkers
to land. You can leave out interim points. Sometimes there exists
more than one move for your input, for example when a you can hit or
not hit on an interim point. In this case you get a selection
list from which you can choose your move. As long as you have not pressed
Enter you can take back your move.


Matches in rfibs-format:
------------------------

Matches in rfibs-format may be either obtained from
Mark Damish's BigBrother database (available
at ftp.cybercom.net in /pub/users/damish/backgammon/bg-matches),
or recorded on fibs and post-processed with rfibs
(available for Dos and Unix - see Mark Damish's
FAQs at http://www.cybercom.net/~damish/backgammon/bg-faq.html
for where to obtain these).
For further links please look occasionally at
http://este.darmstadt.gmd.de:5000/persons/fankhaus/psifibs.html


Limitations:
------------

Games can not be longer than 149 (half)-moves. Usually this suffices
(I did not encounter any longer game in the testing phase). If a game
exceeds this limit, PsiFibs terminates with some error (but nothing
grave happens to the rest of your Psion:).

Annotations can contain up to 3795 characters.

You can not edit games or annotations (to do this you need
PsiBgm - the shareware version of PsiFibs with game recording
and a text editor for annotations).

Games are not tested for consistency when importing them.
For example, if some line is missing, this can lead to an inconsistent
board in the course of playing it through. You are warned when loading
such a game, but you can still play it through and watch how checkers
appear out of nowhere in those moves:)

Being the happy owner of Psion 3a with 2MB RAM, I still have lots of
spare memory, so I didn't test the file-management with external
memory cards. However, I tried to follow the programming guide lines
and did a bit of testing on the 3a emulator, such that this should
not be a problem.

Bug reports are welcome, though I can not guarantuee swift response.


Legal status
------------

This program (refering to the archive file "psifib2.zip") is FREEWARE.
(well almost - if you are associated to Gammon Press and want to use it,
it costs you a free subscription to Inside Backgammon
(Sorry folks, I just couldn't resist:)).

Freeware means it may be distributed by any means except those explicitly
stated by the author in this document.

The program MAY NOT BE DE-COMPILED OR IN ANY WAY ALTERED. 

It may be distributed on commercial or non-commercial electronic 
networks (such as CiX, Compuserve, and also the Internet), and 
through libraries (such as 3-Lib) on floppy disk only, 
although no charge apart from distribution costs should be made. 

It should only be distributed in the form of the single archive file
"psifib2.zip".

There is no implied or stated guarantee as to the programs worthiness
in performing the stated functions and the author will not be held 
responsible for the results of any defects in the program.

Acknowledgements:
-----------------

PsiFibs is my way to express my gratitude to Andreas Schneider who
developed and maintains FIBS, to Mark Damish who runs
BigBrother, a recording program for matches played between the
top 150 players on FIBS, to Harald Wittmann who developed
the neural-net based program mloner, that can be played on FIBS,
and to lots of others providing indepth discussion of bg on
rec.games.backgammon.

For implementing fully scrollable annotation windows with automatic
word wrap I got invaluable help from Alisdair Manson from the
technical support at Psion Inc./US.

My patient beta-testers provided me with lots of suggestions to
improve PsiFibs.

        








