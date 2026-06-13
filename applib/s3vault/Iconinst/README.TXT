Auto-Installation v1.2          (c) Psion GmbH 1993
----------------------

 Author: G.Tappenden
Address: Psion GmbH, Saalburgstr.157, D-61350 Bad Homburg
Hotline: +49 6172 37097
Mailbox: +49 6172 37090


BACKGROUND TO THE PROGRAM

If you ever have to reset your Series3a, this causes one problem - you have
to re-install all of your applications, groups, and lists.  To overcome this
auto-install allows you to setup your Desktop in a way which suits you, and
following a soft reset you can re-install all of your applications in one
go.

INSTALLATION

The program is saved as a .OPO file, so that it appears under the normal
RunOpl icon.  You should copy the file INSTALL.OPO into the \OPO\ directory
on either an SSD or the internal disk.

THE SCRIPT FILE

In order for Install to know how you like your Desktop, you need to write
a script file, which will be used in the installation process.

This file must be stored in the root directory of the internal disc, and
be called SCRIPT.CFG

The following commands are available in the script language:

HOME
This moves the cursor to the left-most application in the System Screen.

END
This moves the cursor to the right-most application in the System Screen.

LEFT
This moves the cursor one position left.

RIGHT
This moves the cursor one position right.

IN filename.ext drive position
Installs a filename from the given drive at the given position,
eg. IN MONEY.APP A L installs money from drive A: in the position "Last".

JN appname position
Installs a standard application at the given position, eg. JN Word L
installs Word in the position "Last".

REMOVE
Removes the application which is currently under the cursor.

KEY key extra
Assigns a key to the application currently unter the cursor,
eg. KEY A C assigns the key Ctrl+Agenda.

ZOOM+
Increases the zoom factor.

ZOOM-
Decreases the zoom factor.

STATUSWIN
Toggles the status window.

STATUSBAR
Toggles the status bar at the bottom of the screen.

Any line beginning ' is regarded as a comment.


RUNNING THE PROGRAM AFTER A SOFT RESET

After a soft reset, select "Install" under the RunOpl icon.  You will be
asked to confirm that you will to proceed.  On confirmation, the script
file will be processed and the applications installed as specified.


CONDITIONS OF USE

All rights reserved.  Psion accepts no responsibility for any damage caused
by the direct or indirect use of this program.  The software remains the
property of Psion GmbH.


