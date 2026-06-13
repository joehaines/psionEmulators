TOOLS3a v2.1
------------

Author: G.Tappenden  (February & March 1994)

        Psion GmbH                      Tel: +49 6172 9654-0
        Saalburgstraáe 157          Hotline: +49 6172 9654-28
        61350 Bad Homburg               Fax: +49 6172 39192
        Germany                     Mailbox: +49 6172 37090

              cix: mhoelscher    compuserve: 71333,2070

              Internet: mhoelscher@uk.co.compulink.cix

All rights reserved.  Psion accepts no responsibility for any loss or
damage caused by the direct or indirect use of this program.  The software
remains the property of Psion GmbH.

No fee may be charged for this product, without the express permission of
Psion GmbH.  If you copy this product, then you MUST copy all the files
which are included in the original archive.

        *       *       *       *       *       *       *       *

TOOLS3a is a file manager for the Series3a.  It allows up to six windows,
between which you can copy files.  You can also delete, rename (and move),
set the attributes, create directories, and remove directories.

You can also launch a file with an application based on its extension.

You can move and size the windows, sort DBF files, insert fields, and
and Hex Editor is also included.

        *       *       *       *       *       *       *       *

VERSION 2.1
Fixes:  The Hex Editor no longer reports 'String too long' if the address
        is more than four characters.
        The DBF Sort now displays 'Busy' when it is working.
        Files without extensions now work correctly.
        When copying or moving, the default directory to copy/move to
        is NOT the same as the current directory, unless only one window
        is open.
        The Tree option is no longer limited to 200 directories.
        A trap has been added to stop the memory filling up with the
        file lists.
        Parts of the documentation for 2.0 read version 1.2, this has
        been fixed.

New Options:
        Remove Directory now deletes the files within the directory, and
        recursively deletes the sub-directories under it.
        A disk information option has been added.
        Tagging files can now be done using SHIFT-UP and SHIFT-DOWN.
        Directories can now be tagged, copied, and moved.
        A drive selector has been added.
        A file search option has been added.
        
        Disk Information shows "Percentage of files backed up".  This is only
        reliable if backups are made using the "Backup" option in the System
        menu, the -M flag with COPY in MCLINK, or any other method that
        backs up using the Archive flag.


VERSION 2.0
Fixes:
        The 'Menu' key is now active even if CAPS LOCK is on.
        The program does not hang if you close an empty window.
        If a directory does not exist, the window is automatically closed.
        The DBF and HEX sub-routines are now 'Locked' to the System.
        PC Volume names are no longer displayed as files.
        All directory names are now converted into upper case.
        There is a new icon.  Please re-install Tools3a in your system screen.
        The program does not allow ALL the windows to be closed.
        Error Handling has been improved in Hex and DBF tools.
        Setting the attributes for ".." is no longer possible.

New Options:
        A 'refresh' option has been added.
        A 'move' option has been added.
        Files tagging is now supported for copy, move, and delete.
        You may now change the directory displayed in a window by pressing
        PSION-TAB.  This also means that you can change drives.
        A directory tree option has been added.

