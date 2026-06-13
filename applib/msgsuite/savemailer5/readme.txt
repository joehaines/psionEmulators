Welcome to SAVEMAIL  v3.00 for Epoc Release 5
------------------
To be used with the following devices:
- Psion S5mx & S5mx-pro
- Psion S7 & NetBook
- Psion Revo & Revo Plus
- Ericsson MC218
Supported languages (see the language.txt file for new languages):
- UK/US English
- German
- Netherlands
- French
- Italian

WHAT'S NEW
------------------
v 3.00 * SaveMail has become a "real" program (but still uses Macro5 libraries)!
         Easier installation through a standard sis file: now you can launch it
         via the Extras bar, or use it as usual via a Macro5 shortcut
       * added a "right" keystroke to move to the messages window in Email if
         you forget the cursor in the folders window
       * added the support to the German Ericsson MC218
       * fixed a bug when opening the Data target file for the first time
       * if you go on using Macro5, remember to upgrade the shortcut to the new
         program location and to change the shortcut type from "macro" to "program"
       * if you have a previous release installed, you can safely delete the
         SaveMail.ini file in C:\System and all the (4) SaveMail.* files in your
         Macros folder
       * remember to upgrade your Data files with the new Attachment(s) field
         if you come from SaveMail v2.51 or earlier (see UPGRADING below)

PLEASE READ CAREFULLY
------------------
SaveMail is a program that automates the process of copying your messages
from Psion Epoc ER5 built-in Email app to any number of  "preformatted"
Psion Data files (e.g. one for Work and another for Home), or to a "plain"
Psion Word document, where they can be easily edited, sorted, printed,
searched or simply stored.
Very useful to speedup the ER5 Email program, notoriously slowing down
its performances when it has to work with many messages (especially if
stored to a CF Card...): by moving them to any user defined Data file, the
program performances remain high over time!
Before proceeding, keep in mind the following:
* the option "Retain original text when replying" must be selected in order
  to get the right time and date of a message; to enable it, launch your Email
  program, select any received message and open it, then select
  menu - tools - viewer preferences... and tick that option
* especially when you choose to store multiple messages, SaveMail
  may take some time to accomplish its task: please note that while the
  program is running, your device is NOT available for other operations
  and you also should NOT switch to other tasks or applications;
* be also advised NOT to touch the keyboard while SaveMail is running,
  because this could interfere with the scheduled sequence of keystrokes,
  thus leading to some misbehavior of the program itself (including the
  deletion of all your messages in the worst case, only if the "auto-delete"
  option is selected);
* use the "auto-delete" option with care, as there is no way of restoring
  any deleted message from Psion Email. I strongly recommend a good
  BACKUP before starting to play with SaveMail...
* I decline responsibility for any loss of data that may result from the
  use of this program.

HOW TO INSTALL AND USE
------------------
1)  Unzip the savem_r5.zip file.
2)  Launch the SaveMail.sis file and select your destination disk.
3)  Launch the Macr5Lib.sis file and select your destination disk.
4)  SaveMail should appear in your "Extras bar", ready to be used.
5)  If you want, you can use Macro5 to create a shortcut to SaveMail
    (select "Program" in the shortcut "Type" definition).
6)  Call SaveMail through the "Extras bar" or the Macro5 shortcut:
    the first time you launch it, a Setup process will start...
  - letting you select your machine type and/or language (to set the correct
    shortcuts used by the program; if you can't find your model, or the
    program tells you that the selected model/language is still not supported, just
    have a look at the LANGUAGE.TXT file, where you can find all the information
    I need to set-up a new machine type); a second option lets you disable the
    "Date extraction" procedure (by doing so, you can get a faster process, but
    the date field in the target data file will refer to the date and time of
    the storing procedure and not the real one); a final option lets you select
    an appropriate value for the pauses between keystrokes in the program: this
    is to adapt the program speed to different processor clock and different media
    support (Data files and Email messages stored to RAM are much more faster to
    be opened than the ones stored to a CF Card or to an IBM Microdrive); as a
    general rule use lower values if you do not have large Data files or many
    messages in Email, and if you keep them in RAM; increase the values if you
    use CF Cards (increase more with IBM Microdrives) or if you find any error
    messages when running the program;
  - once passed the first window, a second one will ask you for the location of
    your default Data file (the one that will be used by default to store your
    messages); if you do not have any previous .box Data files to use, select
    NEW and a dialog will let you create a new Data file and place it wherever
    you want;
  - a third window will let you choose the folder where the program can store any
    attachment it finds (you can select an existing one or create an new folder);
    an option is given to "Optimize names" for "Card view" (each attachment name on
    a new line of the attachment(s) field in the target Data file) or "List view"
    (a number before each attachment, no line feed between them); a final option
    is given to use full pathnames or not: according to your preferences, the
    Attachment(s) field of the target Data file will contain the name and path
    of any stored attachment (e.g. C:\Documents\Mail Attachments\readme.txt) or
    the name only (e.g. readme.txt); the "Set Filters..." button opens a dialog
    window where you can specify up to 4 attachment names that will be always
    filtered by the program, and so won't be saved to disk (useful for attachments
    like "winmail.dat" or "attachments" that sometimes come with your mail and
    can be safely ignored)
  - finally a last window will ask if you want to enable the choice of different
    Data files (note that you can create multiple Data files also in a second
    time using the NEW option), and  if you want to open a dialog window to automate
    the process of storing (and deleting) a group of messages each time you launch
    SaveMail.
7)  All these settings are stored to a SaveMail.ini file located in the folder
    "\System\Apps\SaveMail"; if for any reason you need to change these settings,
    simply delete the file, and when you will recall SaveMail, the setup
    process will be launched again.
    Alternatively, you can relaunch the Setup procedure from within the
    program, in the target data file window (if enabled).
8)  Once accomplished the setup process, these are the steps to follow:
    - launch your Email program
    - select an Inbox or Sent folder
    - highlight a message without opening it (do not press Enter)
    - call SaveMail through the "Extras bar" or the Macro5 shortcut
9)  According to the options you have selected in the setup process, once
    launched, SaveMail may:
    - ask for the location of your target Data file to store your message(s)
    - ask for the number of messages to automatically store
    - ask if it has to delete the original message(s) from Email
    - ask if it has to close the target file when done
    - ask if it has to ignore the attachments
    - an option is also given, to temporarily change the attachments folder
10) If everything is OK, once SaveMail has finished its task, you should
    find yourself again in the Email screen, but you should also find the
    message(s) stored to the (selected) target Data file.

UPGRADING
------------------
From v2.60 the MailBox Data file structure has changed, to make place to
the new Attachment(s) field: if you create a new target Data file from
the program itself, this file will be of the new type; if you want to update
any previous Data file to the new structure (essential if you want to continue
using them with v2.60), just follow the instructions below:

* open your old MailBox Data file
* select Menu/Tools/Change labels...
* select ADD...
* in the "label" field write: Attachment(s)
* in the "data type" select: TEXT
* put 250 in the No. characters field
* choose OK two times
* select Menu/Tools/Label preferences...
* with the down arrow highlight Attachment(s)
* choose the "Move Up" button
* now you should have the Attachment(s) label positioned above the Body one
* choose OK

NOTES ON THE "PREFORMATTED" MAILBOX DATA FILES
------------------
The "preformatted" DATA files, where the messages from Email are
stored, have the following labels:
Subject        (type: text length: 250)  second sorting key (ascending)
Date           (type: text length: 30)   first sorting key  (descending)
From           (type: text length: 250)
To             (type: text length: 250)
CC/BCC         (type: memo length: unlimited)
Attachment(s)  (type: text length: 250)
Body           (type: memo length: unlimited)

NOTES ON THE  "PLAIN" WORD FILE
------------------
You can save your message(s) to a "plain" Psion Word file; the program
can create a new Word target file for you, or can use an existing one;
each field of the original message(s) is copied as text, with an appropriate
description (e.g. subject, date etc.); if the target file already contains
other messages, the new one will not overwrite them, but will be placed at
the beginning of the document.

NOTES ON THE DATE FORMAT OF THE  MESSAGES
------------------
This program copies the DATE string of the original message (no matter
which format is selected in your System settings) and changes it to
"yyyy/mm/dd hh:mm:ss" in order to have them correctly sorted by date
in the MailBox file (it works both with 12h and 24h TIME format).

KNOWN ISSUES
------------------
* If a message has the TO, CC or BCC fields particularly long (e.g. sent
  to a list of people) only the first 250 characters will be copied to the
  Data file; if this happens, a dialog is displayed to alert you
* The same may happen for the Attachment(s) field
* SaveMail can't deal with FAX messages, which are simply ignored and
  never deleted (the copy and paste process doesn't work with them...)
* SMS messages are copied to the target file, but some fields must be
  manually rearranged at the end of the storing process
* if a SENT message is moved from its original Sent folder, its "From" field
  disappears: the program detects this and leaves that field blank
* if a SENT message is moved from its original Sent folder, the "Reply to"
  option doesn't work, so the date field in the target data file refers to the
  time of storing and is marked with (*); if this happens, a dialog is
  displayed to alert you, and the "auto-delete" option is always disabled
* for the two above reasons, it is always better to use SaveMail with SENT
  messages before moving them from the Sent folder
* for Purple Software Navigator users: when running SaveMail, you have to
  disable the "Return to Navigator" option (general preferences). This will
  let SaveMail cycle between the Email application and the Data target file,
  without Navigator popping out between them, thus locking the program.

ACKNOWLEDGMENTS
------------------
A great thank you to:
* Pascal Nicolas for writing Macro5 and helping me in making this program
  work... Visit his homepage for a huge library of useful macros:
  http://pnicolas.epocboulevard.com/
* Mario Collado (author of Assistant) for all his suggestions!
  Visit his homepage at http://www.psionwelt.net/MarioCollado/
* Corrado Formicola, Jarle Hallingstad and all the beta testers from UPS5 Digest

Any comments & suggestions are welcome!

E-mail me at:
  sergioalisi@geocities.com
Visit my homepage at:
  http://www.geocities.com/siliconvalley/bridge/1492

THANKS & ENJOY
