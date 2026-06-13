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
         ! program will leave a remark in    !
         ! files printed or saved with this  !
         ! program if these file are larger  !
         ! than an icon (48x48)              !
         -------------------------------------

Registration
------------

The registration fee is A$19.95 plus postage and handling.

 Shipping & Handling Fees: none

Please use ORDER.TXT and send to:

            Berthold Daum
            lichtbild pty ltd
            21 Margate St.
            LPO Box 6011
            Cromer, Vic 3193
            Australia

            CompuServe 100026,3365
            INTERNET: lichtbild@compuserve.com
            HomePage: http://ourworld.compuserve.com/homepages/lichtbild
            fax: +61-3-9584 9638

or you can register via CompuServe (US$19.95) GO SWREG.


 Author:            Berthold Daum
 CompuServe ID:     [100026,3365]

 Program Title:     pocView S3A V1.1 Pocket Viewer
 Program name:      pview.ZIP
 Fee (US$):         19.95

 Shipping & Handling Fees: none

We supply you with a registration number via e-mail or by mail.



Entering the registration number
--------------------------------

   Press MENU, / , ENTER, ENTER,
   then enter first name and the registration number.

   Note: The entered name must be part of the machines owner name.
         The registration number must be issued on the entered name.


Related products
----------------
 PicPoc is a full featured paint program for the S3a.
 See ORDER.FRM for details.
 


               pocView Version 1.4a - Release Notes
               ====================================

Program Description
-------------------

Image viewer and converter for PIC files and other formats.

Supports currently PIC, PCX, GIF formats.

Prints to Apple, Canon, Epson, IBM, HP, Postscript.


Platforms
---------

 This program runs on PSION S3a/S3c/Siena and ACORN PocketBook II computers.

 Emulator:
 
 The program runs on the S3a- and Siena-emulator.
 


Requirements
------------

 The installation routine informs of the size of each option. 
 Depending on options an installation needs between 21k and 98k disk space.


 When active, the system requires >11k of memory 
 (+ extra memory for opened bitmaps).

Known problems
--------------
 After using a filter, die Find-File-Function will not work. This is
 caused by a problem in the S3a graphics subsystem - and we haven't
 found a workaround yet!

New in version 1.46a
--------------------
   - Fade option for slide shows (not on emulator)
   - Save dialogue in one step
   
New in version 1.45a
--------------------
   - PSION Siena supported.

New in version 1.43a
--------------------
   - We dropped the limitation for the image width of 640 pixel.
     The only limitation now is that imported images must not be larger than 256,000 pixel.
     Larger sizes can be viewed in reduced size. A new option "Auto" for reduction factor
     eliminates the guesswork and imports images with the smallest possible reduction
     factor.
   - When printing a very wide image, the image can be tiled optionally.
   - You can use now the import filters in your own OPL-application for noncommercial
     purposes. An interface description is found in FILTERS.TXT

New in version 1.42a
--------------------
   - Symmetric cropping.
     During cropping press Diamond to switch to symmetric handle movement.
     Press Diamond again to return to normal.

New in version 1.40a
--------------------
   - Support for BMP Import/Export added.
   - All import filters support now 3 types of dithering:
     none, 2x2 (as before), diffusion.
   - Slide show function added.
   - AutoCrop function added.
   - "-" and "+" keys shrink/expand cropping frame.
   - Invert filter added.

New in version 1.31a
--------------------
   - Batch conversion and printing
   - Reduction during file open now also for PIC files
   
New in version 1.30a
--------------------
   - PicPoc filters can be used in PocView to rotate, enlarge, and reduce images
   - New options for printing
     Paper format, Orientation, and number of copies
   - Printer timeout detected

New in version 1.16a
--------------------
   - It is now possible to open images with PocView from the
     system screen. The default directory is the root directory M:\ .

New in version 1.15a
--------------------
   - Support for interlaced GIF

New in version 1.13a
--------------------
   - The title line for prints can be now be specified.
   - Margins and scale for printing.
   - PostScript/Apple printer driver added.

New in version 1.12a
--------------------
   - GIF filter now accepts all GIFLITE files
     (not only the ones created with the registered product).


New in version 1.10a
--------------------

 - GIF support added.
 - Crop function added.
   To define an area, select handle
   with TAB/Shift-TAB and drag handle
   using cursor keys. Press ENTER to confirm.
 - Reduction factor in Open-File function.
   Specify a reduction (1,2,4,8) to display
   a large file. Then use the Crop-function
   to look at an area in detail.
   (Reduction is not available for PIC files).

   Example: HUGE.GIF is a huge GIF file 3840*1280 pixel.
       Start with reduction factor 8, then zoom
       in with the crop function.
   

Import/Export-filters:
----------------------

    All filters support images of up to 5120*3200 pixels
    (using the reduction and crop technique)

    For details on file formats see document FILTERS.TXT

Printing:
---------

    Please see document PRINTING.TXT

Effect-Filters:
---------------

  Picpoc effect filters can be applied in PocView.
  PocView is delivered with 3 selected PicPoc filters:
  
     ENLARG  : Enlarges or stretch by factor 2-8
               Useful to convert S3 icons into S3a icons.
               Copy icon into clipboard, apply ENLARG filter to
               clipboard, and insert into new bitmap.
     REDUCE  : Reduce by factor 2-8.
     ROTATE  : Rotation 90, 180, 270 degree

  
Troubleshooting:
----------------

    Graphic applications are memory hungry.
    A black&grey PIC-image in screen size needs 19kb of memory.
    A RGB-PCX file in the same size can have up to 460k !

    Conversion needs also up to 10k for buffers.

    PIC images are limited to a maximum size of 64 kB by the system.

    When memory becomes short, strange things can happen. Usually an error message
    is given, but if memory becomes so short that even this is no longer possible,
    the system will panic. Free some memory by closing applications or deleting
    unused files on the internal disk and try again.


Documentation
-------------

An extensive help file is supplied.

A manual is provided in PSION-WRD format.









