---------------------------------------------------------
FlFinger - Finger client for EPOC
(C) 1999, 2000 David Rushall
All rights reserved

---------------------------------------------------------
INTRODUCTION

FlFinger is a simple client for the TCP/IP 'finger'
protocol (RFC1288).

Using FlFinger you can collect information about the user
of an email address, if the user's server provides the
service.

Some servers may provide other information via 'finger',
such as the service status information provided for
Demon Internet subscribers.

Features include:

  o  Simple interface.

  o  Address history.

  o  Copy-and-paste responses into other applications.

---------------------------------------------------------
INSTALLATION

NOTE: You must have installed and configured Internet
     (TCP/IP) access (e.g. the Message Suite) before
     installing this software. On more modern machines
     (e.g. Series 5mx and Revo) this is installed as
     standard. Consult you machine's manual for more
     information.

To install this software on your machine:

  1) Close ALL applications on your EPOC device.

     If you do not close all applications before
     installing the two OPX *.sis packages in the
     next step, you may have problems with OPX versions
     later.
  
  2) Install the two OPX SIS files, first:
     
          CDescriptor.sis
          CSocket.sis

     If you get a message saying that a later version
     than the one supplied has already been installed on
     your machine then DO NOT allow the older version to
     overwrite the newer.

  3) Install the main application SIS file:
     
          FlFinger.sis
     
     If you get a message that one of the two OPX SIS
     files has not been installed at the correct level
     then return to the previous step.

  4) Finally, look in the "Extras" draw on the machine
     for the new "FlFinger" icon, in the normal manner.

NOTES:

  *  Please consult the manuals for your device for
     details of installing applications packaged in SIS
     files. You will require either a PC/Mac running your
     EPOC connect software (i.e. PsiWin 2 or equivalent)
     or the "Add/Remove" icon on the control panel of
     your EPOC device. Double-clicking on the SIS file
     either on the EPOC device or on the PC/Mac connected
     to the EPOC device should do the trick.
     
  *  The supplied OPXs are for ARM-based machines, such
     as the Series 5, Revo, Osaris, Geofox 1, Series 5mx,
     Series 7, etc. If you wish to run this application
     on the Windows EPOC emulator then please contact the
     OPX author, Keith Walker (web address below), for
     the appropriate versions.

  *  OPXs are special resources that may be shared by
     many different applications on the EPOC device. If
     these applications do not deliver the OPXs correctly
     in the standard SIS files, they may cause problems
     with other applications when they are installed, or
     even uninstalled. If you find that this application
     fails to start after installing or uninstalling
     another application, perhaps reporting a missing or
     invalid OPX file, please try reinstalling this
     application using the instructions above. This
     should correct the problem.

---------------------------------------------------------
UNINSTALLING

If you have finished with this software you can remove
the application as follows:

  1) Make sure the FlFinger application is not running.

  2) Using the "Add/Remove" icon on the EPOC control
     panel, remove the "FlFinger" application.

After you have completed these steps, FlFinger will have
been removed from your machine's memory and "Extras" bar.

NOTE: I DO NOT recommend removing the small OPX files,
     installed by the additional supplied SIS packages, 
     as they may also be in use by other applications.

---------------------------------------------------------
BASIC USE

If you are familiar with using other EPOC TCP/IP
applications (e.g. the Message Suite) and have used
finger clients before, you should find FlFinger easy to
use.

Simply run FlFinger, select 'Open connection...' from
the main menu, enter a valid email address (or select
one from the history of recently used addresses) and
FlFinger will do the rest for you.

Anything you can't work out should be documented in the
online Help available from the application menu.
Inexperienced users are advised to browse through this
file when they first run FlFinger.

---------------------------------------------------------
SERVING FINGER REQUESTS

FlFinger is a finger client only, so it cannot respond
to finger requests (perhaps from another machine).

However, you can use the supplied 'flfingerd.opo' sample
program to serve finger requests from your EPOC machine,
and so try FlFinger even when you are disconnected from
the Internet.

Simply run the 'flfingerd.opo' program and then use
FlFinger to send a request to 'someone@localhost',
or 'someone@127.0.0.1', where 'someone' is any valid user
name.

If you wish, you can customise this OPL sample to serve
your own message, and even use it to serve requests from
other machines when you are connected to the Internet.
See the 'flfingerd.opl' source file for details.

---------------------------------------------------------
CHANGES

Version 1.3

  *  Pioneering 'ping' probe.
  *  Fabulously faster 'finger' fetching.
  *  Long format finger requests.
  *  Improved display on Osaris and colour machines.
  *  Colour application item.

Version 1.2

  *  Improved Revo support;
  *  "Disconnect from Internet" menu item;
  *  Automatic response dialog preference setting;
  *  More attractive interface;
  *  Installable on drive E:
  *  Sample finger server included;
  *  Minor enhancements.

---------------------------------------------------------
DISCLAIMER AND COPYRIGHT

  1) FlFinger is freeware and copyright; it is not
     public domain. The rights to this package remain the
     property of the author.
     
  2) You may use this software, free of charge, for an
     indefinite period and at no obligation to the
     author.
     
  3) Permission is given to publically distribute this
     software provided the distribution contains all, and
     only, the unaltered files of the original
     distribution, the ownership of the rights to this
     software is clearly stated and any fee charged is
     extremely nominal.
     
  4) Use of this software is entirely at the user's own
     risk. The user must accept responsibility for any 
     direct or indirect loss or damage arising from the 
     use (or misuse) of the package. This software is 
     supplied "as-is".

  5) Note: The OPX files employed by this software (and
     included in this distribution) are the copyright of
     Keith Walker. Please see his WWW site,

     http://www.starship.freeserve.co.uk/

     for more details.

---------------------------------------------------------
CONTACTS

If you have any problems, comments or suggestions about
this software, you can contact me at the following email
addresses:

     dave@freepoc.de

     dave@piecafe.demon.co.uk

This application is distributed in association with
FreEPOC:

     http://www.freepoc.de/

For the latest news of this an my other software, also
visit:

     http://www.piecafe.demon.co.uk/

---------------------------------------------------------
Dave Rushall, 2000
