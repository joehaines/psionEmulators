NP2AGN Plugin For Notepad - Version 1.1
=======================================

1.0 Introduction
----------------

Np2Agn is a plug-in for PelicanSoftware inc's Notepad application for the Psion Series 5.

It is intended as a quick way of copying dated items from Notepad to an Agenda file. For details
of which items are copied, please see section 2.2.2 below.

It is offered free for use by anyone who wants it. The author will not be held responsible for
any lose of data that may arise from its use.

IMPORTANT - IF YOU HAVE VERSION 1.0 READ ON
If you are using version 1.0 of this plug-in then 
a) Stop immediately
b) Reinstall the new version of the plug-in and the Agenda2.OPX

Failure to do so may expose you to the CBASE bug fixed in v1.1 If this happens to you it is
unlikely that you will be able to use your Agenda file again.

YOU HAVE BEEN WARNED :-(


2.0 Installation and Use
------------------------
2.1 Installation:
-----------------

Install the contained NP2AGN.SIS to the drive on which you originally installed Notepad.
This will add the file NP2AGN.PLG to your SYSTEM\APPS\NOTEPAD\PLUG directory.

The installation will also install AGENDA2.SIS. This provides a set of OPX routines to allow easy access
to Agenda files.

2.2 Use:
--------
2.2.1 Setting Up:
The first time Np2Agn is run it will prompt you to pick an Agenda file.
From then on all items will be copied to that Agenda file.
Should you wish to copy to a different file, you will need to delete 

	C:\SYSTEM\APPS\NOTEPAD\PLUG\NP2AGN.INI

Next time you run Np2Agn, you will again be prompted to select an Agenda file to use.

2.2.2 Items copied:
Only active (i.e. unfinished) dated items are copied.

Such items are only copied if:
 a) they START on TODAY (where TODAY is the date when the plug-in is run)
or
 b) they END on TODAY and have no start date (when they are, in effect, items that start & end
    on the same day)

2.2.3 Location of copied items:
Items are copied to a ToDo list titled "Notepad". If this todo list does NOT exist, it is
created.

2.2.4 Format of copied items:
Items are copied with the same priority as they have in Notepad.

They will start and end on the same day as specified in Notepad.

The format of the copied item is:

	[<owner>] <Title>

where <owner> is the name of the owning notepad, and <Title> the item's title in the notepad.
If the item does NOT have a title, then the first 20 characters of the note are used.

Example:
A note "Call Joe Bloggs" in a notepad called "phone calls" would be transferred as

	[phone calls] Call Joe Bloggs


3.0 Acknowledgements
--------------------

Thanks are first due to Mark Esposito for creating such a good package in the first place,
and for then making it even better be allowing users to add to its functionality. 

Secondly, thanks are due to Andy Clarkson for his Agenda2.OPX which is used to do the writing
to Agenda. Thanks to him also for his excellent OPP development environment. It is a pity
that Psion could not have come up with something like this as standard.

Mark and Andy are also to be thanked for tracking down the CBASE bug in version 1.0 and 
giving me a fix for it.

Finally, thanks again to Mark for kindly packaging it all up in a SIS file.

4.0 Contacting the Author
-------------------------

Until I have my web page up and running (and who knows when that will be with all the other
things I have to do this year!), contact will have to be via email.
Please address all comments, queries, bug reports or suggestions for improvement
to:

	Simon.A.Perry@pemail.net

If you want me to send out updates automatically, then please send me your
email address and I will forward new versions as soon as available.

5.0 Possible Enhancements
-------------------------

At present Np2Agn copies all items that fall on the relevant day.
The following are some possibilities for enhancements. If you have your own suggestions,
please let me know:

* Fix bugs
* Filter by priority
* Filter by project
* Copy over alarm information
* Allow the user to specify properties of the created "Notepad" todo list (e.g. control
  time slot, order, use a different title etc.)
* Allow copmpleted items to be copied.
* Filter by notepad

6.0 Known Problems
------------------
None

7.0 Version History
-------------------

1.0	Initial release
1.1	*After np2agn has run, it now correctly sets focus back to Notepad window.
	*SQL improved to increase speed of copying (although I guess this will only be noticeable 
	if you are copying LOTS of items ;-) )
        *Fixed CBASE error that was crashing some Agenda files. (Thanks to Mark E. & Andy C.)


Hope you find it useful.
Enjoy!