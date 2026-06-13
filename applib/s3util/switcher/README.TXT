
                   Task Switcher Version 3.20
                          01-12-1998

               (c) JPh.Reynaud - 1997-1998
			  EMail: JPh@anais.aurecvideo.fr
       

Summary
-------
Task Switcher is an application that runs on your favorite Psion
Series 3, 3a, 3c, 3mx, Siena and Workabout.
It has been developped to react according to the specifications
of the auto detected machine. 
It allows you to switch between running applications faster than
you have ever seen.  When you press the HotKey, a "Windows Like"
task bar is displayed on the screen.
A cursor enables you to select the application you want.
Release the Shift Modifier key, moves the selected application to
the foreground.



SWITCHER.ZIP Files
------------------
1. ReadMe.txt     This document.
2. LisezMoi.txt   This document in French.
3. LiesMich.txt   This document in German.
4. Switcher.App   The application.
5. Switcher.Ini   The configuration file



Installation
------------
If you already installed a previous version of TaskSwitcher 
where it was an IMG application, please back it up and remove
it from the machine before installing this new one. 

Copy SWITCHER.APP to \APP directory of any drive.
Copy SWITCHER.INI to M:\OPD directory.
Install SWITCHER.APP by using Psion-I from the System screen.



Summary of Key
--------------
CTRL-TAB is the default Hot Key to "Awake" the software.

Note: Depending of the "StayOn" parameter (described bellow in 
the INI file section), the application will stay automatically 
on the screen or the Shift modifier key must stay pressed for
the following actions :

- Tab          Select next process icon
- Cursor Keys  Navigate through process icons 
- Enter        Move the selected process to the Foreground
- Psion X      Quit the application
- Psion S      Setup, Launch the text edition of Switcher.INI
- Psion Z      Zoom of the Task description line
- Help         Little help screen



Disclaimer
----------
Neither the Author, nor Psion, can be held responsible for any
loss of data as a result of using Task Switcher.
The information contained in this document is subject to change 
without notice.



Registration
------------
The registration fee is only US$ 10.

You can registered through RegNet. The Registration Network.  
RegNet can be reached on the World Wide Web at the following URL:
        http://www.reg.net
An easiest way is by following the link from my Home page at :
        http://web.avo.fr/jph
TaskSwitcher has been allocated the REGNET Number: 1424 

Note that paying using this method will involve an extra US$ 5 charge

If you prefer, you can send me a cheque (French cheque of 50FRF only),
at the following adresse :

    Jean-Philippe Reynaud
    5, rue Toulouse Lautrec
    42100 St-Etienne
    France

    EMail : jph@anais.aurecvideo.fr
    URL   : http://web.avo.fr/jph

In each case, after confirmation of your registration, I'll send you
a personal registration code to change the demo version as a full
working version. Please note this code will stay valid for all future
versions of TaskSwitcher.

 

INI File configuration
----------------------
The SWITCHER.INI file is a text file which allow you to define some
parameters for the application. It could be edited with the built in
Word application. Be careful to always save it as a "Text Only" file.

Alphabetical list of Task Switcher parameters :

- AwakeOnPower
  Allow the application to be displayed at power-on of the Psion or not.
  Accepted values are 0 or 1. Default is AwakeOnPower=0 (Not diplayed)
  
- HotKey
  Must be the Ascii code of the key wanted to awake TaskSwitcher.
  Accepted value are any Ascii code of the KeyBoard. (see KeyCode values
  section bellow) Default is HotKey=9 (Tab Key)
  
- HotShift
  Must be the code of the Modifier key wanted in combination of the HotKey.
  It must be one of the following :  8 for PSION, 4 for CTRL or 2 for SHIFT
  Default is HotShift=4 (Ctrl Key)
  
- ProcHide
  You can define here some process you don't want to be drawed inside the
  icons list. Syntax is : ProcHide=application-file
  application-file could be complemented by it's path
  Some process are default hidden :
  ProcHide=SYS$RPCS.IMG ; Serial Link
  ProcHide=SYS$IR.IMG   ; Infra Red 
  ProcHide=SYS$TIP.IMG  ; Tip of the Day
  You can add as many as process to hide you want.
  ie : ProcHide=PSI-ON.OPA

- ProcIcon
  This keyword, allow you to define an Icon to process who haven't one.
  Syntax is : ProcIcon=application-file,icon-file
  application-file could be an extention or a proc file eventually
  complemented of it's path.
  icon-file could be an Application or an IMG file as well.
  Some process extention are defined by default :
  ProcIcon=IMG,ROM::RUNIMG.APP
  ProcIcon=OPO,ROM::RUNOPL.APP
  You can add as many as process icon you want.
  ie : ProcIcon=\APP\MYAPP.OPO,\PIC\MYICON.PIC
  
- FileIcon
  This keyword, allow you to define an Icon to user files.
  Syntax is : FileIcon=file-name,icon-file
  file-name could be an extention or a full file-name eventually
  complemented of it's path.
  icon-file could be an Application or an IMG file as well.
  The following file icons are defined as default :
  FileIcon=JOT,ROM::JOTTER.ALS
  FileIcon=OPL,ROM::PROGRAM.ALS
  You can add as many as file icon you want.
  ie : FileIcon=TXT,\PIC\BOOK.PIC
       FileIcon=SECRET.WRD,\PIC\SAFE.PIC

- StayOn
  Allow you to avoid keeping the Shift modifier key pressed when you
  want to navigate thrue running process. With this parameter set to on,
  the application will stay on the screen until you press the Enter key.
  Accepted values are 0 or 1. Default is StayOn=0 (Not staying on)
  
- UseGrey
  Because memory is often not enough inside a Psion, many people prefers
  to disable the grey plane of the icons. This option is not available
  for Psion S3 machine.
  Accepted values are 0 or 1. Default is UseGrey=1 (Grey active)



Keycode Values
--------------
Key-Modifiers values :
 Psion : 8 
 Ctrl  : 4
 Shift : 2

Keyboard keys values :
 Del     : 8    Tab     : 9    Enter   : 13
 Esc     : 27   Menu    : 290  Help    : 291
 Diamond : 292  

 Ctrl-A  to Ctrl-Z   :   1 to  26
 Shift-A to Shift-Z  :  65 to  90
 Psion-A to Psion-Z  : 609 to 634
 
 

History
-------
Ver 1.0 Oct 13 1997
- Task Switcher birth.

Ver 1.1 Oct 16 1997
- On Siena and Workabout machines, the Window of the application was
  shifted to the bottom of the screen.

Ver 1.2 Oct 29 1997
- When using S3 applications the task switcher was not able to run.
  This is now solved. The 24x24 icons are enlarged to 48x48 pixels.
  This version has not been distributed. Only to a few Beta testers.
  Thanks to them for the FeedBack.

Ver 2.0 Oct 30 1997
- Add a Setup application. 
  Details are precised at the top of this document.
- Cosmetics problem one the Siena solved too.

Ver 2.1 nov 06 1997
- Change this txt file name from Switcher.txt to ReadMe.txt
- The 'Display at startup' was permanently activated, 
  even though this option unselected.
- The icon of certain application compatible with S3 and S3a was
  not displayed correctly, even interfering with other icons.

Ver 2.2 nov 07 1997
- With the "Display at Startup" activated, after canceling an Alarm
  or Entering the Psion Password at startup, Task Switcher was not
  clear the screen and redraw itself.
  
Ver 2.3 nov 11 1997
- When running the software the first time, the icons task bar is
displayed now. This is more usefull for new users !
- The application was drawn twice at startup.

Ver 2.31 dec 04 1997
- Add LisezMoi.Txt, this document in French (asked by many people)
- No change in the application itself (still 2.3)

Ver 3.00 jan 11 1998
- TaskSwitcher become an APP file (rather than an IMG).
- Registration method change. A personal code is now used.
- All parameters are now user definable in a INI File (see INI file section)
  this as a replacement of the small setup utility.
- New parameters added :
  - HotKey   Hot Key is now full definable
  - HotShift Modifier Key to (Psion, Ctrl or Shift as well)
  - StayOn   To avoid staying SIHFT key pressed 
  - ProcHide To define process you don't want to see in the list
  - ProcIcon To define an Icon to process who haven't one


Ver 3.10 feb 01 1998
- New parameter added :
  - FileIcon To define an Icon to user files
- Switcher.INI is directly editable by pressing the "S" (Setup) key
- Little Help screen added

Ver 3.20 dec 01 1998
- Change web site URL and EMail adress
- Switcher.INI file can now be stored in any disk. It is searched in
  the following order:   A:\OPD then B:\OPD then M:\OPD

End of Document.