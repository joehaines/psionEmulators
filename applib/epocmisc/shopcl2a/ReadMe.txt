ShopCL Version 2
=======
An intelligent shoplist program.
Freeware by Paul Laterveer
e-mail:P.P.Laterveer@inter.nl.net
Latest version: v.2a
Parts of coding used from
- Al Richey (S5Event) www.rmrsoft.com
- R.Panton (VWin) www.iota.demon.co.uk/psion/vwin.html

The program helps creating shopping lists from a complete list
of possible items and assists in the actual shopping.
Prior to a shopping event ShopCL presents a shopping list with the 
most likely shopping items, based on your previous shopping
behaviour and allows you to adjust the list.
Export and import functions allow for combining lists and using 
shop profiles and lists from other users.
ShopCL can also be used for creating and maintaining check lists.
 
Content:
========
This zip archive includes all the files necessary to
install and run ShopCL

- This ReadMe.txt
- ShopCL.aif
- ShopCL.app
- ShopCL.hlp
- ShopCL.ini
- ShopCL.mbm
- ShopCLDemo
- ShopCLLogo.mbm
- SysRAM1.opx
- VWin.opo

Installation:
=============
- create a \System\Apps\ShopCL\ folder on the disk C: or D:
  (if necessary, make the 'System' folder visible: press
  Menu from the System screen, then Tools, Preferences, and
  tick  "Show 'System' folder"...)
- copy the 6 files ShopCL.app, ShopCL.aif, ShopCL.hlp, ShopCL.ini, ShopCL.mbm, and
  ShopCLLogo.mbm to the \System\Apps\ShopCL\ folder
- Copy ShopCLDemo to the \Documents\ folder
  (If necessary create a \Documents\ folder on the disk C: or D:)
- Copy VWin.oph to the \System\Opl\ folder
  (If necessary create a \System\Opl\ folder on the disk C:)
- Copy SysRAM1.opx to the \System\Opx\ folder
  (If necessary create a \System\Opx\ folder on the disk C:) 

Upgrade note:
=============
You only need to copy the files that are more recent than the ones 
you already had.
The first time you start the program after installing the files 
the program displays a message which states that the database might 
be corrupt. If you confirm the recover process, the integrity of the 
database is checked and inconsistencies are fixed. If you want to keep 
your database in the original state and did not make a back up already, 
do not recover, make a back up of your database and try again.

Operation:
==========
Start ShopCL from the Extras toolbar.
The on line help contains a Getting Started section.
Please e-mail me if you encounter any bugs or have suggestions for
improvement.

History:
========
First release 5-11-1998: version 0.9 (Beta)
	
Release 22-11-1998: version 1.0
	A few improvements have been made to the beta version.

Release 06-12-1998: version 1a
	Errors solved:
	New Item gives an error when shoplist is current 
	The shoplist is blank after update shop (until the next action)

Release 13-12-1998: version 1b
	Errors solved:
	The wrong SysRAM1 (SysRAM1.oxh) was suplied instead of SysRAM1.opx
	and the installation instructions indicated to copy the file to System\OPLl
	instead of to System\OPX.

Release 22-12-1998: version 1c
	Errors solved:
	Leaving the name field blank, when entering a new Item, Shop, Category,
	Shelf or Unit, resulted in an error.

Release 08-02-1999: version 2
	Major improvements:
	Introduction of the Shop Profile. 
        Import and export of a whole database, a shopping list 
        or a shop profile.
        Database integrity and recovery.
        Merge and New List for either the current shop or for all shops
        Toolbar replacement button in wide view.
        Many shortcuts.
        A few fixes.

Release 22-02-1999: version 2a
	Improvements:
	Automatic detection of Tab delimited import files
	Hot keys added to dialogs
	Last used Pick list order remembered
	Bug fixes:
	Update unit
	Delete unit
	Error in Recovery function, when the pick list is empty

Disclaimer:
===========
Users of ShopCL must accept this disclaimer:
"Paul Laterveer is not responsible for any damage that is a result
of running ShopCL.
This product is provided as is, without any representation or
warranty of any kind, either express or implied.
Use at your own risk."

Last updated 9 February 1999.


