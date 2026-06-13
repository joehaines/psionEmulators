Mobile Connectivity Update 1.10
===============================

This update will improve the IrDA or cable connection of the Psion Series 5mx
with the following range of data enabled mobile phones:
Ericsson T28 (with DI28), Siemens S25, Sagem M850, Bosch 908 and 
Motorola Timeport L7089.

This update is NOT required for users of the Psion Series 5, Series 7 or Revo.

Known Issues
============

It is possible for the Motorola handset to get into a situation where you can
send SMS messages using the Series 5mx, but not bring messages, that the handset
has already received, onto the Series 5mx.

If this occurs it can be easily resolved by turning the handset off and then on again.


Installing the Mobile Connectivity Update using a Win 95/NT PC
=============================================================
You must have PsiWin v2.X installed on your PC.

PsiWin 2.1 (and above), and other Psion programs now include "EPOC Install" 
which you can use from your PC to install programs to your Series 5mx. 

To check if you already have EPOC Install, right click on the "My Psion"
icon on the desktop and note if the "Install New Program" option exists.
If it does not, you should first run SETUP.EXE to install the "EPOC
install" program. 

You can now use the "Install New Program" option or double-click on a
.SIS file in Windows Explorer to install the update on your Series 5mx. 

Installing Mobile Connectivity Update using other computers
===========================================================

You must have software installed on your computer to copy files to your 
Series 5mx, and a suitable cable to connect your computer to the Series 5mx.
 
- If you have a Windows 3.xx PC, you can use PsiWin 1.xx.

- If you have a Macintosh computer, you can use PsiMac or Psion MacConnect.

MacConnect users only:
----------------------
You can install the Mobile Connectivity Update from the .SIS file using MacConnect's 
install feature.

Select PsiTools from the Psion menu, press the Install button, and
navigate to the folder in which you have saved MobileConnectivityUpdate.sis. Select this
file and click Open. Follow the on screen instructions to install the
Mobile Connectivity Update on your Series 5mx.

All other users:
----------------
1. Transfer the MobileConnectivityUpdate.sis file to your Series 5mx. 
If the file is on your PC, you should do this using PsiWin 2.x

2. Select the MobileConnectivityUpdate.sis file in the System screen then tap on it
again. The installation process will begin. Follow the instructions on
screen, making sure that you use the same disk for each component. 

Note: the MobileConnectivityUpdate.sis file will be deleted once the update has
been installed. To prevent this, open the 'Add/Remove' section of the
Control panel on your Series 5mx and tap on 'Prefs', then remove the tick
from the 'Delete .SIS installation file?' box. Note that removing the
tick from this option means that you will need approximately double the
normal recommended amount of disk space free in order to keep both the
installed version of the software and the installation file on your
machine.


Using the Remote Link
=====================
You will need to use the Remote link on your Series 5mx in order to
install programs using EPOC Install. Make sure the link is set to
'Cable', and that the Series 5mx is connected to your PC.

Possible Error messages
=======================

"The component Comms components (TCP/IP) is not present. Install Comms components (TCP/IP) and try again."

This message will only appear if:

a) You have an old version of EPOC install on your PC and it was not uninstalled when PsiWin 2.3 was installed.
The current version of EPOC install is 52 to check which version you have goto the directory containing your PsiWin program files (usually C:\Program Files\Psion\PsiWin) and right click on EPOCinst.exe. If under the version tab it has a number lower than 1,0,0,52 then you have the wrong version of EPOC install.
To fix the problem you will have to re-install PsiWin 2.3

b) You have an old version of Message Suite on your Psion. To ensure you do not have any copies left over from a previous machine
1. goto Add/Remove in control panel and delete Message Suite v1.??
2. delete the following directories from your system folder on either your C: drive or D:
\system\controls
\system\apps\email
\system\apps\instctl
\system\apps\browser
\system\libs
\system\messages
\system\data
\system\printers

(c)Psion Computers PLC 1999
v1.10
November 1999