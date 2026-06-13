Clock5   v1.81  (14th October,1999)
======================================

Clock 5 is a Series 5 / 5mx / Geofox / Ericsson MC218 password-protected
screensaver - much like those available for the Mac and Windows.  Of
course you don't really need a screen saver as such for your Psion /
'fox / Ericsson - why would its screen need saving?  With this in mind,
I thought it should at least do something useful - like display a large
clock perhaps...

The idea was originally based on Dan Comiskey's 'Clock' program for
the 3a/c.  There are quite a number of enhancements planned for future
releases so visit Pscience5 at:-
http://ourworld.compuserve.com/homepages/martin_guthrie
ocassionally to check for updates and more details.

Installation
============
Installation is via the now standard .sis file method.
There are 6 files:-
Clock5.sis			(Clock5's installation file)
Alarm.sis			(Symbian's OPX file)
SysRAM1.sis			(Symbian's OPX file)
Systinfo.sis			(Symbian's OPX file)
ReadMe.txt			(this file)
Changes.txt			(changes since the last release)

Clock5.sis, Alarm.sis, SysRAM1.sis, and Systinfo.sis can be installed on
your machine either by using Psion's EPOC install program (as supplied with
PsiWin 2.1/2.2/2.3) or by using the Add/Remove icon in your Psion's control
panel if you've got (say) Email already installed on your machine.  If the
latter, just copy the .sis files onto your Psion and either find them using
the Add/Remove icon in your machine's control panel or just double-click on
them to auto-run the install routine.

During the installation of Clock5, you will be given the option of installing
voice files.  These enable Clock5 to be used a speaking clock when in its
'Sweet Dreams' mode.  (NB: The machines Owner Information screen must be switched
off for this feature to work).  The files are quite large however (~100Kb) so
there's an option not to install them.  Clock5 will work without them minus
this particular feature.

The Clock5 icon should now have appeared in your Extras bar ready to run.

If you want to add your own user-defined logos to the standard ones built-in
to Clock5, just create an additional directory called 'Logos' in the new
System\Apps\Clock5\ directory (on whichever drive you specified during
installation) and add .mbm files into it.  They'll be found automatically by
Clock5.  There are some sample logos available on the Pscience5 web site.

"Incompatible opx version" during installation
==============================================
If you should get this error when you're installing Symbian's OPX files, then one
of two possible things could be the problem:-

1) You've got an older version of Systinfo.opx or Sysram1.opx on your Psion causing
conflict with the new one.  Symbian released new versions of these opx files for
ER5/5mx compatibility.  The new files will work fine on the Geofox/S5 - but all of
the machines appear to get confused if there's a copy of the old opx files on the
machine somewhere as well as the new one(s).

Hence I'd recommend checking for old versions of sysram1.opx and Systinfo.opx either
in c:/system/opx/ or d:/system/opx/ before installing Clock5.  BTW, old apps. will
work fine with this newer version of these opx files.

2) That you've got a program already running that's using Sysram1.opx or Systinfo.opx
when you try to install Clock5 (e.g. Macro5).  Close down running programs before the
installation.

Best regards,

Martin Guthrie
martin_guthrie@csi.com