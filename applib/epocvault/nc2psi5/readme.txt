Instructions for setting up NC2PSI5
===================================

What is NC2PSI5?
---------------

NC2PSI5 stands for Netscape Calendar to Psion 5 conversion.  The program will
take an ASCII file produced by Netscape Calendar's Export function and create a
Psion Series 5 Agenda file.  The process is one-way, from Netscape to Psion.  There
is no process to send a file from the Psion to Netscape Calendar.  No syncronisation
is available.  The program is released as freeware - don't expect it to be elegant 
or fully-featured ;-)  I wrote it purely for my own use - if it helps you then great, if
not then I'm afraid you'll have to look elsewhere.

Objective of the program
------------------------

The objective of the program is to help anyone who is connected to a network which uses 
Netscape Calendar to maintain multiple diaries.  The user can download his/her Calendar
diary and have it available on the Psion Series 5.  Since no syncronisation is possible,
there are some constraints.  New appointments gathered "on the run" with the Psion will
also need to be added into Netscape Calendar.  Only information which is available in
Netscape Calendar's export file can be downloaded into the Psion.  However despite these
constraints, I find that it's very useful to transfer my diary on a daily basis - either
first thing in the morning or last thing at night - and have all the information available
in the Psion.

Setting up NC2PSI5
------------------

To install, double-click on nc2psi5.sis in Windows Explorer or transfer the file to the
Psion and install it from there.  If you have installed the files successfully, then you
should see the familiar Netscape symbol appearing in your Extras bar.

Using a flash disk
------------------

For reasons of speed and to prevent write failure with low batteries, I recommend that 
you keep your Agenda file on your c:\ drive and not on the flash disk.

Running Netscape Calendar Export
--------------------------------

First you must export your Netscape Calendar file.  From the File menu, choose the Export
Data option.  Under the Format section, choose the "File" radio button and select Ascii 
Tab delimited file from the drop down menu.  Then from the Period section, select the 
number of days that you want to export (my own preference is from 0 previous days to the 
next 90 days, giving about 3 months of entries).  Under People/Resources, your own name
should appear.  Under File Name you should delete the default name shown by Netscape 
Calendar and enter a name for your export file.  I use filename NC2PSI5.TRF.  Lastly,
press the Export button.

Netscape Calendar will now have created an export file which you will need to transfer to
your Psion using PSIWIN.  If you don't know in which directory Netscape Calendar has 
placed the export file then, still within the Export Data dialog, press browse - this 
should show you where the export file resides.

Transferring the export file to the Psion
-----------------------------------------

When you transfer the Ascii file, be sure that PSIWIN doesn't automatically convert your 
file to Psion Word (check the Psiwin literature if in doubt about how to do this).  You
want to do a straight copy from your PC to the Series 5 with no conversion.  If you have
called the Export file NC2PSI5.TRF, then place this file in Psion directory c:\.  An easy
way to do this operation is to right-click on the NC2PSI5.TRF file and select "Send
to Psion C drive".

Running NC2PSI5
---------------

Select NC2PSI5 from your Extras bar.  You will be presented with an initial screen.  By
default the program will look for the export file as C:\NC2PSI5.TRF and will create a
Series 5 agenda file called c:\documents\calendar.  You also need to select the correct
date format here - if in doubt then look at your exported file with a text editor to
see what default format Netscape has used.

On the second page, NC2PSI5 gives a number of options for possible inclusion in your 
agenda file.  The options should be self-explanatory - the best way to decide which options
you want to include is to run NC2PSI5 a few times with different settings and look at
the resulting Agenda file.  Note: your Agenda file must NOT be open when NC2PSI5 runs,
otherwise an error will result.

Summary
-------

The above steps may sound complicated, but in fact are quite straightforward with practice -
particularly since both Netscape Calendar and NC2PSI5 save most of your settings each time
you run the programs.

Enjoy the program.


Malcolm Bryant
July 1998, August 1999

malbry@ms.com

