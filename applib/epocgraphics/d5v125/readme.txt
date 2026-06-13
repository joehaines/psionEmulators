Demarco5 V1.25 1/8/98

License and Copyright.
===============

The Demarco5 application is copyright Andrew Ferry 1998. It is released as freeware on condition that it is not modified, decompiled or reverse engineered in any way.
The Gprinter OPX file is in the public domain as freeware copyright Andy Clarkson 1997. 
You use the software at your own risk and I offer no guarantee of fitness for purpose or offer any commitment to support. I do however welcome feedback and will endeavour to fix any reported bugs on a best efforts basis.

Installation
========

1) A new folder should be created in  \System\Apps called Demarco5
2) The following files should be copied into the \System\Apps\Demarco5 folder:
	Demarco5.app
	Demarco5.aif
	Demarcob.mbm
	Demarcol.opo
	GPprint.opo
	dcircle8
	dcircle16
	dcircle3
	dcircle48   <<<<< this is new in V1.25>>>>
	dcircle64
	WMFout.opo  <<<<< this is new in V1.25>>>>

3) The Gprinter.opx and SysRAM1.opx files should be copied into the \System\OPX folder.

4) Demarco5 will appear on the extras bar.

Usage
====
The application is self explanatory so I have not included a manual or help file.
To create objects select a location on the diagram, tap it twice and a pop up menu will appear. Do the same to edit an existing object. Flows are selected by tapping twice on the flow name.
The diagram may be zoomed using the standard zoom icons on the screen. The diagram may be panned using the arrow keys. Fn+arrow key increases the diagram size in that direction.
See my web site for further information. www.btinternet.com/~adfhome/demarco5/

Revisions
======
V1.24 Fixed the following bugs.
	*Incomplete deletion of objects causing incorrect menu to appear when 		vacant area selected.
	*Ignoring system screen New commands
	*Incorrect printing of nodes
	*Incorrect handling of child file pathnames longer than 32 characters
	(This now constrains child files to the parent diagram file folder)

And added the following new features
	*Official application UID from Psion (provides old diagram file update 		facility)
	*Definition text files can be created/linked to flows, stores and 				terminators.
	*Intelligent zooming ensures selected object stays on screen
	*Menu and ctl-m support for zooming

V1.25 Fixes the following bugs.
	*Incorrect "Can't find GPprinter.opo " error message when printing after opening a text file.
	*Incorrect display of flow names after renaming with a shorter name.
	*Incorrect printing of multi line names where subsequent lines are longer than the first. 
	*Error when creating a text file with a name longer than 28 characters. 
And adds the following new features:
	*Includes Windows Metafile Export facility
	*Now supports moving multiple objects in one operation - shift tap selects additional objects - move first object as normal - additional objects follow
	*Text is no longer selected when text file edit dialog box is displayed.
	*A Delete button is now included in the text file edit dialog box to enable the file to be deleted and the link to the parent diagram disconnected. 
	*An additional intermediate zoom level is provided between the two largest settings.

Thanks to all those who have provided feedback.

Other Information
============

I am no longer intending to publish the DXF export module DXFout.opo. This is now  superceded by the Windows Metafile export funtion included in V1.25 (WMFout.opo).

Andrew Ferry 1/8/98
email adfhome@btinternet.com
