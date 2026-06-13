Introduction
------------

Welcome to StartUp!

StartUp is a utility to launch an unlimited number of
programs and files in batch mode.

I've written this program because I make a backup of my
Series 5 nearly every day. I did not understand why PsiWin
2.1 was capable to close open files before a backup but
unable to reopen all of them afterwards. StartUp has been
implemented as a workaround.

Use StartUp to launch your preferred set of programs and
files after a backup, soft reset or whatever. You may define
as many StartUp sets as you like - e.g. for business and
private life.

StartUp makes your EPOC machine work hard for you and it's
fascinating to watch. - Enjoy!


Release Information
-------------------

V2.06 Production - December, 29 1999

What's new?

- Purple Software's PowerBase files are fully supported now. 
  The bug which caused a 'not found' message in prior 
  releases is fixed.
  
- When launching StartUp the first time from the Extras bar, 
  StartUp tries to create a file named 'StartUp' in the 
  default folder (usually C:\Documents). In case a file or 
  folder named 'StartUp' already exists StartUp does not 
  raises an error anymore. Instead a file named 'StartUp(01)
  ' will be created.
  
- The default alias name for OPL OPO files does contain the 
  program name now. Example: The default alias name for a 
  file named MyProg.opo is 'OPL - MyProg.opo' instead of 
  simply 'OPL'.


Compatibility to prior releases

- This release is fully compatible to prior releases

- The StartUp file structure has not been changed

- StartUp checks the version field on opening a StartUp file
  and will automatically migrate the data to the new data
  structure if necessary. However, it is strongly
  recommended to backup your data first, just in case...

- The global preferences defined in the .ini file will be 
  set to default values during the upgrade. Please use 
  the appropriate functions for the necessary changes.


Feature list

- Capture running programs and open files from system task
  list to be stored in your StartUp file
  
- Capture programs and/or files in a directory tree

- Launch programs and files

- Auto execution on opening according to file preferences

- Close open files

- Close open files which are not defined in the current
  StartUp file
  
- Three different views

- Five sort options

- Alias names for StartUp entries

- Navigate to entries by pressing first letter

- Global and file properties

- Customizable hotkey to switch to StartUp

- Unlimited number of StartUp sets

- Unlimited number of StartUp entries

- Runs on

    - EPOC Release 3 (e.g. Series 5)
    - EPOC Release 5 (e.g. Series 5mx)
    - EPOC Emulator Release 5 (WINS)
   
- Available in German, English, French and Brazilian 
  Portuguese
   

Installation
------------

StartUp is distributed as .zip file containing the following
.sis installation file:

- StartUp.sis          for EPOC Computer (e.g. Serie 5)
- StartUpEmulator.sis  for EPOC Emulator R5 on a PC

To install StartUp from your 32-bit MS-Windows PC, double
click on the .sis file and follow the instructions.

To install StartUP from your EPOC machine or the EPOC
Emulator, launch the .sis file and follow the 
instructions.

After the installation a new icon should appear in your
extras bar.

Grap a copy of the appropriate installation program from my
web site http://salvis.com/ when your PsiWin release or
your EPOC machine does not support .sis files.

Important: The old EPOC Emulator Release 3 is not supported 
*********  anymore. Please download the new Emulator from 
           http://www.epocworld.com/ - the OPL or JAVA SDK
           are relatively 'small' and include a full working
           Emulator. They may be downloaded for free by
           EpocWorld members and the membership is free too.
           The new emulator is definitely better than the
           old one and is highly recommended!


Translation
-----------

The following persons have translated StartUp for free.
Thank you very much!

  - Gérald Aubard - French. 
    Visit Gérald's web site at http://www.psionist.com/
    
  - Matthias Neisser - Brazilian Portuguese. 
    Visit Matthias' web site at
    http://www.geocities.com/SiliconValley/Platform/3613/
    
The German and English variants are maintained by myself.


Feedback
--------

Do you have bug reports, enhancement requests or comments?

Your Feedback is very appreciated.

Thank you!

Philipp Salvisberg

e-mail: philipp@salvis.com
www:    http://salvis.com/
