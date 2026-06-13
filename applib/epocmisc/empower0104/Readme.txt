Installation - Version 1.04
===========================

What's New
----------
- Fixed a bug in initial letter searching of expenses.
- Fixed a bug which caused problems with email addresses over 32 characters long.
- Changed the add/edit dialogs to save on Ctrl-S rather than Enter.  i.e. keyboard shortcuts now work from Notes dialogs.

Frequently Asked Questions
--------------------------
- Can I import my Data database?
  No.  Importing a non-relational database into a relational database quite complex and I am still working on a method that is simple, effective and fast.  I hope to include this in a future version.

- Can I use the existing Import facility to import my data?
  Yes (and no).  Import reads an ASCII text file which is formatted to be simple, fast and small.  Anyone who wants to write a utility that creates Empower-compatible Import files is welcome and I will happily distibute the file format (just ask).  However, I cannot adequately support this so I do not recommend this course of action unless you are a comptetent programmer.

- Can I synchronise Empower with Outlook?
  No.  I do not have the resources to write this and am unlikely to ever do this.

- Can Empower be changed to do something I want?
  I am always willing to hear suggestions for extra functionality or changes to existing functionality.  I acnnot promise to please everyone but good ideas are always welcome.

Required Files
--------------
Empower is distributed in five .SIS files which contain:

- Empower application;
- Window5 library;
- RMRData OPX, RMR Buffer OPX, Message OPX.

All three RMR OPXs are also available from the RMR web site.  The OPX authors' rights are acknowledged.

Installation Instructions
-------------------------
1.	To install Empower you must have PsiWin 2.1 installed already.  This is available from the Psion web site.
2.	If you already have Empower installed then deinstall it from the Control Panel on your Psion;
3.	If you already have Window5 installed then there is no need to deinstall it.
4.	Use Empower.sis to install the application.  You will be asked if you do not have Message Suite on your Psion - answer YES to install the non-Message Suite version of Empower;
5.	Use Window5.sis to install the windowing library (if not already installed);
6.	Install the OPXs if you have not done this already using the SIS files provided.  If you do not have Message Suite installed then you do not need to install the Message OPX.

Empower should now be visible on your extras bar.

Trouble Shooting
----------------
If you experience problems when installing Empower, please email the author, Ian Leake, at empower@ianleake.demon.co.uk.  Remember to give as much information about the problem as possible (e.g. error messages, etc.).

Known Problems
--------------
- If you change between tab sheets using the keyboard shortcuts (e.g. ) then the tabs do not change even though the correct entries are displayed.


News & Patches
--------------
For latest news please visit http://www.ianleake.demon.co.uk/.