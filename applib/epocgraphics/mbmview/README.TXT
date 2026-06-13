MBMView Version 4.12
Copyright: Lieuwe de Vries 1997-2002
====================================

MBMView is shareware. If you use the program for more than 3 weeks,
please register. The program is working for 3 weeks without nags and
three weeks with nags. Then it stops...
How to register can be read at the end of this document.

New in release 4.12
1. Another bug solved in loading PCX/DCX.
2. Now really three weeks of extra trial.

New in Release 4.11
1. Loading of PCX/DCX sometimes failed. Now less critical.
2. Extra three weeks of usage should you have tried MBMView 4.00
3. JPG's from the Agfa CL20 loaded with a warning message. Removed it.

New in release 4.10
1. Support for PCX and DCX files.
2. Select and load pictures from the mosaic.
3. Some small stuff.

New in release 4.00
1. Major speed up of MBMView.OPX (with just a few lines of code..):
   JPG files load about 27 % faster, large tiff file up to 135%
2. Implemented some very-short cut keys. Only the (lower case) letter is enough to
   execute some often used commands:
   p = print preview
   del = previous file
   u = last used folders
   l = list bitmaps
   w = fit width
   s = fit screen
   h = fit height
   k = preferences
   i = bitmap info
3. Toolbar visibility now saved in ini file.
4. MBMView's screen capture in color on color machines.
5. New recognizers
6. Using a resource file now. Current languages: US, UK and Dutch.
7. Fully Revo compatible.
8. List of last viewed folders.
9. Copy to clipboard function. Save it as Sketch so it can be pasted in Sketch. Also usable
   in Word/Agenda etc. to insert a MBMView picture.
10. Printing.
11. 'Fit height' zoom option
12. List of supported bitmaps in current directory. Makes loading easy and fast.
13. Multiple instances of MBMView now possible.
14. Some enhancement to 'look & feel' of MBMView. Some text is changed, menu's can be dimmed etc.
  (Thanks to advice from Phil Spencer!)
15. Automatically add the correct extension to a file name when saving.
16. New menus
17. Full Color support for Psion 7/Netbook (up to 256 colors).

New in release 3.10
1. All MBMView supported bitmap files (MBM,JPG,TIF,BMP,GIF) are recognized by the system. 
   Just double tap on the file and MBMView is openend. Works in EMail as well.
   On Series 5mx and other ER5 machines the MIME types (JPG/TIFF/MP and GIF) are supported.
2. RealMap files are recognized in MBMView.
3. New zoom option which lets you enter absolute dimensions.
4. Some small enhancements and bug fixes.

New in release 3.00
1. Support for the TIFF file format. This includes (multi page) faxes (JFAX as well) in this format.
   It supports most other TIFF types.
2. Fast zooming.
   Using zoom buttons or dialog.
   Option for "Fit to screen" which automatically adjusts the size of the picture
   to your screen size.
   Option "Fit width" to adjust the width of the picture to your screen size.
3. Fast rotating
4. Fast flipping
5. The standard option to capture a screen (ShIFT+CTRL+FUNC+S) to bitmap is 
   overruled when MBMView runs. MBMView saves the bitmap in MBM format.
   (16 colors instead of the standard 4 used by the system option).
   It saves them using "Capture1.mbm" to "Capture99.mbm" filenames.
6. Mosaic options (optionally) use zooming to show the complete pictures.
7. Optionally make mosaic in 16 colors.
8. Support for larger pictures.
9. Now all AIF files can be read (the icons that is).

New in release 2.22
1. Psion 5mx (ER5) compatible
2. Recognize RMRArt files.

New in version 2.21
1. Recognition of KODAK JPG's as was implemented in version 2.10.
2. Optional use of scrollbars (You can turn them off for maximum screen space)

New in version 2.20
1. Use of scrollbars. Due to this the 'Center' and 'Top left' commands are gone.
2. Saving of bitmaps as JPG (256 colors), GIF (16 colors) and BMP (256 colors)
3. Slideshow of all supported bitmaps in a directory.
4. Better recognition of bitmap types.

New in version 2.10:
1. Support for reading GIF files.
2. New registration numbers required for this version. Registered users will have receive one already.
3. New icon and helpfile thanks to Phil Creed.

New in version 2.00:
1. Support for reading JPG and BMP files. (Using an OPX)
2. Support for reading SpritEdit files.
3. Program is now shareware...

New in version 1.30:
1. Save bitmap as Sketch file.
2. Mosaic from all supported bitmaps in directory
3. New button in bottom toolbar which load the next
   supported file in the current directory.
4. A number of other small enhancements.

New in version 1.21:
1. Removed all the (stupid) bugs from version 1.20

New in version 1.20:
1. Changed menu structure.
2. Create mosaic from all bitmaps in current file.
3. Extra error checking.


What is MBMView
---------------
MBMView is a bitmap viewer which views all bitmap types of the Series 5:
1. 'Normal' and 'multi' MBM files.
2. ROM MBM files(Only from ROM).
3. Sketch files.
4. AIF files (which contain the icons)
5. JPG files.
6. BMP/GIF files.
7. SpritEdit files.
8. Photo5 files.
9. Create mosaic of all bitmaps in one file or directory
10. TIFF files (including fax)
11. PCX files.
12. DCX file. (Multi page PCX fax files)

Once loaded and on screen you can save them as (multi)MBM, Sketch,
JPG, GIF or BMP. You can create/add to multi MBM files. You can even
convert ROM multi MBM files to normal MBM files.

You can scroll the image using the keyboard, pen (just drag it around) or
scrollbars.

It shows bitmaps in 2, 4 or 16 gray scale mode.
If you're lucky to have a color screen, color is also supported in loading and saving.

MBM files are saved in such a way that they can be converted on the PC using
BMCONV.EXE. You can patch existing files with the same result.


Installing MBMView:
-------------------
1. Install all three sis files.

Registering MBMView 4.10:
--------------------------
1. Register on Regnet (www.swregnet.com, ID=2076). 
2. Register on RegSoft (www.regsoft.com, ID=4245). 
   You can easily access these services from my homepage.
   Faxing or calling these services is also possible.
   Details on the pages of these services.
3. Send cash to me at:
   L. de Vries
   Taling 11
   9101 ZG Dokkum
   Netherlands.
   Only use currencies: USD$ 25, EUR 25.

Some interesting information about MBM files on the Series 5:
-------------------------------------------------------------
While analyzing MBM files on the Series 5 I stumbled across some interesting
things:

There are 3 quite different types of bitmap files on the series5. Each with
its own (quite) distinct format:

1. ROM Image files: These are MBM files on the 'Z' drive containing one
   or more bitmaps. They are used by the built in applications. They can be
	loaded using 'gLoadBit' in OPL as long as they are on the 'Z' drive.
	If you copy them to the internal drive (Drive C) they cannot be 
        accessed...
   You can however use MBMView to save such a file as a normal MBM file.
2. Normal MBM files: Files containing 1 or more bitmaps. Such files are
   supplied with most (shareware/freeware) programs, containing images etc.
   You can create single MBM files in OPL using 'gSaveBit'. If you copy
   this file to the PC, it cannot be converted using BMCONV.EXE!
   MBMView patches these files when saving, so they CAN be converted.
3. Sketch files: Files created using the built in Sketch application. These
   file always contain just 1 bitmap and are always in 4 colors.
   MBMView reads these files. They can then be saved as normal MBM files.
   You can save 16 color images to Sketch format, but Sketch will reduce
   these to 4 when loading.
4. AIF files : Contain program icons. MBMView read all AIF types. 

Viewing the bitmaps on screen:
Painting bitmaps on screen can be slow if the screen mode differs from
the bitmap mode. So showing a 16 color bitmap while the screen is in 4 color
mode will be slow because of color conversions. MBMView has an option (set in
the preferences) to automatically change the screen mode so that is matches
the bitmap mode.


Thanks to:
----------
The Independent JPEG Group's JPEG source code (version 6b) which was used
as a base for this software (JPG/BMP/GIF reading/writing).
For the TIFF part of the software:
Copyright (c) 1988-1997 Sam Leffler
Copyright (c) 1991-1997 Silicon Graphics, Inc.



Contacting the author
---------------------
If you find bugs or have suggestions, mail me at:
platodva@compuserve.com
New versions will be available on my homepage:
http://ourworld.compuserve.com/homepages/platodva/psion.htm


Legal stuff:
------------
Notice to users: Carefully read the following legal agreement. Use of the software provided with this agreement (the "software") constitutes your acceptance of these terms. If you do not agree to the terms of this agreement, promptly remove the software together with all copies from your computer. User's use of this Software is conditioned upon compliance by user with the terms of this agreement.

The software is protected by copyright law and international treaty provisions. You acknowledge that no title to the intellectual property in the software is transferred to you. You further acknowledge that title and full ownership rights to the software will remain the exclusive property of L. de Vries, and you will not acquire any rights to the software except as expressly set forth in this license. You agree that any copies of the software will contain the same proprietary notices which appear on and in the software.

We do not warrant that the software is error free. We disclaim all other warranties with respect to the software, either express or implied, including, but not limited to, implied warranties of merchantability, fitness for a particular purpose and non-infringement of third party rights.

There is no liability for consequential damages. In no event shall we or our suppliers be liable to you for any consequential, special, incidental or indirect damages of any kind arising out of the delivery, performance or use of the software, even if we have been advised of the possibility of such damages. In no event will our liability for any claim, whether in contract, or any other theory of liability, exceed the license fee paid by you.
