README.TXT 28/11/1998

This is the readme file for the EPOC 3D Engine and flame libraries, by Nik Van den Wijngaert


Installation:
-------------

The engine has been named the FLAME Graphics Engine
The Eikon app runs with support from 2 Libraries: tfixed.dll and flame.dll.
These must be installed in this order, or the .sis installer will notify you of an incorrect dependency.
(so first tfixed, then flame and finally 3dfe)



NOTES:
------

* I am aware of some undesired results when clipping to the screen (when zooming in really wide), mode switching could not perform as expected (but should do so in most cases :) )
* while the app is still in development, this release is labelled as 1.00 (as in "Public release 1.00"; not "final release 1.00"  :) )
* the shortcut keys for rotation have changed, because of some undesired effects on the menu handling (when using the arrow keys to navigate menus)
* the installed files will show up as 3 different files in the Add/Remove panel, so you can remove the app, but keep the libraries
* you can only open one file; close and restart to open another (small fix in the engine library, will be ok in next release)
* only open correct .asc files
(the open dialog filters out the .asc files, but don't try to open a word file with the extension .asc for example...)
* the main program and the libraries _do not_ have officially assigned UID's, I'm waiting for EpocWorld to assign some to me...


A NOTE ON MODES
---------------

The engine currently supports 3 display mode:
Bounding Box:		a bounding box is sdrawn
Dotted:			only vertices are drawn
WireFrame:		full wireframe display

Since transformations on the wireframe are slow (due to the redrawing and number of vertices), it's now possible to switch to bounding box mode, transform that and switch back to wireframe.


TECH NOTES:
-----------

* the blitting to screen in Eikon seems to suffer greatly from the Eikon framework; I'm working hard to speed things up
FOR NOW, THE DISPLAY WILL FLICKER, this is because drawing is done directly to screen, and not by using double buffering, this will be fixed once I find the way to speed up the blitting big time


NOTE: this program is in full development (alpha stage), it has not been optimized for
performance


If you have any comments and/or remarks/tips, contact me at
Nik.VandenWijngaert@ping.be




Disclaimer: the engine has worked perfectly on the emulator and on my Psion5, so it should do
so on yours.
I shall not be held responsible for any unfortunate events as a result of running this app.	