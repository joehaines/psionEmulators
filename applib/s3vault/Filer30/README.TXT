Filer 3.00
==========
Filer is a file management tool for the Psion Series 3a. You are able to handle
your files in one or two windows in one of four viewing modes -
Devices: show a list of all available devices and navigate through a tree of directories
Files: navigate through the files and directories of any local or remote device
Scan: scan a whole device for any file or file specification
View: take a quick view on the files content
Copy, move, delete and rename files, set attributes and file modification date/time,
create or remove directory subtrees, sort directory lists, start applications by itself
or by their data files.


INSTALLATION
============
To install Filer, copy the following files into the specified destination:

	Filer.app	79.472 bytes	\App		Application file
	Viewer.dyl	 6.032 bytes	\App\Filer	Hex/Text viewer
	Shell.dyl	 4.496 bytes	\App\Filer	Shell module
	Undel.dyl	 1.504 bytes	\App\Filer	Wasted space calculation
	Unzip.dyl	 1.840 bytes	\App\Filer	Unzip module interface
	View_pic.dyl	 1.664 bytes	\App\Filer	Picture viewer
	View_dbf.dyl	 2.048 bytes	\App\Filer	Database viewer
	View_wve.dyl	   768 bytes	\App\Filer	Wave file viewer

Then install the application as usual using the 'Install' command from the
'Applications' menu.


USAGE
=====
The application will usually start from the devices list. Select any device and press
ENTER to switch to the files view. Navigate through the list by pressing ENTER for
any subdirectory or ESC for the parent directory. Start an application by selection
and ENTER or by selecting it's data file (if predefined or defined by the extension
editor).

Devices
-------
The devices are listed by it's drive letter, local devices before remote devices.
'Copy'          Copy all (or only the modified) files of a selected device to a
                directory tree.
'Rename'        Rename a device (from it's original name given by the last format)
'Format'        Format a device.
'Disable scan'  If one of the devices in the list is not accessible (e.g. because there
                is no media inserted) you can avoid it's inclusion or reread.
'Recoverable space' Calculate the wasted space on Flash SSD's occupied by deleted files

'Rename'	Rename a directory
'Create'	Create a new directory
'Delete'	Remove a directory
'Compress'	Compress/Expand a directory
'Compress all'	Expand all subdirectories
'Open tree'	Show the files of a directory in a second window
'Description'	Edit a description for any file/directory

Files
-----
Navigate through the files and directories using the cursor keys, ENTER and ESC, mark
them by SPACE or +, unmark them by -.
'Copy'          Copy the contents of a directory, any selected file or a group of marked
                files to any target directory.
'Move'          Move acts like copy but removes the original files and directories.
'Delete'        Removes files and directories.
'Rename'        Rename a file or directory.
'Attributes'    Change the file attributes
'Date'		Edit the file or directory modification date or time.
'Mark'          Select a number of files and/or directories by a given mask and/or any
                modification date range.
'Description'   Enter a short description in addition to the 8/3 filename
'Create'        Create a directory within any path.
'Remove'        Remove a directory.
'Sort'          Specify the sort criteria: sorted by name, size, date, time, extension
                either ascending or descending.
'Refresh'       Reread the current path in case of any changes (e.g. from any other
                processes).
'Path'          Specify a new file mask and/or a complete path.

Scan
----
The result of a file scan can be treated like any file list (without directory navigation)

Extension editor
----------------
By using the extension editor, every extension can have an assigned application, which
will be executed after pressing ENTER on the selected data file. So if there is an entry
like 'CHS' connected to 'A:\APP\CHESS.APP', and you press ENTER on a file 'Game.chs',
the application Chess.app will be launched loading the respective file.
If you press ENTER on any 'unconnected' data file, you will receive a connection dialog
automaticly.
'Insert'        Insert a new extension into the database.
'Edit'          Edit the currently selected entry.
'Delete'        Remove the selected entry.

Unzipping files
---------------
Filer supports the usage of the 'Unzip.img' application, which can be found in
the Palmtop Forum of CompuServe. Simply install the application as suggested
in any \Run or \App directory. Activate the unzipping by simply pressing ENTER 
on any '.zip' file.

Please note the context sensitive help mechanism!


ABOUT THE PROGRAMM
==================
This application is shareware, so if you are using it, you have to register it!

There should be no additional fee charged for distributing the package.

The package is to be kept unmodified and as it is.


REGISTRATION
============
Filer is not free! If you are using it, please register with the author.

Entering the registration code
------------------------------
After receiving the registration code you have to enter it in the registration
dialog from within the setup dialog. Exit and restart the application to activate
the code.
Your name has to be included in the first or second line of the owner info.
After a successful registration there will be no more nag screens, you are able
to save the actual configuration, you can use a definable hotkey for switching to
Filer and the multi-file operations are possible.

Registration fee
----------------
Registration can be made by cash, cheque or by using the CompuServe SWREG service
or the RegNet service on InterNet.

The full registration for Filer is 30.00 USD.
An upgrade from Filer3a 1.2 is 10.00 USD.
If you want the application on floppy and a printed manual this will be 
an additional 5.00 USD.
Please add 2.00 USD for paying by cheque or in foreign currency.
Payment by the CompuServe SWREG service is very quick and simple, but costs 
an additional charge of approx. 15% (which is payed to CompuServe).
Payment by the RegNet service: add 8.00 USD.

So, e.g., if you want a full registration, pay by sending cash, and want to receive
the application on disk together with a printed manual, this will be 35.00 USD.
If you want an upgrade from Filer3a, pay by cheque or in foreign currency, this
will be 12.00 USD.

Registration by SWREG
---------------------
If you prefer registration by the CompuServe shareware registration service,
GO SWREG using the following ID:
Full registration Filer 2.10      ID=12549
  plus disk and printed manual    ID=12478
Upgrade from Filer3a 1.2          ID=12477
  plus disk and printed manual    ID=12550

Registration by RegNet
----------------------
To register by RegNet, load 'http://www.xmission.com/~wintrnx/regnet/regnet.htm'

Registration request
--------------------
Unless you are registering by using the CompuServe SWREG service, you have to send
some registration information:
User name (as stated in the first or second line of the owner data from the
system screen).
Requested registration level (full registration, upgrade).
If possible, an e-mail address, for a faster reception of the registration code.


HISTORY
=======
Up to version 1.2 Filer3a was implemented using OPL. Since applications
which are written in OPL are restricted in many aspects, Filer 2.00 is
the first release which is completely recoded in object oriented C.

Version 2.00:
* First release coded in C
* Installation is simplified by using only one file.

Release 2.01:
* Selectable font
* Extension database may now reside in the \app\filer directory
* Extension editor layout changed
* Auto switch off is now allowed
* A number of bugs were fixed

Release 2.10
* Support of the EPOC Unzip program
* Now there are multifile operations in the scan view possible
* There is a build-in hex viewer with search facility
* You can scan for files as well as for text in files
* Write a short description for every file/directory
* Extension editor:
  You can connect aliases to extensions
  Defined connections will now overrule ROM applications
  Connections can now be made to ROM applications
* There are now two different alphasearch methods
* Setup is now divided into general setup and display/window setup
* Select the inclusion of hidden/system files
* Define a hotkey to launch Filer
* A number of bugs have been fixed

Release 2.20
* Text viewer
* Viewer library, using independent Dyl files
  - PIC viewer: scroll through grey or b/w PIC files
  - DBF viewer: browse through DATA apps files or any OPL generated DBF file
  - WVE 'viewer': take a quick acoustic view onto wave files
  - fallback to hex viewer on demand
  - the library of viewing modules will be extended in the future
* Optional inclusion of the internal ROM device (browse, copy, view,...)
* Extensive usage of additional confirmation dialogs within deletion operations
* Save disabled disks to configuration
* Support write protected disks as application disks
* Optional quickview (autoview) of files
* Bugfixes

Release 3.00
* Migration of the tree mode into the device mode by using a complete new layout
* System information (memory usage)
* Local and remote disks are now subgrouped
* Multifile operations now even work on rename, attributes and modification date change
* Tree creation performance increased
* Edit descriptions for directories in the device mode
* Calculate the recoverable space on Flash SSD's
* Provide a much better unzip support
* Expand all directories below any directory or any whole disk
* Better representation of HexView
* Automatic tree update in quickview mode
* Automatic link switch on/off support
* Bugfixes

DISCLAIMER
==========
Although the program has been tested extensively, there is no guarantee that
it is error free (like any piece of software  :-) )
Please make backup copies from your important data!

The author will not be liable for any damage or consequential loss caused
by any error, defect or failure of the application.


ABOUT THE AUTHOR
================
If you have any questions or suggestions regarding Filer, please do not
hesitate to contact the author:

Thomas Nemec
Gladbeckstrasse 1/12a/10
2320 Schwechat
Austria

CompuServe 100024,3461
Internet 100024.3461@compuserve.com
WWW http://ourworld.compuserve.com/homepages/tnemec
Phone/Fax: +43 1 7077807 (MEZ daytime)

December 1996, Schwechat, Austria
