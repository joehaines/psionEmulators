                     IMPORTANT INFORMATION
                     =====================

     !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
     !  Before unzipping the application files in PRODUCT.ZIP  !
     !  please read LICENS.TXT.                                !
     !  You must not unzip PRODUCT.ZIP only if you do not      !
     !  agree with the contents of LICENS.TXT                  !                                      !
     !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

     Installation
     ------------
     Please see INSTALL.TXT.

     
         -------------------------------------
         !           Warning !               !
         ! The unregistered version of this  !
         ! program cannot store bitmaps      !
         ! larger than 48*48 pixels.         !
         -------------------------------------

     
     Registration
     ------------

     The registration fee is A$49.95 plus A$6.00 for shipping and handling.
     
     Please use ORDER.TXT and send to

            Berthold Daum
            lichtbild pty ltd
            21 Margate St.
            LPO Box 6011
            Cromer, Vic 3193
            Australia

            CompuServe 100026,3365
            INTERNET: 100026.3365@compuserve.com
            HomePage: http://ourworld.compuserve.com/homepages/lichtbild
            fax: +61-3-9584 9638


or you can register via CompuServe (US$39.95) GO SWREG.


 Author:            Berthold Daum
 CompuServe ID:     [100026,3365]

 Program Title:     PICPOC S3A V2.6 PICTURES IN MY POCKET
 Program name:      PIC3A.ZIP
 Registration ID:   4498
 Fee (US$):         39.95

 Shipping & Handling Fees:  US$ 4.00

We supply you with a registration number via e-mail or by mail.



Entering the registration number
--------------------------------

   Press MENU, / , ENTER, ENTER
   then enter name and the registration number.

   Note: The entered name must be part of the machines owner name.
         The registration number must be issued on the entered name.





               PicPoc Version 2.6a - Release Notes
               ===================================

Program Description
-------------------

Advanced painting tool for PSION Series 3a computers.

Includes features such as:
 Polyline, Rectangle, Rhomb, Ellipse, Text, Pen, Eraser, Fill,
 Spray, Lupe, Undo, Copy, Cut, Paste, adjustable pen, eraser,
 and spray size, different text fonts and styles, different and user
 definable fill patterns, scrolling in large bitmaps.


Platforms
---------

 This program runs on PSION S3a and ACORN PocketBook II computers.

 S3a-emulator:
 The program runs on S3a-emulator. Images up to 640x400 pixels can
 be printed and can be edited full screen by running the emulator
 with large screen setting.
 Some multiple key operations (like B,W,G-cursorkey) are not supported
 and the system crashes sometimes. Save your work frequently.


Requirements
------------

 The installation routine informs of the size of each option. 
 Depending on options an installation needs between 48k and 93k disk space.


 When active, the system requires >12k of memory 
 (+ 3-5 times the size of the bitmap).

Related products
----------------
 PocView is a viewer for PIC,PCX, and GIF files. PocView is able
 to open very large files in reduced form and to select parts of
 a large file for detailed viewing.
 It also offers convenient screen snapshot viewing and printing.


New in PicPoc 2.60
-------------------
   - Image navigator for images that won't fit on the screen.
     Presents a reduced image (4 times reduced) and allows
     to position the screen window within the reduction.
   - Autoselection. When defining a selection area or an
     object size, press "-" to get rid of empty space
     around an object. "+" grows the selection area.
   - Info palette shows current position.
   - Support for BMP Import/Export added.
   - All import filters support now 3 types of dithering:
     none, 2x2 (as before), diffusion.
   - The key combination PSION-Spacebar (Filefinder) has been
     replaced by CNTRL-Spacebar because of S3c.
   - New effect filters: Wave, ZigZag
   - The Invert filter was changed. It no longer inverts by
     planes, but by grey values.
   - Filter foundry added:
     Define own filters as algebraic expressions.
   
   - Printed manual now also in German language.

New in PicPoc 2.58
------------------
   - Rotate filter can now rotate any angle.
     Due to the nature of bitmapped images the image quality
     will suffer if the angle is other than 90,180,270 degree.
     Rotation will also be significantly slower.

New in PicPoc 2.57
------------------
   - Faster and smaller Rotate-, Enlarge-, Reduce-Filters
   - Faster filling
   - New options for printing
     Paper format, Orientation, and number of copies
   - Printer timeout detected

New in PicPoc 2.56
------------------
   - Support for interlaced GIF

New in PicPoc 2.53
------------------
   - A title line can be specified for printouts.
   - Margins and scale for printing.

New in PicPoc 2.52
------------------
   - GIF filter now accepts all GIFLITE files
     (not only the ones created with the registered product).

New in PicPoc 2.51
------------------

   - GIF import/export filter.

   - PCX and GIF import interruptable with ESC key.
   
   - CROP filter. Selected area is cropped.
     The cropped area can be placed on a larger canvas.
     The new canvas will replace the current drawing.

   - SAVE filter. The selected area is saved
     into the specified bitmap file.

   - PRINT filter. The selected area is printed.

   - help text for filters.


Handy hints
-----------
        - You can't find a needed function in the menu?
          You may find the functionality in the filter collection!
          Select the filter function and browse through the filters.
          
        - Where is the cursor?
          Press the spacebar. The lupe area shown encloses the cursor.
          Press ESC to abort lupe.

        - Draw dotted line.
          Define the distance between dots via Preferences as step width.
          Then switch to the pen tool and move pen while keeping the PSION key
          pressed.
          You can also make a line dotted, by drawing a solid line, setting
          the fill border to solid area, and filling the line with an appropriate
          fill pattern.

        - Draw a sinus curve.
          Draw a straight line and apply the Wave filter.

        - Making a screen hardcopy.
          Use the internal PSION function: PSION-SHift-CNTRL-S. The screen
          is written to m:\screen.pic. Rename this file before making the next
          snapshot.
          
		- Converting a two-plane (black&grey) bitmap into a one-plane bitmap
		  Create a new Black-only bitmap of the same size.
		  Place the old bitmap on the new one. They grey tones will be
		  converted into a black/white pattern.


Import/Export-filters:
----------------------

    Please see document FILTERS.TXT
    

Printing:
---------

    Please see document PRINTING.TXT
     

Effect-Filters:
---------------

  Picpoc allows to apply predefined or user written filters to
  selected image areas or to the clipboard.
  
  The following filters are delivered with PicPoc:

     CROP    : Crop selected area and place into (larger) canvas
     EMBOSS  : Emboss selected area.
               Define edge thickness in X- and Y-direction
     ENLARG  : Enlarges or stretch by factor 1-8
     FLIP    : Flip horizontal or vertical
     INVERT  : Invert selected area (black only, grey only, both planes)
     PRINT   : Print selected area.
     REDUCE  : Reduce by factor 1-8.
     ROTATE  : Rotation -360 ... +360 degree
     SAVE    : Save selected area into bitmap file.
     SHADOW  : Create grey shadows (X- and Y-offset can be specified)
     SKEW    : Skew to the left, right, upwards, downwards in angles from 0-45 degree.
     WAVE    : Applies a sinus wave distortion
     ZIGZAG  : Applies a zigzag wave distortion

  Rolling your own is simple:
  Each filter module must contain a procedure

	proc filter%:(indrw%,planes%)

			indrw% = handle of drawable containing input bitmap
			planes% = number of planes of input drawable
			          (1=black only, 2=black&grey)

			The filter procedure must create a new drawable
			with the same number of planes. The new drawable
			contains the modified bitmap. The handle of this
			drawable is returned as the result of filter%:.
			Return 0 for no result.

			If you had created other drawables during the
			filter operation make sure to close them before
			leaving the filter.

   Translate the filter and install the resulting OPO-module
   into folder \APP\PICPOC\FILTER\ .
   
   Examples: FLIP.OPL and SHADOW.OPL

   To add help pages for filters:

         Simply add new records to the help file (PICPOC.HLP).
         These files are Database files and can be edited with
         the inbuilt DATA application.
         The title line must contain the help hook and the
         actual title:

               :name@title

         where <name> is the name of the filter module.

         Example:

               :flip@Using the flip filter

            would be an appropriate title line for the flip filter.

         The following lines can contain text or references to other
         help pages. References are prefixed with "." .


Troubleshooting:
----------------

    Graphic applications are memory hungry. A black&grey image in screen size
    needs 19kb of memory.
    Multiply the images size by 5 to have a rough estimate what PicPoc will need for
    some operations (especially Fill and Filters).
    You can save memory by switching off UNDO and emptying the clipboard.

    When memory becomes short, strange things can happen. Usually an error message
    is given, but if memory becomes so short that even this is no longer possible,
    the system will panic - the drawing is lost.

	In such environments it is a good idea to save from time to time. Choose "Save as..",
	press ENTER, and answer Yes.

	In case of insufficient memory, PicPoc automatically clears the clipboard and
	UNDO buffer, before prompting the user with an error message.
	Choose "Continue" and save your work immediately.
	

Documentation
-------------

An extensive help file is supplied.

A printed manual is shipped on registration.



Acknowledgements
----------------
Thanks to Lonnie Dworkin for PATTERN2.
Thanks to Eberhard Rompf for PATTERN3-9.
The portrait in Examp1.PIC was taken by Johanna Hechfellner.
                        








