Welcome to SAVEMAIL Macro v2.51 for Epoc Release 3
------------------
To be used with Macro5 (from v2.37) and a
Psion S5 or Geofox + MsgSuite Email (v1.52)

WHAT'S NEW
------------------
v 2.51 * improved the handling of messages stored to a Psion Word file: the
         wbox extension has been eliminated, so any Word file can be selected
         as a target file; now the cursor is always left at the beginning of the
         file when the storing procedure ends; if you select an existing Word
         file, now the macro opens it in background (faster)
       * changed the structure of SaveMail.ini file: the first time the macro is
         run, a dialog box will ask to relaunch the Setup procedure and upgrade
         the .ini file

PLEASE READ CAREFULLY
------------------
SaveMail is a macro that automates the process of copying your messages
from Psion MsgSuite Email to any number of  "preformatted" Psion Data
files (e.g. one for Work and another for Home), or to a "plain" Psion Word
documents, where they can be easily edited, sorted, printed, searched or
simply stored.
Before proceeding, keep in mind the following:
* especially when you choose to store multiple messages, SaveMail
  may take some time to accomplish its task: please note that while the
  macro is running, Macro5 is NOT available for other operations and you
  also should NOT switch to other tasks or applications;
* be also advised NOT to touch the keyboard while SaveMail is running,
  because you could interfere with the scheduled sequence of keystrokes,
  thus leading to some misbehavior of the macro itself (including the
  deletion of all your messages in the worst case, only if the "auto-delete"
  option is selected);
* if a message contains one or more ATTACHMENTS, the macro simply
  ignores them, so be careful to manually save them before deleting the
  original message from MsgSuite;
* use the "auto-delete" option with care, as there is no way of restoring
  any deleted message from Psion Email. I strongly recommend a good
  BACKUP before starting to play with SaveMail...
* I decline responsibility for any loss of data that may result from the
  use of this program.

HOW TO INSTALL AND USE
------------------
1)  Unzip the savem_r3.zip file.
2)  Copy SaveMail.opo, SaveMail.ico  and SaveMail.box to the folder
    where you keep all the macros of Macro5. The SaveMail.box file is
    the "preformatted" Data file used to create new MailBox Data files
    from within the macro. DO NOT use this file to store your messages,
    just leave it empty in your macros folder.
3)  Launch Macro5 and create a new shortcut to SaveMail.opo (remember
    to select "Macro" and not "Program" in the shortcut "Type" definition).
4)  Call SaveMail macro through the associated Macro5 shortcut: the first
    time you launch it, a Setup process will start, showing your System
    settings: location of your Email program, Date format and Macro5
    version (remember you need v2.37 at least, as this macro is not tested
    with previous releases; also note that if you upgrade from v2.37 to 2.40
    or later in a second time, you will have to relaunch this setup): simply
    check if these settings are correct; once passed the first window, a
    second one will ask you for the location of your default Data file (the
    one that will be used by default to store your messages); if you do not
    have previous .box Data files to use, select NEW and a dialog will let
    you create a new Data file and place it wherever you want (the .box
    extension will be automatically added if you forget it); finally a last
    window will ask if you want to enable the choice of different Data files
    (note that you can create multiple Data files also in a second time using
    the NEW option), and  if you want to open a dialog window to automate
    the process of storing (and deleting) a group of messages each time you
    launch SaveMail.
5)  All these settings are stored in a SaveMail.ini file located in the
    "C:\System\" folder; if for any reason you need to change these settings
    (e.g. you changed the date format in the System settings, or upgraded
    Macro5 from v2.37 to v2.40), simply delete the file, and when you will
    recall SaveMail, the setup process will be launched again.
    Alternatively, you can relaunch the Setup procedure from within the
    macro, in the target data file window (if enabled).
6)  Once accomplished the setup process, these are the steps to follow:
    - launch your Email program
    - select the Inbox or the Sent folder
    - highlight a message without opening it (do not press Enter)
    - call SaveMail macro through the associated Macro5 shortcut
7)  According to the options you have selected in the setup process, once
    launched, SaveMail may:
    - ask for the location of your target Data file to store your message(s)
    - ask for the number of messages to automatically store
    - ask if it has to delete the original message(s) from Email
    - ask if the highlighted message has no subject (see known issues)
8)  If everything is OK, once SaveMail has finished its task, you should
    find yourself again in the Email screen, but you should also find the
    message(s) stored in the (selected) target Data file.

UPGRADING
------------------
* If you have v2.10 or later, simply replace SaveMail.opo and SaveMail.box
  with the new ones (all your settings won't be lost).
* If you already have a MailBox Data file from v2.00, simply rename it,
  in order to add the ".box" extension.
* If you need to upgrade from v1.xx just follow the instructions in the
  history.txt file.

NOTES ON THE "PREFORMATTED" MAILBOX DATA FILES
------------------
The "preformatted" DATA files, where the messages from Email are
stored, have the following labels:
Subject  (type: text length: 250)  second sorting key (ascending)
Date     (type: text length: 30)   first sorting key  (descending)
From     (type: text length: 250)
To       (type: text length: 250)
CC/BCC   (type: memo length: unlimited)
Body     (type: memo length: unlimited)

NOTES ON THE  "PLAIN" WORD FILE
------------------
You can save your message(s) to a "plain" Psion Word file; the macro
can create a new Word target file for you, or can use an existing one;
each field of the original message(s) is copied as text, with an appropriate
description (e.g. subject, date etc.); if the target file already contains other
text, the new message will not overwrite it, but will be placed at the
beginning of the document.

NOTES ON THE DATE FORMAT OF THE  MESSAGES
------------------
This macro copies the DATE string of the original message (no matter
which format is selected in your System settings) and changes it to
"yyyy/mm/dd hh:mm:ss" in order to have them correctly sorted by date
in the MailBox file (it works both with 12h and 24h TIME format).

TRANSLATING THE OPL CODE
------------------
Feel free to modify the source code (SaveMail.opl) as you like...
If the translation fails with a "not found" message and the cursor at the
line INCLUDE "Systinfo.oxh", it means that you have to install that file
to your "System\OPL\" folder; feel free to email me at the address below
and I will send you a copy.

KNOWN ISSUES
------------------
* If a message has the TO, CC or BCC fields particularly long (e.g. sent
  to a list of people) only the first 250 characters will be copied to the Data
  file; if this happens, a dialog is displayed to alert you
* The macro can't deal with FAXES: such messages are simply ignored
  and never deleted (even if you selected the "auto-delete" option); a final
  report window will let you know the number of messages not stored
* If a message has NO SUBJECT, you have to tell this to SaveMail, using
  the apposite DIALOG WINDOW (if enabled), otherwise the macro will put
  the content of the first field it finds above the empty subject into the
  subject of the MailBox file and you have to manually rearrange the fields
  once the macro has ended its task...

ACKNOWLEDGMENTS
------------------
A great thank you to:
* Pascal Nicolas for writing Macro5 and helping me in making this macro
  work... Visit his homepage for a huge library of useful macros:
  http://pnicolas.epocboulevard.com/
* Mario Collado (author of Assistant) for all his suggestions!
  Visit his homepage at http://www.psionwelt.net/MarioCollado/

Any comments & suggestions are welcome!

E-mail me at:
  sergioalisi@geocities.com
Visit my homepage at:
  http://www.geocities.com/siliconvalley/bridge/1492

THANKS & ENJOY