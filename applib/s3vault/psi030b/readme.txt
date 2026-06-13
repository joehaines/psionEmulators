18/9/1998

Readme.txt for psi030.zip.   Copyright HCS 1998.


This package gives you access to Psion series 3/3a/3c/5 and Siena 
machines over a serial line.


Files in this package:

PATTRIB.EXE     - Utility to set/list attributes of files on the Psion
PCOPYFR.EXE     - Utility to copy files from the Psion to your PC
PCOPYON.EXE     - Utility to copy files within your Psion
PCOPYTO.EXE     - Utility to copy files from your PC to the Psion
PDEL.EXE        - Utility to delete files on your Psion
PDIR.EXE        - Utility to list files on a drive on the Psion
PMD.EXE         - Utility to create directories on your Psion   
PRD.EXE         - Utility to remove directories from your Psion 
PREN.EXE        - Utility to rename files on your Psion         
PTYPE.CMD       - Utility to type files from your Psion to the display
PWAIT.EXE       - Utility to synchronise processes
LNKS3.OPO       - The server application that runs on the Psion 3x/Siena
LNKS3A.OPO      - Same as LNKS3, except that it does not support the S3t
LNKS5.OPO       - The server application for the S5
PDALNK.DLL      - Protocol driver file
README.TXT      - this file


Files distributed with the package
----------------------------------

SYSRAM1.ZIP
SYSTINFO.ZIP

The files distributed with the package are included in the archive file.
They each have their own copyright statement and the one of this package
does not apply to those files. The files distributed with the package are
needed to run the LNKS5 program.


General rules
-------------

Above mentioned files (as distributed in this package) are copyrighted
by HCS (Huelsmann Computer Services). HCS has imposed restrictions on
the distribution and use of these files.
This is a test version to exchange data between OS/2 and your Psion.
You are permitted to redistribute the files under the condition that 
you distribute them together and in unaltered form.
You are not allowed to reverse translate or disassemble any of the
above-mentioned files.
No limit, implicit or explicit, is imposed on the number of machines 
on which you may use this software on.



Installation
------------------------------------------------------------------------

Psion Server installation
-------------------------

Different servers exist for different platforms. For the Series 3, use 
the LNKS3.OPO server. For the Series 3a, Series 3c or Siena, use either 
the LNKS3.OPO or LNKS3a.OPO server. You may want to use the LNKS3a.OPO
server because it takes full advantage of the display capabilities.

Installation of the LNKS3(a) server:

 * Open a connection to your Psion as usual.
 * Copy the file LNKS3.OPO or LNKS3a.OPO to the \OPO directory of 
     any drive of the Psion.

After the system screen has been updated, program will appear under the
OPL Run icon.

Installation of the LNKS5.OPO series 5 server is more complicated. In 
order to be able to use the server, in addition to the server itself,
the files SysRAM1.OPX (from sysram1.zip) and SystInfo.OPX (from 
systinfo.zip) have to be installed.
If SysRAM1.OPX is not already on your Psion you have to copy it to your
machine to the \System\OPX directory on your C or D drive. In case the
\System directory (also known as System folder) is hidden, you will have
to make it visible with the Tools | Preferences menu option in the 
system screen. In case the OPX subdirectory does not exist, you will 
need to create one.
The SystInfo.OPX has to be installed using the SIS-file installation 
routine.
To do so, the Add/Remove control panel icon already must be installed on
your Series 5. If you don't have the Add/Remove control panel icon, you
can download the software (which is free) for it from the Psion web site
(www.psion.com).
Copy the SIS file to your Series 5 (Psion recommends the "\" folder).
Select the file in the system screen and press enter. This starts the
installation of the OPX. Follow the instructions on the screen.
The SIS file will be deleted when installation is completed 
successfully, to preserve disk space.
As a final step, you will have to copy the server-application to a 
directory on your Psion. Running the server from C has shown to result in
slightly faster file transfers.
                      

PC client installation
----------------------
Assuming you put the files in the directory C:\Psion, you will need to
make sure the text ";C:\Psion" (without the quotes) is part of the 
LIBPATH= statement in the CONFIG.SYS file. Alternatively, move the file
PDALNK.DLL to a directory which is already in the LIBPATH= statement.

All utilities assume that your Psion can be accessed through COM2. If 
your Psion is connected to another COM-port, then you have to make sure
the environment variable "PSIDEV" (without the quotes) points to the 
right port. You can do this by using the SET command either in the 
CONFIG.SYS or on the OS/2 command line. For example when your Psion is 
connected to COM1 you could insert the next line into your CONFIG.SYS:

       SET PSIDEV=COM1

If you have more machines, each connected to a different port, then it
is suggested to set this variable per OS/2 session. This way you can 
specify which machine to access.



Program description
------------------------------------------------------------------------

LNKxxx.OPO
----------

All servers are basically the same. They all have the same command menus
and short-cuts. There are 3 menus:
  * File
  * Port
  * Misc

The "File" menu has only one item: "Exit". As with all applications, 
this item is used to close the application.
The "Port" menu also has one item, but this item is not the same in all 
cases:

  * "Close" when the COM-port (on the Psion) is open;
  * "Open"  when the COM-port is closed or unavailable.

If you want to use the port temporarily for some other task, such as the
built-in link, you can close it without leaving the application: select
"Close" in the "Port" menu.
If the port is closed (or unavailable, such as when the link pod is not 
plugged into your S3), the text of the "Connected"-line in the status 
window is "N/A" (not available). In this case, the "Port" menu gives the
"Open" option. Use this option when you want to use the link program to
connect to the PC-client utilities again.

The Misc menu has one item. It allows you to pop up an about-box which
contains some information about the program and its version.

When the program-status is "not-connected" the current-line-speed item 
indicates 9600. This allows the client utilities to adapt their speed
to the maximum the server supports (which is different for every 
platform).

When the program-status is "connected", no keyboard input or short-cuts
are supported. All input is ignored, since the server cannot handle both
menus and transfers at the same time. Close the link when you want to 
use the menus.

REMARK: On the S5 the Exit-shortcut is Ctrl-e, whereas on the S3x it is
Psion-x. This is to adhere to standard application convention on the 
different platforms.


General comment on the Pxxxxx utilities
---------------------------------------
On successfull completion the exit value is 0. The meaning of other exit
values can be found at the end of this document.
The utilities have a 2 common parameters:

/!q when specified, no progress information is written.
/!v when specified, no copyright and version information is written.


The PATTRIB utility
-------------------
This utility can be used to show or change the attributes of a file or 
a group of files. It works similar as the OS/2 attrib command.

The OS/2 ATTRIB command supports one option: /S. This version of PATTRIB
currently does not, but future versions will.

REMARK: On the S5 there is no support for the "A" (Archive) attribute.


The PCOPYFR utility
-------------------
This utility can be used to copy a file located on your Psion to the 
OS/2 PC. This is a 1-1 binary copy, which means that there is no file 
format or code page translation on the data file.

Wildcards are not (yet) supported in this version. Neither does it 
support the 1 parameter invocation the OS/2 equivalent does.


The PCOPYON utility
-------------------
Currently neither wildcards nor filename completion are implemented.
This means that both the source and destination filename have to be
fully specified.

The PCOPYTO utility
-------------------
This utility is the same as the PCOPYFR utility, except that it copies 
files to your Psion. The two parameters have the same order here: 
source first, then destination; resulting in the name of the file on the
PC coming first, followed by the name of the file on the Psion.



The PDEL utility
----------------
This utility can be used to delete a file located on your Psion.
One argument is necessary: the name of the file you want to 
delete. For example:

PDEL b:\opo\lnks3.opo

This statement will remove the file lnks3.opo from the opo directory on
the right drive of your Psion Series 3x.

Wildcards are supported, but BE CAREFUL: an undelete, as there is for 
OS/2 or DOS, is not (yet?) available for the Psion. Once deleted, there
is (almost) no way of restoring the file.



The PDIR utility
----------------
This utility lists the files and directories in the source that you
pass on its command line. All OS/2 DIR options are supported, as far 
as they apply to the Psion.


The PMD & PRD utilities
-----------------------
These utilities handle directory creation and deletion (PMD and PRD
respectively). They take one argument: the (fully specified) name of
the directory to be created or removed. When a directory is NOT empty,
you will get an ACCESS DENIED error.


The PREN utility
----------------
This utility renames a file on your Psion. It takes 2 arguments. The 
first is the name of the file you want to assign a new name. The 
second argument is the name which the file should be assigned.

The utility can also be used to move files as it supports renaming 
across directories.


The PTYPE utility
-----------------
This is an OS/2 command file front to PCOPYFR to be able to use it as 
the OS/2 TYPE command. Its output can be routed into the MORE utility
in order to paginate the output.


The PWAIT utility
-----------------
This utility can be used to synchronise processes that use the link, 
or to wait for the completion of a current operation. When invoked,
it will determine if any of the PSI utilities is running. If there is,
PWAIT will wait for its completion and return. Otherwise PWAIT returns
immediately.


Thanks, special thanks and how to reach me
------------------------------------------------------------------------

My thanks go to the helpful fellow programmers in the 
comp.sys.psion.programmers newsgroup.

And special thanks to:
Simon Kriztian for writing some S5 specific code and providing 
                    a lot of S5 information.
Theo de Groot for rigorously testing the PSI utilities


How to reach me
---------------
Where you can reach me:

e-mail: erikh@bitsmart.com which is currently forwarded to
        f.w.h.hulsmann@rcondw.rug.nl
http://www.bitsmart.com/erikh/PSI.html which is currently forwarded to
http://www.oprit.rug.nl/hulsmann/PSI.html


Thank you for reading the documentation,

Erik Huelsmann
HCS


PROBLEMS WHICH I AM ALREADY INFORMED ABOUT:

* the Psion may switch off, inspite of the fact that the link is on.
* the series 5 server does not support the Archive attribute.
* PAttrib does not verify if the attribute was actually set.
* Curently no error checking is performed during file transfers
* An english Psion S3a (ROM version 3.25F) did not report its serial
   capabilities correctly, limiting it to 9600 baud. If you experience
   the same problem, please report, so that I can know that I need to
   try to fix this. (The guy that reported the problem does not need
   it to be fixed, since he does not use his S3a.)


REPORTING BUGS:

Please report bugs (and requests) to:

ErikH@bitsmart.com
In all cases please don't forget to mention:
* the version of OS/2 you are running;
* the series number (S3(a/c)/S5) and ROM version of your Psion;
* the COM driver you are using (COM.SYS/SIO.SYS);
* fixpacks applied to the system
And any other information that seems relevant to you.

Please try to describe as precicely as possible how to reproduce 
the error.

