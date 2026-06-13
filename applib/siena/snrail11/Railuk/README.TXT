Siena Rail UK  v 1.1
Copyright (c) 1998/99 Adrian Wymann

January 1999

Overview:

0. What's new in v1.1 ?
1. General Info
2. Installation procedure
3. Installing the application
4. Running the application
5. Troubleshooting
6. Additional remarks
7. Disclaimer
8. History


0. What's new in v1.1 ?

* Three new items of motive power have been added: two EMUs (Cl 442 and Cl 465) and one locomotive (Cl 66). In addition, new and better pictures have been added for the Cl 58 and the Cl 60.

* The information regarding the numbers of locomotives on the books has been updated and is now correct as of December 8th 1998.

* Apart from some modifications of the OPL program code, Siena RailUK now supports full fledged error handling. Hopefully, you'll never get to see any of this, but if you do, you'll now be able to note down relevant error information and look up possible ways of solving the problem in section 5 "Troubleshooting" (below).

* The application has a new icon.

* And finally, the readme.txt file was extensively reworked, so even if you've already made your way through this file in connection with v1.0, a second look now might well be worth the time.

 
1. General Info

Siena Rail UK is a reference-type application featuring selected technical information on most of the major motive power on British rails. Locomotives, both diesel and electric, are the main feature, but multiple units, again both diesel and electric, have not been forgotten. In addition to the technical data, every individual type of motive power covered is accompanied by a picture illustrating what all the figures and numbers add up to in visual terms.

Siena Rail UK also gives the number of units listed of each class. These figures represent the standings of January 1st 1998 for multiple units and August 30th 1998 for locomotives. These figures do not state the number of units actually in service, but rather the total number of units in possession of the current rail operators - especially with classes which are fading away quickly now (e.g. the Cl 33s), the number of units in actual operation may change from day to day and be much lower than the number of units listed (some of which may be stored and could either be returned to service or sent to the scrapyard).  


2. Installation procedure

Installing Siena Rail UK v 1.1 follows the usual standard procedure. Unzip the ZIP either on your PC/MAC or your Siena using one of the available tools for this (e.g. WinZip, PsiZip, etc.). Remember to keep the directory informations. Following all of this, your Siena should have a new directory RAILUK, where all the files of the application are placed (i.e. snrail11.opa, the PIC files, and readme.txt). If for some reason you only have the extracted files, just create a directory RAILUK and copy all the files into this directory.


3. Installing the application

The short key for this is <PSION+i>, but if you have any doubts or are actually doing this for the first time, it might be a good idea to check the Siena manual.


4. Running the application

Following installation, you will find the application's icon on your system screen. By highlighting this and then pressing <ENTER>, you will start Siena Rail UK. Moving around should be easy and straightforward, using the menu and the dialogue options. Pressing <ESC> should get you out of any kind of trouble you're likely to encounter by taking you back to the previous step. Exiting the programme is accomplished by either selecting this option from the menu, or by using the shortcut <PSION+x>.


5. Troubleshooting

If there is a problem with Siena RailUK, you will be told what is causing the hickups in the language of your Siena's operating system. So far, only two possible problems have come to light. Details for these are given below, which should help you overcome any difficulties. If all else fails, don't hesitate to contact me via e-mail (adrian.wymann@bfa.admin.ch).

5.1. "No system memory"

Pictures are heavy burdens in terms of disk space. Just one bitmap file of Siena Rail UK takes up 4'820 bytes - and the application contains 33 such bitmap files. Hence the large size of the package: If you want to install the complete Siena Rail UK, you will need 201 KB of free disk space. However, the program only requires four PIC files (pfm_logo.pic, snrail11.pic, bg.pic and bg2.pic) to run properly - all others may be deleted. If a PIC file is deleted, the program can still access all the data, but (obviously) no picture will be displayed. This means that users can cut down considerably on the amount of disk space used by the program by deleting those PIC files which they do not want (or do not like) from the M:\railuk directory. The program thus allows for some amount of "customising" if required. A cut-down minimalist-graphics (i.e. no pics of locos and multiple units) installation will only take up 53 KB. The program is therefore clearly more suited for the 1MB Siena model and has not been tested on a 512kB model. 

If you should run out of memory while running the program, your Siena will exit the program. By doing so, it also frees all that clogged-up system memory - so you just hit the icon, and off you go again...

5.2. "Wrong file type"

The reason for this error is not quite clear. The file requested by the program (e.g. Cl73.pic) is correctly located in the directory M:\railuk, but the program cannot open the file. One possible explanation is that the file has been "touched" by some other process writing to the internal disk, even though the file is actually "write protected".

To solve this problem, extract the file in question (e.g. Cl73.pic) from the ZIP file from which you have originally installed the program and copy it into the railuk directory on your Siena, thus replacing the file which is causing the problem. 


6. Additional remarks

Siena Rail UK v1.1 is (C) Copyright 1998/99 by Adrian Wymann. The program is freeware and as such may be distributed freely by anybody, provided that it is supplied complete (including this file) and unaltered. Any feedback is appreciated.


7. Disclaimer

This application comes with no performance warranty.  By using the program, the user agrees to take no legal action (both now and in future) against the author for any damages suffered, whether direct or indirect, as a result of such usage.


8. History

10/98   v1.0 released 
01/99   v1.1 released

The program was originally released as "British Railways Motive Power" (and later "RailUK") for the Series 3a/3c in July 1998.
