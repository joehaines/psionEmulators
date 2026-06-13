Psion Series 5 Message Suite (1.52)
====================================
UK/US version (Build 055)
October 1998


VERY IMPORTANT. Message Suite is not compatible with the "EasyFax" 
software. If you have a version of EasyFax installed on your Series 5
you must remove this BEFORE installing the Message Suite.

See "What to do if you have EasyFax installed" for instructions.

The Message Suite installation will also add new items to the Series 5
Control Panel (Internet, Modems). You must change settings in the
Control Panel and the Email program before you can use the Message Suite
programs. See the Message Suite User guide for more details.


If you have a previous version of Message Suite...
==================================================
It is best to install the new release without uninstalling the previous
version so that your settings (e.g. email messages, setup and service
provider information) are preserved. These settings will be removed if
you remove your existing version before upgrading.

Note that it is recommended that you back up your Series 5 before
installing additional software or making changes to important settings.


Installing Message Suite using a Win 95/NT PC
=============================================
You must have PsiWin v2.X installed on your PC.

Message Suite, PsiWin 2.1 (and above), and other Psion programs now
include "EPOC Install" which you can use from your PC to install
programs to your Series 5. 

To check if you already have EPOC Install, right click on the "My Psion"
icon on the desktop and note if the "Install New Program" option exists.
If it does not, you should first run SETUP.EXE to install the "EPOC
install" program. 

You can now use the "Install New Program" option or double-click on a
.SIS file in Windows Explorer to install Message Suite, the service
provider templates or other programs on your Series 5. 

The first time you install a program or file using EPOC install, 
an Add/remove icon is added to the Series 5 Control panel.

See the Message Suite user guide for more details.


Installing Message Suite using other computers
==============================================

You must have software installed on your computer to copy files to your 
Series 5, and a suitable cable to connect your computer to the Series 5.
 
- If you have a Windows 3.xx PC, you can use PsiWin 1.xx.

- If you have a Macintosh computer, you can use PsiMac or Psion MacConnect.

If you have not installed software to your Series 5 before
----------------------------------------------------------
To install the Add/remove icon to your Series 5's Control panel:

1. Connect the Series 5 and copy the file instexe.exe to the Series 5.
It is best to copy the file to the "\" (root) folder. Then, locate the
file in the System screen, select it and press Enter to run the file. 

2. After a few moments, the file will disappear from view.

MacConnect users only:
----------------------
You can install the Message Suite from the .SIS file using MacConnect's 
install feature.

Select PsiTools from the Psion menu, press the Install button, and
navigate to the folder in which you have saved Msgsuite.sis. Select this
file and click Open. Follow the on screen instructions to install the
Message Suite programs on your Series 5.

All other users:
----------------
1. Transfer the Msgsuite.sis file to your Series 5. If the file is on your PC, 
you should do this using PsiWin 2.x

2. Select the Msgsuite.sis file in the System screen then tap on it
again. The installation process will begin. Follow the instructions on
screen, making sure that you use the same disk for each Message Suite
component. 

Note: the Msgsuite.sis file will be deleted once the Message Suite has
been installed. To prevent this, open the 'Add/Remove' section of the
Control panel on your Series 5 and tap on 'Prefs', then remove the tick
from the 'Delete .SIS installation file?' box. Note that removing the
tick from this option means that you will need approximately double the
normal recommended amount of disk space free in order to keep both the
installed version of the software and the installation file on your
machine.


Using the Remote Link
=====================
You will need to use the Remote link on your Series 5 in order to
install programs using EPOC Install. Make sure the link is set to
'Cable', and that the Series 5 is connected to your PC.

The Message Suite programs require the Remote link to be 'Off'. If the
link is in use (e.g. set to 'Cable'), the link will be automatically set
to 'Off' when you connect to the Internet. 

Make sure that you switch the link on again when you need to connect to
your PC.


What to do if you have EasyFax installed
========================================

If you have previously installed EasyFax then you should remove the 
following files in order to ensure correct behaviour when sending and 
receiving faxes:

   \SYSTEM\APPS\EASYFAX\EASYFAX.AIF
   \SYSTEM\APPS\EASYFAX\EASYFAX.APP
   \SYSTEM\APPS\EASYFAX\EASYFAX.MBM
   \SYSTEM\APPS\EASYFAX\EASYFAX.R01
   \SYSTEM\APPS\EASYFAX\EASYFAX.R10
   \SYSTEM\APPS\EASYFAX\EASYFAX.HLP
   \SYSTEM\LIBS\FAXSTR.DLL
   \SYSTEM\LIBS\FAXTRANS.DLL
   \SYSTEM\LIBS\FAXVIEW.DLL
   \SYSTEM\PRINTERS\FAXPRINT.PDL
   \SYSTEM\PRINTERS\FAXPRINT.PDR
   \SYSTEM\PRINTERS\FAXPRINT.UDL

Also, to free disk space, remove all old fax files. These will generally 
be in the folder "\Fax\" but could be stored elsewhere. They will always 
have the form "name.fax". 

Note, deleted faxes will be lost, so remember to print out any faxes 
which  you would like to keep a copy of.


Additional information
======================

You can use many standard modems or set up a new entry for your modem. 
Message Suite includes pre-defined entries for:

      - Hayes compatible modem (use this for any non-listed modem)
      - Psion Travel Modem
      - Psion Dacom Modem (e.g. Meteor, Surfer)
      - US Robotics Sportster
      - Direct cable connection (e.g. for a cable connection to NT RAS)

Note that for desktop modems, you need to use a null modem adaptor/cable
and your Docking cable.

Templates for popular Internet service providers are available. You
should check these files and use the template for your provider if
possible. For further information check http://www.psion.com

The Psion Series 5 Message suite supports the following:

Email:
	- SMTP mail send
	- POP3 mail receive
	- Attachments
	- Multiple remote mailboxes
Fax:
	- Fax transmission and reception
	- Class 1, class 2 and class 2.0 fax modems

Web browser:
	- HTML 3.2
	- GIF, JPEG image formats
	- Forms
	- Tables
	- Mailto
	- Image maps
	- 4 / 16 grey scale display

File viewers:
	- Microsoft Word 95
	- Microsoft Word 97
	- Text



