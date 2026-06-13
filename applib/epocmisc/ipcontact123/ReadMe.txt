                           =========================
                                IPContact v1.23
                           =========================

            ************* WARNING (upgrade from IPC v0.86) **********
            **                                                     **
            **  When you have used IPContact v0.86 read carefully  **
            **  article number 3. 'Upgrade from IPContact v0.86'   **
            **                                                     **
            *********************************************************

Contents
--------
  1. About IPContact
  2. Installation
  3. Upgrade from IPContact v0.86
  4. Using the program
  5. Licence
  6. History


1. About IPContact
------------------
   - IPContact is a 'relational' phonebook manager for EPOC32 systems.
   - You can maintain your database, and associate multiple individuals
     With one firm...


2. Installation
---------------
   The program can be installed one of two ways:

   - In the PSIWIN menu on your PC select '\Psion\Install New Program' 
     and select IPContact.SIS

      OR

   - Copy the 'IPContact.SIS' anywhere onto the Psion, and select it.


3. Upgrade from IPContact v0.86
----------------------------   

      There is a change in IPContact v1.00 in data structure and INI file, 
   therefore it is necessary before installation of a new version completely
   uninstall the old version (except your old data files) and perform data 
   files conversion. You will make uninstallation via Control panel and item
   InstCtrl.
      There is program IPCnvrt for data files conversion (install it from 
   IPCnvrt.SIS). After all data files conversion you can uninstall IPCnvrt. 

4. Using the program
--------------------
   - All of the basic tasks are accessible from the menu, and are easy
     enough to use (I HOPE), they don't need additional explanation.
   - USEFULL shortcuts using the keypad:
        Ctrl + arrows         ... changes the view of the active screen
        Ctrl + Shift + arrows ... changes size your active screen
        PgUp, PgDn            ... moves the window
        Tab, Shift + Tab or   ... switches between listings
        Left/Right arrow
        Del                   ... same as ctrl+D
        Up, Down              ... list entry fields up/down or list database 
                                  records forward/back

   - All fields of the database have autosearch (companies and individuals)
   - If you would like to search for an entry, you only need to enter the first 
     letters.
   - You can move/size all windows by pen like in Windows.

    Note: i.e. [23]/30 in window header means "23th record (with ID=23) from 30 
    records total in database". IT IS NOT regular record number. As you will add 
    and delete records, ID may be greater than total number of records 
    (it is because deleted ID's are not distributed again). 
    In case you want to sort your database and re-order identifiers, 
    use Menu > Tools > 'Reindex database' option.	

5. Licence
----------
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation (version 2).

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have a copy of the GNU General Public License in 
    '\Licence\GNU_Eng.txt'.

6. History
----------
* version 1.23, 28.05.1999
   - German version created (thanks to Peter Smolak)
   - new keyboard shortcuts added - active window can be changed also by using
     left and right arrow
   - minor change of header of some dialogs

* version 1.22, 19.05.1999
   - speed of searching in DB was INCREASED by using function from SysRAM1.OPX
   - backup of database otion added to menu \Tools and to dialog before 
     reindexing
   - zoom in/zoom out option added
   - Arial/Times font option added
   - option for showing found text as Underline/Inverted added

* version 1.16, 23.04.1999
   - menu item added '\Tools\Reindex database' to reorder firms, persons and IDs
   - new choice in '\Tools\Preferences' to Last/First name
   - appearance of export's dialogs changed
   - import, export and print preparing can be canceled now
   - up/down arrow can be used to move next/previous record
   - bug fixed: there is possible use spaces in the name of database now

* version 1.15, 21.03.1999
   - there is a change in database structure (conversion from v0.86 necessary):
        - added another field into address  (Street1)
        - field ICO was extended into 12 characters
        - field DIC was extended into 18 characrers
   - added possibility of export into text file
   - added possibility of import from text file 
   - added possibility of printing 
   - added program setting (direct crossing onto the list during linkage adding
     and DB compression with program ending)
   - French version created (thanks to Jean-Luc Damnet)
   - mistake deleted: during more people assignment there was sometimes wrong person 
     displaied
   - mistake deleted: during setting of a long company name there was an 
     overflow of character variable (only in English version)
   - mistake deleted: contact of an empty window and list made program breakdown

* version 0.86, 28.02.1999
   - the first public release of IPContact

        (c) 1999 Ivo Pastyrik (Program), e-mail: ivo.pastyrik@asys.cz
                 (c) 1999 Vit Novacek (English translation)
               (c) 1999 Jean-Luc Damnet (French translation)
		(c) 1999 Peter Smolak (German translation)
             (c) 1999 Petr Bohdan (Icon, English translation)
