               Uggie's Garden V1.02 for the Psion 3a/c (Only)
               ~~~~~~~By Ian Webster, (C) October 1996~~~~~~~
                      ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
                  ** See end of document for competition!*

How to install Uggie's Garden
=============================

To install, copy UGGIE.OPA and UGG_ED.OPA to the \APP directory on any
drive (SSD's are file). On the same drive make a directory \APP\UGGIE
and copy the following files into it:

                  EDIT.DAT         (Level editor resource)
                  REGISTER.DAT     (Registration details)
                  UGG_BDR.PIC      (Main game screen)
                  UGG_SPR.PIC      (Game sprites)
                  UG_TITLE.PIC     (Title screen)
                  *.WVE            (Sounds)

In addition make a \APP\UGGIE\LEVELS directory and copy the following
file into it:
                  UGG_SCR.LEV      (Level definitions)

To add icons to the system screen, press PSION-I on the system screen
and select UGGIE using the cursor keys and ENTER. Repeat this step for
the UGG_ED application.

Note: The entire game requires around 90Kb of disk space. If you are
      short on space the following files may be deleted with the
      noted effect:

                  *.WVE           No sounds in game (19Kb)
                  EDIT.DAT        No level editor.  ( 5Kb)
                  UGG_ED.OPA      No level editor.  ( 7Kb)

The Story
=========

Uggie has tended his garden well for many years but now the evil
landlord has put his rent up! To pay for this you must run around
the extensive garden and collect all the apples to sell at the
market.


Playing Uggie's Garden
======================

After selecting the game icon from the system screen the title page
will appear giving details of the registered user (pleeeeese register!)
and the user who created the current levels. Press <MENU> for the
following options:

GAME MENU
---------
    NEW       Start a new game from level 1.
    PASSWORD  Enter a password to start at a more advanced level.
    SOUND     Toggles the in-game sounds on/off.
    EXIT      Quit the game and return to the system screen.

HELP MENU
---------
    HELP      Details of game keys and objectives.
    ABOUT     The release version of the game.
    REGISTER  Displays the registered user if registered otherwise
              shows how to register and allows your name and
              registration code to be entered.

EDIT MENU
---------
    LOAD      Allows levels created using the level editor to be
              loaded for play. This is only available to a
              registered user.



           The Characters (Please excuse the bad ASCII art!)
           =================================================

    O__O     UGGIE - That's you, that is.
   (_\/_)
    o  o
     
     /       APPLES - You must collect all of these to complete
    X X               the level.
   XXXXX
    X X

    XXX      TRESS - Use the AXE to chop these down to release
   XX|XX             more apples.
    _|_

     /\      AXE - Chops down trees.
     \/\    
     /\/
    /

    ( )      BOULDERS - These will fall if unsupported and kill
   (   )                uggie. They will also roll off curved
    ( )                 surfaces such as apples and other boulders.

   XXXXXX    WALLS - Block your way.
   XOOOOX
   XXXXXX

   XX        WALLS (Curved) - Rocks will fall off these.
   XXXX
   XXXXXX

   .. . .    EARTH - Uggie can dig through this.
   . . ..
   . ....

    ....     CURVED EARTH - As Earth but boulders will roll off.
   .. . .
   . . ..

    ----     DOOR - A KEY will open this.
   |XXXX|
   |XXXX|

    -        KEY - Opens a door - only one can be carried at a
   | |--,,         time. If you are carrying a key you will not
    -              be allowed to walk over another. If a key is
                   held at the end of a level, it WILL be carried
                   over to the next.

  _O  O_
    \/       PLANTS - These active critters will eat Uggie alive
   _||_               but can be killed with the weedkiller.


  |-XXXX-    WEEDKILLER - Kills all known Uggie eating plants.


   ####      MUSHROOMS - Fatal to the touch!
  ######
    /\


                             THE PLAYING SCREEN
                             ==================

 +----------------------------------------------------------------------+
 |         G  I E             +-------------------------------+ +-----+ |
 |       G        '           | PROGRESS BAR                  | |TIME | |
 |      U  GARDEN  S          +-------------------------------+ |LEFT | |
 |                            +-------------------------------+ |     | |
 |  +-----------------------+ | GAMEPLAY AREA                 | |     | |
 |  |  STATUS AREA          | |                               | |     | |
 |  |                       | |                               | |     | |
 |  |  Score    0000        | |                               | |     | |
 |  |  Lives    3           | +-------------------------------+ +-----+ |
 |  |  Apples   15          | +-------------------------------+ +-----+ |
 |  |                       | |           LEVEL NAME          | |KEY? | |
 |  +-----------------------+ +-------------------------------+ +-----+ |
 +----------------------------------------------------------------------+


                          RULES AND REGULATIONS
                          =====================

 O  To complete a level all apples must be collected, including
    those in trees.
 O  Five lives are given per game.
 O  When a life is lost Uggie returns to the start position.
 O  Once a level is completed a password is shown to allow direct
    access to the new level at the start of a new game.
 O  There is a time limit to complete each level.
 O  Three lives are given per game.

During gameplay the following menu optons are available:
    SUICIDE - Kill yourself if you become trapped.
    SOUND   - Toggles the sound on / off.
    EXIT    - To the menu screen.
    PAUSE   - Pause the timer.

KEYS
    Use the cursor keys to move Uggie around the level.



THE LEVEL EDITOR
================
In the registered version the level editor allows you to create
your own levels to bemuse your friends. Start this from the Icon
on the system screen. If you attempt to access the level editor
when unregistered a message to this effect will appear and send
you back to the system screen.

KEYS
    Cursor keys        - Move cursor around the level.
    PSION up / down    - Scroll the current level up / down.
    PSION left / right - Select the level to be edited.
    < / >              - Select the character to be placed.
    SPACE              - Place a character on the level.
    DELETE             - Deletes the current block.

RULES
 O  Each level MUST contain at least one apple or tree.
 O  The Uggie character denotes the start position. If no start is
    specified Uggie will begin in the top left of the level.
 O  The border must contain solid blocks. The editor will not allow
    you to break this rule.


                                MENU OPTIONS
FILE MENU
---------
    NEW      - Creates a new level file. Specify the 8 character filename
               without an extension (e.g. MYLEVELS). Each new set of 10
               levels requires 17Kb of disk space.
    OPEN     - Opens an existing level file. Once opened you will be
               prompted to enter the edit password if set.
    PASSWORD - Sets the password required to allow levels to be edited.
               Do not forget this password otherwise your levels will
               become inaccessable to the editor!
    EXIT     - Quits the editor.

LEVEL MENU
----------
    OPTIONS  - Sets the access password and name for the level.
               If no password is specified the level will not be
               accessible via the password feature and can only
               be played by completing the previous level.
    ABANDON  - Reverts to the last saved version of the level.
               Note: The current level is saved when another
               is selected or the level editor exited.
    COPY     - Copies the current level to another position.

HELP MENU
---------
    HELP     - Summary of level editor keys.
    ABOUT    - Details of the program.


                            *IMPORTANT NOTES*

When developing levels DO NOT store the level file on a flash SSD as
the large number of file writes will rapidly fill it up! Instead save
the level file on the internal disk whilst editing and then copy it
to the \APP\UGGIE\LEVELS directory on the flash SSD. Use the TAB key
on the file selector box to select another device / directory. Levels
may be played from other directories using the same method during the
load operation.

A level file cannot be loaded into the editor AND game at the same
time. If this is attempted the message "Unable to access level" will
appear.

Level files are freely distributable by any means to other registered
users - lets see who has the most twisted logic! The name of the
registered user is tagged onto each level file and will appear on
the title screen of the game - Your chance for fame!


History
=======

Uggie's Garden was originally created on the Acorn Electron around six
years ago, developing as a colaboration between two A Level students to
try and make a bit of cash. Unfortunately at this point the BBC Games
market went downhill and the project was never published! This conversion
is fairly faithful to the original except the two player mode is missing
as it is a litle difficult to get two players on a S3 keyboard at once!


Registration
============

Due to my previous release, Web World only receiving two registrations
out of over 1000 downloads (Either it was rubbish or everyone likes a
free game!) I am afraid the restrictions on this game are a little
tighter. Only the first five levels are available to the unregistered
version and the level editor cannot be used. This also applies to
levels created by registered users from being loaded.

Registration is only œ10 and is now even easier. Simply send your
payment along with your name and an e-mail address to the following
address and a registration code will be dispatched which will unlock
all the above features:

                         27 Meadow Close
                         London Colney
                         Herts
                         AL2 1RH
                         ENGLAND

If you have any queries or comments I can also be contacted via
e-mail at:
                         100421.1412@compuserve.com

This game has taken 4 months to write, so please support future
developments, REGISTER NOW! (It's costing me a fortune in batteries
to write these things!)

COMPETITION TIME
================
If you e-mail me with the correct answers to the following question you
will receive *FREE* registration (limited to first 5 correct entries).

    Q. What was used to create the sound of the boulders falling
       and the axe chopping?

***************************
** Ian Webster, 21/10/96 **
***************************
