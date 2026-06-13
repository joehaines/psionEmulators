              ********    P - SHELL   RELEASE 2.00 ********
CONTENTS:

(i)     Shareware information and terms of use.
(ii)	Installation of P-Shell.
(iii)	Command supported by P-Shell.
(iv)	P-Shell Auto - Lookup.
(v)     General notes from the author.
(vi)	Version History.
(vii)   Help

****************** IF YOU DON'T READ MUCH ELSE, READ THIS ***********************
*********************************************************************************
*********************************************************************************

NEW EMAIL ADDRESS wilbur@rmcs.cranfield.ac.uk - I read this daily.
                  106077.3544@compuserve.com - once a month?

CIS Website: http://ourworld.compuserve.com/homepages/awilbur/
CIS SWREG Code: 13030

*********************************************************************************
*********************************************************************************
*********************************************************************************

Please note that I *think* that all of the commands will work on the Psion 3c
including the RS (Run Standard) command (which now supports FILES and SOUND)

SHAREWARE INFO and TERMS of USE
Please note that you use this software at your own risk completely. It comes
with no warranty AT ALL and is completely unsupported by Psion PLC. Improper
use will result in the loss of data (Such as responding YES to a DEL *.*
prompt when you meant to respond  NO). The author or Psion PLC shall not be
liable for any loss of data or damage arising from the use of this software.
I have done my best to ensure that this program will not corrupt files or
delete the wrong files, but as stated above I or Psion PLC will not accept
any responsibility for such a loss. Any substantial bugs which are reported
to the address below are gratefully recieved (this also applies to
improvements you would like to see).

P - SHELL is shareware, which means  that if you continue to use it you are
required to register with myself. By registering you will get an 'unlock' code
to get rid of the nag screens which appear at the start and the end of your
P-Shell session. You will also receive future versions FREE and I will also
provide technical support where I can. By registering you may also suggest
improvements which you'd like to see, although I do reserve the right to not
include your suggestions.

There are currently two ways to register:

1) Use Compuserve's SWREG, number 13030. This actually works out as 10.40UKP
so it's about the same as snail mail postal charges and bank charges, etc.
It's also quicker!

                             *** OR THE SLOW WAY ***


1.	Send 10 UK pounds money order or cheque.
				OR
2.	10 UK pounds in cash.
				OR
3.	The equivalent of 12 UK pounds in your own local currency.
				OR
4.	A non-UK pounds cheque to the equivalent of 14 UK pounds.

The additional charges incurred by the last two payment methods are to cover
currency exchange charges.

After sending me payment, I will provide you with a code which will unlock
the NAG screens. This means that all the little annoying pauses will be 
removed, which in tern makes P-Shell much friendlier to use.

Send your chosen payment method to;
Mr. A.  Wilbur,
13, Warminster Road,
Westbury,
Wilts.
BA13 3PA.
UK.

Email 106077.3544@Compuserve.Com or wilbur@rmcs.cranfield.ac.uk (preferred)

Until you register at the start of every P - Shell session, you will have a
SMALL pause until you are able to use the Psion - Shell, and likewise you
will also have periodic reminders throughout your operations using P - Shell,
but this *IS* the full version of P-Shell, and has *NO* features disabled at
all.


INSTALLATION

Files Included:
Readme.Txt      -  This readme File
P-Shell.Opa     -  Main P-Shell File
                   Place this file in the \APP directory on any drive.

P-Calc.Opo      -  The optional mini calculator

P-Hex.Opo       -  Optional P-Shell Hex. Editor
                   Place this file in a \P-Shell directory on
                   the same drive as the MAIN P-Shell.Opa

P-Auto.Bat      -  Auto Configuration File
                   Place this file in a \P-Shell directory on
                   the same drive as the MAIN P-Shell.Opa

P-Shell.Dbf     -  List of Auto - Lookup Commands for P-Shell
                   Place this file in a \P-Shell directory on
                   the same drive as the MAIN P-Shell.Opa

P-Shell.Rsc     -  Help resource file.
                   Place this file in a \P-Shell directory on
                   the same drive as the MAIN P-Shell.Opa


Whichever drive you choose to install P-Shell on, if you want to use some of
the features which make P-Shell so useful, you must also install the
accompaning files (ie. P-Hex.Opo, P-Calc.Opo, P-Shell.Dbf, P-Shell.Rsc,
P-Auto.Bat) in a \P-Shell\ directory, on the SAME drive as the main
P-Shell.Opa.

Eg. P-Shell.Opa in LOC::A:\APP
and the others in LOC::A:\P-SHELL

Use Psion-I on the system screen to install a program, type P-SHELL, and then
press enter. The P-Shell icon should then appear. Position the cursor on the
icon, and press enter.

COMMAND REFERENCE
Please note the following conventions for use during a P-Shell session.
LOC::   -   Specifies a LOCal drive, such as A: B: or M:
ROM::   -   Specifies the Psion ROM.
REM::   -   Specifies a REMote drive

When specifing commands which contain a reference to the Psion's ROM do *NOT*
use R: (to represent ROM:: - you can change onto the Psion's ROM quickly by
typing R:) in your file specification, instead use ROM::, otherwise you will
generate an error message.

1.  DIR    -    Displays a directory listing
                Switches    :s  - subdir. listing
                            :w  - wide format
                            :p  - pause between pages
                Eg.         DIR *.opl :w:p

                Dir can now display files which match a specified size spec.
                or attribute spec.
                Eg. For size spec:-
                DIR *.* >=100 or DIR >=100 or DIR <1000 or DIR <=200

                Eg. For Attributes:-
                DIR =rm (displays only files which have the r & m flag set)
                DIR =rmsh

                Also, to display only files in a directory, enter the filespec
                prefixed by a minus (-) sign.
                Eg. DIR -*.bat DIR -*.*

                Note: Directories are now displayed first, followed by files.
                Also, the format of DIR :S now follows the convention of DIR
                rather than TREE

                **** PLEASE READ THE ONLINE HELP FOR A MORE COMPLETE
                DESCRIPTION ****

2.  CLS     -   Clear the screen
                No switches

3.  CD      -   Change current directory
                No switches 
                Subformats  cd.. (parent)
                cd\xxx or cd\ (to root)
                cd \xxx
                cd. - Displays current directory.
                cd Opl

4.  ATTRIB  -   Change file attributes
                Switches    (+/-)rshm
                Eg. syntax  - attrib clock.opl  +r+s+m
                            - attrib clock.opl  -r+s-h
                            - attrib clock.opl  -m
                            - attrib *.*     -m
                            Note:   r = Read only
                            s = System (won't appear on system screen)
                            h = Hide file
                            m = Modifiable
                            + = Add Attribute
                            - = Subtract Attribute

6.  TREE    -   Display's a listing of a branch much like the TREE function
                in MS-DOS
                Switches    :F  Default is to display directories only,
                                but by adding :F forces P - SHELL to display
                                files as well.

                Press any key to pause a screen or escape a few times to stop
                tree listing

7.  TIME     -  Sets the system clock
                No switches

8.  DATE     -  Sets the system date
                No switches

9.  COPY     -  Copies a file or files from A to B 
    Eg.         COPY    \    \OPL     - will copy \*.* to \OPL\
                COPY \*.TXT \WRD\     - will copy \*.TXT to \WRD\
                COPY \OPL      \      - will copy \OPL\*.* to \
                COPY OPL TEMP         - will copy OPL\*.* to TEMP\
                COPY M:\OPL  A:\TEMP  - will copy M:\OPL\*.* to A:\TEMP\

                Overwrite warnings will be shown.

10. MOVE    -   Moves a file or files from A to B. The same syntax is used
                as for COPY. The only difference in this command is that
                files are DELeted after they have been copied. If there is an
                error in copying the file(s) to their new destination, this
                error will be reported, and the file will not be moved.

                Overwrite warnings will be shown.

11. RD      -   Removes a directory previously created with MD. Cannot remove
                a non empty directory. 
    Eg.     -   RD \TEMP
                RD TEMP
                RD \TEMP\SYS

12. MD      -   Creates a directory as specified. If preceeded by a backslash
                ie. "\" it will attempt to create a sub-directory in the root
                directory "\".
                Eg.-    MD \TEMP - make dir. TEMP in the root directory
                        MD TEMP- make dir. TEMP in the current directory
                        MD LOC::M:\TEMP - make dir. on LOC::M:\ TEMP

13. DEL     -   Deletes files as specified. Eg. DEL *.* will delete all files
                in a directory, providing they are not marked read only, etc.
                You will be prompted for confirmation on typing DEL *.* 

                Eg. DEL *.TXT  - will attempt to delete all .TXT file in
                the current directory.
                DEL \*.* - will attempt to delete all file in the root
                directory.

                DEL can (like DIR) delete files matching a specified size
                or attribute specification.
                Eg. DEL *.OPL <1000
                Eg. DEL *.* =m
                
                **** PLEASE READ THE ONLINE HELP FOR A MORE COMPLETE
                DESCRIPTION ****

14. P - SHELL has the facility of 15 command line memories, ie. it can
    remember the 15 previously entered commands. Use the UP and DOWN keys to
    scroll through the memories.

16.	The Psion key and Right will move editing to the end of a line.
17.	The Psion key and Left will move editing to the start of a line.
18.	The Psion key and Delete will ZAP anything left of the cursor.
19.     SHIFT and ENTER will scroll through matching entries in P-Shell.Dbf.
        ALSO Psion and I switched overwrite/insert on and off.

20. EXIT    - Will end your P - SHELL session.
21. SYSINFO - Will display information about the current machine, and also
              a specified drive.
              No switches supported
              Eg. SYSINFO (to get info about LOC::A:\ LOC::B:\ and LOC::M:\)
              Eg. SYSINFO REM::C:\ (to get info about REMote drive C:\)

22. REN  -  Will attempt to rename A to B. If B already exists an error will
            be reported. FULL support for wildcards.

            Also can be used to rename a directory!

23. Note -  This program supports devices connected to the LOCAL node, ie.
            LOC::M:\ and ROM:: In order to change drives you must
            type the drive name followed by a colon. Eg. A: or B: or C: (for
            the 3-Link pod or ranger disks) or M: or ROM:: or R:  or REMote
            drives Eg. REM::C:\ REM::D:\ REM::A:\ I have not tested any of
            the above commands on any drive other than M: (the internal disk)
            and R: (ROM), as I don't own a SSD.

24. FIND    -   Find a file on the current drive
                Eg. FIND *.OPL

                OR FIND - to display a dialog where you can choose to look
                for a file spec, or look for files containing certain text.

25. BATT    -   Shows Battery information.
26. HELP    -   Shows Help pages or specified help page.
27. REGISTER-   Shows registering information.
28. REORDER -   Alpabetically reorder the specified Data File.
                Eg. REORDER P-Shell.Dbf
                Eg. REORDER *.*

29. LIST    -   Display the current contents of the P-Shell.Dbf auto lookup
                file. (if it is currently open)

30. TYPE    -   Type the contents of the specified file.
                Eg. TYPE Readme.Txt

31. ADD     -   Add an entry to the P-Shell.Dbf File. (ie. the auto - lookup
                database file). Any new entries will then be inserted onto
                the P-Shell command line if a match is found (See below for
                more on the auto lookup file).

32. CONFIG  -   Configures P-Shell, by loading P-Shell.Cfg into the Data Base
                Editor, so you can easily edit your preferences.

33. HEX     -   Loads the specified file into P-Hex.Opo providing you have
                specified the location in P-Shell.Cfg (the config. file).
                If you haven't, type CONFIG at the command prompt to edit
                the config. file.

34. EDITAUTO -  Loads the P-Shell.Dbf into the built in Database program for
                editing of the auto-lookup commands.

35. VER      -  Shows current P-Shell Version and my EMAIL address.
36. PROCESS  or PROCESS <spec>
             -  Without a specification will List all processes
                otherwise will list any processes matching the spec.
                Eg. PROCESS - all
                    PROCESS SYS* - only processes matching with SYS*

37. DIAL <x> -  Dial the specified phone number
                Eg. DIAL 00000 123456
38. KILL <x> -  Will kill the named process.
                Eg. KILL WORD.$08
39. RESUME <x> -Bring the process named <x> to the foreground.
                Eg. RESUME WORD.$08
                    RESUME TIME.$06
40. DELTREE <x> Delete the branch of a diskdrive <x>. This will work on LOCal
                or REMote drives.

                Eg. DELTREE \Temp

                Typing the above will attempt to delete all the files and
                subdirectories within \Temp.
41. ZAP <x>     This command is not dissimilar to DELTREE, but the difference
                is that it does not attempt to delete sub-directories. It is
                useful for removing the contents of directory and the named
                directory all in one go. Obviously, if a directory contains
                a sub-directory an error will be raised.

42. CALC        Load the Mini-Calculator. This only takes up another 1.5K of
                SSD space, and may often be useful for performing those 'on
                the spot' calculations. To enter a calculation, press the
                appropriate key on the psion keyboard. The keypress will be
                simulated on the screen (ie. the key will depress). The key
                value will then appear in the calculator window. Press enter
                to evaluate an expression. If you wish to use the answer to
                an expression as the basis of a new expression, when the
                answer is displayed (eg. =10 where an '=' sign specifies an
                answer) press the symbol to append the the end of the answer
                and the equals sign will be removed, with the previous answer
                at the start of the line.

43. LINKON      Switch the 3-Link on (where available)
44. LINKOFF     Switch the 3-Link off (where available)
45. FCOMP <x>   Compare the contents of the current directory with another
                specified directory.
46. FVIEW <x>   View an example of a specified font(s). Supports wildcards.
47. PLAY <x>    Play the WVE file specified. Does NOT support wildcards.
48. VIEW <x>    View the PIC file specified. Supports wildcards.
49. FTYPE <x>   Find a files origin. Supports wildcards.
50. COMPRESS <x>Compress the specified database file. Supports wildcards.
51. RS          Run a Standard Built in APP, Eg. WORD, DATA, Etc.
52. FILES       Display information about open files or your Psion.
53. ASCII       Display an ASCII extended character table. Also can be
                accessed by Psion - A
                Use the cursor to navigate. Also Psion & left/right/up/down
                move the cursor position to the corners. Pressing a key will
                also move the cursor to the position of that key in the ASCII
                window.
54. BATCH FILES This is a new feature in version 1.70 of P-Shell. Batch files
                allow the user to execute repeated commands, just by executing
                one file. All of the commands specified above, can be used
                in batch files, as well as two new commands, ECHO and PAUSE.
                ECHO <x> Will display <x> to the screen.
                PAUSE Will prompt the user to press a key.

                As well as these two new commands, there are a number of
                special SET commands which may also be used.

                SET CACHE=<x> This command is used to set the default value of
                the P-Shell cache (in bytes). This command will only take effect
                on the initialisation of P-Shell. It will have no effect if
                implemented in any file, other than P-AUTO.BAT
                SET FONT=<x> A font between 1 and 13
                SET INSERT=ON or OFF
                SET LINK=ON or OFF
                SET FSELECT=ON or OFF. This is new in 1.70. It
                allows the user to choose a file to pass to an OPA or APP, when
                trying to execute one of these files. Without it the user will not
                be prompted to select a file by default.
                SET PATH=<x> This is a new feature in 1.70. It allows the user
                to set 5 paths (each must be sparated by a semi-colon (;)) which
                will be searched when trying to execute a file, if the file does
                not exist in the current directory.
                Eg. SET PATH=LOC::M:\APP;M:\OPO;A:\APP\;LOC::B:\OPO\

                SET ECHO=ON or OFF. New in 1.70. Allows the user to choose
                whether commands are displayed to the screen, before they are
                executed. Only works in BATCH FILES.

                SET STARTUP=<x> The startup location of P-Shell.

55. Run OPA's, OPO's, APP's and BAT's
                New in P-Shell 1.60 and above is the ability to run all of
                these different file types. To run a file, just type its
                name (with or without its extension), and the file will run.
                If the file is an OPA or APP it is also possible to pass a
                filename to OPEN or CREATE in the end executable. If the file
                which is passed in the second arguement does not already exist
                it is assumed that the file is to be created. Of course this
                will only be effective if the end executable actually supports
                this feature.

                Eg. To run DRAW28 and open LOC::M:\PIC\ICON.PIC. type the
                following at the command prompt;

                DRAW28 M:\PIC\ICON.PIC

                If the file does NOT exist, the file will be created in DRAW28.
                A NEW feature in version 1.70 are Batch files. These allow the
                user to store lists of commands to execute one after another, in
                one file. To run a batch file, again just type it's name.

                Eg. To run BACKUP.BAT in LOC::M:\ type;
                BACKUP or BACKUP.BAT

                If the executable you wish to run does not exist in the current
                directory, then the path specified in your P-AUTO.BAT file is
                searched for the executable. If it is NOT found, an error is
                reported. Applications which support the passing of parameters
                can also be run using the path searching mechanism, again
                just by typing the name of the APP/OPA followed by the file
                to OPEN/CREATE.
56. AUTO DIRECTORY CHANGING
                P-Shell 1.70 now supports Auto Directory Changing. This means
                that you can change into a subdirectory, by just typing its
                name. Eg. when in LOC::M:\ to change into LOC::M:\APP\ just
                type the following;

                APP

                Your path will now be set to LOC::M:\APP\
57. SUPPORT FOR A FORWARD SLASH
                P-Shell is now able to use a forward slash (/) as well as a
                backslash when entering a filespec.

                Eg. CD/APP is the same as CD\APP or CD \APP\ or CD /APP\

                *** BUT *** In order to implement this, the syntax of DIR and
                TREE has changed. To specify switches for these two command
                the switch initiator is now a colon (:) Eg.

                DIR :W (for wide) or DIR :W:P (wide and pause) or DIR :S (sub)
                TREE :F (display files)
58. FONT or FONT <x> This command is not new but I kept forgetting to include
                it in the readme & help file! Sorry. You can choose which
                font is used within P-Shell. Orriginally you could only have
                two fonts, but now there is provision for 13, where x is a
                number between 1 and 13
59. SOUND       Will allow you to alter the Psion's sound settings.
                Type SOUND at the command prompt.
60. PRINTER     Will allow you to alter the Psion's printer settings
61. BACK        Will send P-Shell to the background. Use CTRL and P to
                bring P-Shell back to the foreground
62. []          Will evaluate the expression in square brackets and print the
                result to the screen.
63. ENV         Will display misc. environment information.
64. OFF <x>     Will switch the Psion off/off for <x> seconds.
65. DEVLIST     Will display a list of all connected devices, and information
                about them.
66. LABEL <x>   Will display the volume label of the specified drive (if <x>
                is given) otherwise will display the label of the current
                drive.
67. SIZE <x>    Will display the total size of the specified drive (if <x>
                is given) otherwise will display the size of the current
                drive.
68. FREE <x>    Will display the free space on the specified drive (if <x>
                is given) otherwise will display the free space on the current
                drive.
69. Auto Editing of Files
                If a filename is entered at the command prompt, and it's
                extension is recognised, then it will be loaded into the
                respective internal APP. Eg. If README.WRD is entered at the
                command prompt, and the file README.WRD exists in the current
                directory, it will be loaded into WORD. If the extension
                is not recognised it will be loaded into a plain text editor
                by defualt.
70. PRINT<X>    Where <X> is F/S/P for File/Serial/Parallel
                Enter PRINTF <filename> to print <filename> to another file
                Enter PRINTS <filename> to print <filename> to a serial device
                Enter PRINTP <filename> to print <filename> to a par'll device

                Where PRINTF is specified, you will be promted to enter the
                output name (ie the file to print TO)

Please note that more descriptive command syntax can be found in the online
help for most of the above commands. I have now written so many commands that
it is just not sensible to duplicate the full syntax in two files. It takes
ages! :-)

USING P-SHELL AUTO-LOOKUP

In order to save you time when you are navigating through directories on your
Psion, I have included that I feel adds a substantial Extra above other
similar Command Shell programs available for the Psion. This feature, allows
the user to customise the quick entry of commands supported by P-Shell. For
example, if you frequently want to change into a directory on your Psion,
called LOC::M\OPL\SYSTEM\CLOCKFNC, that's quite a lot to type in, so in order
to simplify this use P-Shell's ADD function in the following manner;

		Don't type the < and >, they are just showing you what to type.

				<ADD CD LOC::M\OPL\SYSTEM\CLOCKFNC>

		Next Reorder the P-Shell Data Base file (P-Shell.Dbf)

				<REORDER M:\DAT\P-Shell.Dbf>

Now type in <CD LO>, and providing you don't have another entry starting <CD
LO> in the data base P-Shell will automatically add the rest of the line onto
the command prompt for you. If you didn't want to use the automatically
inserted line, just carry on typing, and anything to the right of the cursor
(providing you don't have insert swiched on) will be over-written.

If you just wan't to scroll through entries use SHIFT and ENTER on the
command line, and P-Shell will scroll through matching entries in the auto -
lookup data base.

GENERAL NOTES FROM THE AUTHOR

P-Shell was written entirely by myself, and has take a considerable amount of
my time up over the last few months. Originally, I wrote P-Shell about a year
ago (or at least a crude version with support for only a few commands). Since
then I have considerably improved the interface and overall  functionality of
the program, via the use of the excellent PSIONICS file, kindly written and
made publically available by Clive Feather. This is my first major outing in
OPL so please be patient with any updates that will be forthcoming. As
mentioned at the start of this document, I have tested it quite substantially,
but I am sure there will be things that need small adjustments making here and
there. The main reason for this is that I don't own a SSD at the present time,
and so any testing regarding commands on the A and B drives will be initally
done by you the user. I would strongly suggest you attempt to copy a few files
onto a temporary directory on drive A or B and try out some of the commands on
these backup files first, before letting yourself loose on important files.
Please feel free to contact me via Email or Snail-Mail at the above addresses,
with any problems you may be having, and also any command suggestions etc.

Anyway, all the best with using P-Shell!

NOTE:: Since I wrote the above I have had confirmation that P-Shell works
on SSD's

VERSION HISTORY
Version 0.95 (Beta) -   Originally sent to Steve Litchfield - Not released as
                        it wasn't finished.
Version 1.00        -   Released 11/08/1996
Version 1.10        -   Released 26/08/1996
                        Added VER command
                        Added REMote drive support
                        Added Psion-Like help screens
                        Altered MEM command
                        Altered Installation method, ie all files in 1 dir.
                        Fixed Tree bug when running on emulator.
Version 1.20        -   Released 27/08/96
                        Added registration code entry system
                        Added Overwrite warnings for COPY and MOVE
                        Fixed Minor bugs

Version 1.30        -   Released 28/08/96
                        Fixed some bugs eg. REMote drive access
                        Inserted code for auto-install of remote link
                        (where possible)
                        Altered VER command (again)
                        Altered SYSINFO command so that info can be found
                        for a specified drive (including REMotes)

Version 1.40        -   Added CALC (mini-calculator)
                        Added DIAL (phone number dialing)
                        Added KILL (process killer)
                        Added extra syntax for PROCESS
                        Altered VER command (yet again)
                        Fixed bug in registering system


Version 1.50        -   Fixed bug for DIR on ROM::
                        Fixed (very) minor VER bug
                        Added RESUME to bring a process to the foreground
                        Added DELTREE to delete a branch of a directory
                        (Ie. remove all files and dirs within sub-dir)
                        Added ZAP to delete a files in a specified dir,
                        and then remove the dir (if it doesn't contain
                        any sub-dirs.)

Version 1.51	     -	A minor update.
                        No new commands added.
                        Fixed bug with application launching
                        Slightly altered MEM command.

Version 1.60         -  Added PLAY command - to play wve's
                        Added VIEW command - to view pic's
                        Added FCOMP command - to compare the contents of
                        2 dirs.
                        Added COMPRESS command - to compress data files.
                        Added FTYPE command - to get a files origin.
                        Added FVIEW command - to view an example of a specified
                        font example.
                        Slightly altered the MEM command (again!)
                        Added 3-Link checking on startup
                        Added LINKON command - to switch link on
                        Added LINKOFF command - to switch link off

Version 1.70         -  Added FILES command to display open files on the Psion
                        Added ECHO
                        Added PAUSE
                        Added ASCII command to view ASCII table.
                        Added RS command to run a built in APP, Eg. WORD, DATA
                        Improved DIR command: Now *A LOT* faster.
                        Fixed a few bugs.
Version 1.71         -  Fixed small bugs in VIEW.
                        Altered TYPE command. Now slightly improved
Version 1.72         -  Improved external OPA and APP running to include the
                        correct directory and extension, as specified in the
                        APP..ENDA clause in OPL
Version 1.80         -  Improved FIND command to search for files and files
                        containing certain text.
                        Improved FONT command so that any internal font can
                        be selected as the default font, likewise updated the
                        SET FONT command for P-Auto.BAT.
                        Improved ASCII command so that Psion & left/right/up
                        and down, move to corners of screen. Also pressing
                        a key moves the cursor to the position of that char.
                        in the ASCII window.
                        Added the provision for full screen height in the PC
                        emulator.
                        Improved the REN command so that directories can be
                        renamed without moving/deleting/re-creating them.
                        Also added support for FULL wildcards with REN
                        Sorted out a problem with the HELP file causing
                        P-Shell to crash.
                        Added my new EMAIL address (wilbur@rmcs.cranfield.ac.uk)
                        Changed NAG screen.
                        Other misc. things... :-))))
Version 1.90            Not really released, as ver. 2.00 was quite a major
                        update, so I decided to give it a new overall version
                        number!
Version 2.00            Lot's of new stuff here:
                        PRINTF/PRINTS/PRINTP - Print to File/Serial/Parallel
                        PRINTER - Change printer settings
                        BACK or * - Send to the background (CTRL P to bring to
                        foreground)
                        PROCESS improved
                        SYSINFO improved
                        ENV to display environment info.
                        Auto editing for built in APP's introduced.
                        *MASSIVE* improvements to DIR and DIR :S so that files
                        can be displayed which match sizes/attributes
                        Also directories are displayed first, then files. If
                        you don't want to display directories, then prefix
                        the spec. with a minus sign (-)
                        OFF to switch the Psion of for a specified time
                        DEVLIST to display a list of connected devices and
                        misc info about them.
                        LABEL to display volume labels of drives.
                        SIZE to display capacity of a drive.
                        FREE to display free space on a drive.
                        
HELP & ASSISTANCE
I greatly appreciated the assistance of Phil Spencer with all BETA testing
and also for assisting me with the Registration Code Entry system, plus his
many suggestions.
*MANY* thanks to Eberhard Rompf for his many command suggestions, and testing.
Thanks to Clive Feather for making his Psionics file available.
Also Steve Litchfield for his review, and suggestions.
