IconPatcher: Patch/extract/view/rename your Extras bar icons...!


What is IconPatcher?
====================

IconPatcher is a small program designed to easily let you replace a program's icon on the Extras bar.  The program icon is the one that shows in your Extras bar (and possibly elsewhere in your System folders depending on what the program is).  The main reson for doing this is probably for replacing older (or no-longer maintained) greyscale icons/applications with a colour one - mainly so that they'll look prettier on a colour ER5 machine such as a Series 7, netBook, or netPad.  However the program will work equally well on a monchrome ER5 machine such as a 5mx, revo, etc.

The program will also let you extract images from the program icons (via their .aif files) for editing, etc., view these images, and change the program name as it appears on the Extras bar (the last two options via the plugins supplied with this program).

Program icons can be patched manually - and indeed I've already described how to do this (both on my own website - www.pscience5.net - and FoxPop -  www.foxpop.co.uk).  However, IconPatcher has a number of advantages over doing it manually:-

•	You don't need to use a hex-editor program such as FileDump to manually extract program UIDs.

•	You don't need to use a graphics  program such as MBMView or OneMBM to combine single-image MBM into a multiple-image MBM.  Equally, you don't need one of these programs in order to extract images from original .aif files for editing.  All you need is IconPatcher and the built-in Sketch program in order to extract from, edit, and recreate .aif files...

•	You can view original .aif files before doing anything.

•	IconPatcher will correctly preserve the UID and also all captions, capabilities flags (eg. hide from Extras, New File), and MIME information from the old icon - which the manual method won't (easily) do.

•	You can equally well use other icons (i.e. .aif files) as the source for the replacement icon you want to generate.  This means that you can directly swap icons around from one program to another if you want to!


What it can't do...
===================
It won't do the colouring in for you!  In other words, you still have to do the necessary 'artwork' (in Sketch or another image-editing program).  However, Sketch will import and export single-image bitmaps quite happily so that's all you need in order to edit/create the replacement images and masks for the various icon sizes - and you can extract original images from .aif files (using the Extract function) for importing into Sketch as well.

Remember that you need to create a 24x24 image+mask, 32x32, and 48x48 (and a 16x16 pair if you want full revo compatibility too).  Well, in actual fact you can get away with just the 48x48 image+mask for the Extras bar - but it'll still show a greyscale version of this in the System screen view if the zoom is not set to the maximum level.  So it's worth doing all the size versions if you can.  :¬)  See the 'More info on .aif files' section for more details...


Using IconPatcher
=================

When IconPatcher starts and initialises, you are presented with the main dialog box which gives you the following options:-

•	Close - quits the program.

•	About - displays the credits boxes.

•	Help - runs this help file.

•	View - runs the IconViewer plugin which lets you view the images in .aif files.  See the 'Plugins: IconViewer' entry for details.

•	Extract - extracts the images from an .aif file (for editing, etc.).  See 'Extracting .aif images' for details.

•	Caption - lets you change a program caption (i.e. title) in the Extras bar. See the 'Plugins: CaptionPatcher' entry for details.

•	Patch - changes the icon(s) in an .aif file using multiple single-image .mbm files, a single multi-image .mbm file, or an existing .aif file.  See the 'Patching .aif files' for details.


More info. on aif files...
==========================
AIF files store various bits of information about the program such as its UID (unique ID - which identifies it as being different from any other program), file associations and/or MIME information, it's title in the Extras bar, and the icons it needs to use on the Extras bar and in the System screen.

You can see why different sizes are needed for the system screen view if you go to a \system\apps\anyprogram\ folder and then toggle through the various zoom views.  A well-designed/sized icon will zoom nicely and look good in each view.  If the program's author has been a bit lazy and not provided different size icons for each zoom level, then EPOC will make an intelligent guess at re-sizing them itself as required - but these are never as good as the real thing.  Also, if you're looking at colour icons on a colour machine but some sizes are missing, the EPOC-guessed ones will only be in greyscale...

By way of example, lets look at a complete set of icons for Clock5 (my own clock program for every EPOC machine):- 

1. 16x16 icon image:		[image not shown]
2. 16x16 mask image:		[image not shown]
3. 24x14 icon image:		[image not shown]
4. 24x24 mask image:		[image not shown]
5. 32x32 icon image:		[image not shown]
6. 32x32 mask image:		[image not shown]
7. 48x48 icon image:		[image not shown]
8. 48x48 mask image:		[image not shown]

Every size of icon used in the AIF file must have a matching 'mask' as well.  The purpose of the mask image is to mark the parts of the associated image that won't be transparent where they're white.  In the above images, you don't want the white clock face to be transparent (and hence show the Extras bar or System screen underneath it) and so that has to have a black mask covering it.  In fact, for most icons, you'd simply make a copy of the original image and fill it all in as black from the outermost edge of the image inwards (leaving anything beyond the outermost edge of the image picture since you probably would want that to be transparent).

The above example is the maximum number of icon images/masks that you might need.  Many authors only distribute 24x24, 32x32, and 48x48 icons - however the 16x16 size is the smallest zoom size used by the Psion revo (although it doesn't use the 48x48 size).  Hence a complete set should strictly speaking use 4 pairs of images - although the largest 3 pairs might also be considered sufficient).

Normally, the icon images/masks will be ordered either smallest to largest or the other way around.  The images must be paired with their respective masks however.  Hence you would expect the images to appear either in the order as in the above example - or the other way around.


Plugins: CaptionPatcher
=======================
Essentially, CaptionPatcher adds the ability to edit/change an icon's title.  This will affect it's title both on the Extras bar and in the System screen (depending upon what the application is and how it appears on the System screen in the first place).

CaptionPatcher itself is very straightforward to use and should be self-explanatory for the most part.  If installed, it is started from the main IconPatcher dialog by selecting the 'Caption' button.

NB: The name that you use to replace the existing caption name with must be the same length as the original.  However, if you want to use a shorter name then you can simply use spaces to fill in the extra characters.  In order to keep the name centred on the Extras bar, I would suggest putting an equal number of spaces before and after the shorter-name replacement.  E.g. change "Clock5" to "  C5  ", etc..


Plugins: IconViewer
===================
IconViewer lets you view the images contained in a specified .aif file.  It will display a table of 8 cells showing 16x16, 24x24, 32x32, 48x48 images (in colour if they are in colour) and their associated masks.  Note that even if a particular size of icon does not exist, IconViewer will display an extrapolation of what it believes the icon should look like (similar to what the EPOC OS does if an icon is missing).  You can usually tell if this has been done because the image will look 'approximated' compared with one that really exists.

IconViewer is straightforward to use and should be self-explanatory for the most part.  If installed, it is started from the main IconPatcher dialog by selecting the 'View' button.


Extracting .aif images
======================
The image extraction utility works similarly to the IconViewer plugin except that instead of displaying the images, it will extract them to single-image .mbm files (for editing purposes, etc.).  I.e. it will generate 16x16, 24x24, 32x32, 48x48 images (in colour if they are in colour) and their associated black & white masks.  Note that even if a particular size of icon does not exist, the program will generate an extrapolation of what it believes the icon should look like (similar to what the EPOC OS does if an icon is missing).  You can usually tell if this has been done because the image will look 'approximated' compared with one that really exists in the .aif file.


Patching .aif files
===================
The first file you have to specify is the .aif file of the program that you want to patch.  This is the file that contains the Extras bar / System screen icon information for the program.  Normally, this will be located in c:\System\Apps\ProgramName\ or d:\System\Apps\ProgramName\ - where 'ProgramName' is the name of the application you want to change the icons for.  (NB: Make sure that in the System screen you tick the Menu|Tools|Preferences...|Show 'System' folder... option in order to be able to navigate to these folders).  For ease of navigation, it initially points to c:\System\Apps\ - but this can be changed to wherever the .aif file is located.

The edit box below is for entering the second file - or files - that you're going to use as the source for the icon information that will replace the original.  This can either be single-bitmap .mbm files (as can be exported from Sketch for example), a multi-bitmap .mbm file, or an existing .aif file.  This needs to be entered as the full path + file name for the latter two options (e.g. c:\temp\icon.mbm) or as a string of single-bitmap .mbm locations/files separated by commas for the former (e.g. c:\temp\icon24.mbm,c:\temp\icon24m.mbm,c:\temp\icon32.mbm,c:\temp\icon32m.mbm,c:\temp\icon48.mbm,c:\temp\icon48m.mbm - where the files are paired image and mask files - see my website descriptions for further details).

Note that IconPatcher will correctly preserve the UID and also all captions, capabilities flags (eg. hide from Extras, New File), and MIME information from the original .aif file.

Once you've specified the files/locations, just hit 'OK'.  The program will back up the original .aif file to ProgramName.aif.bak before changing the ProgramName.aif file.


Compatibility
=============
IconPatcher has been designed to work on any ER5 EPOC OS machine - e.g. 5mx, revo, etc.  It really comes into its own on colour machines however.


Acknowledgements
================
The 'core' for this program was written by Andrew Gregory (andrew@scsoftware.com.au) and the source supplied as-is for further development/publication.  Visit Andrew's website at http://www.scsoftware.com.au/family/andrew/ to find some of his other software.

I am indebted to him for doing this as I simply would not have had the time nor the inclination to start development of IconPatcher.

Thanks Andrew!  :¬)

The CaptionPatcher and IconViewer plugin modules were very kindly donated by 'Edo'  - many thanks Edo!


Standard disclaimer
===================
Whilst I have tested IconPatcher reasonably carefully, I cannot and do not give - or imply - any sort of warranty with it.  Nor will I accept responsibility for any data lost or damage caused as a result of using IconPatcher.  I.e. use at your own risk!


Updates
=======
Any updates will be via the Pscience5 web site at:
http://www.pscience5.net


Best regards,
Martin Guthrie, July-Oct 2003
(martin@pscience5.net)
