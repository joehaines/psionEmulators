                         THE RPRINT.EXE PROGRAM
                         ======================

The \RPRINT directory contains the RPRINT.EXE program. RPRINT allows a
Series 3/3a, MC, or HC (or any other serial-printing device) to print
to a printer which is connected to an IBM PC/XT/AT or compatible. The
PC must have a free serial port which is used to connect to the
Series 3/MC/HC. The printer may be connected to a parallel or serial
port on the PC, or be on a network.

Even if you can link the Series 3/MC/HC directly to the printer, 
there may be reasons why it is more convenient to use RPRINT:

(1) The printer is shared by other PCs - either using a multi-port
    printer buffer or a Local Area Network (LAN)- and it would be
    unreasonable to connect it directly to the Series 3/MC/HC.

(2) You do not wish to disturb the connection between the PC and the
    printer.

(3) You have already set up the serial connection to use RCom for
    file transfer to the PC.

(4) If your PC is connected to more than one printer, and you wish to
    be able to select which one the Series 3/MC/HC will use.

Note: it is also possible to print via RCom to a printer attached to
your PC. To do this, set your Series 3/MC/HC to print to a file, and
give the printer device on REM:: (such as REM::LPT1) as the `file' to
use. This method may be slower than using RPRINT - especially when
printing `justified' text from the Word processor on either computer.
However, RCom can correct transmission errors, whereas RPRINT can
only report them. Such errors may occasionally be caused by PC
add-ons, such as some network card drivers.

Using RPRINT on Windows-based PCs
=================================

1. Set the correct printing settings in RCom Setup - the `Printer
port', `Baud rate' and `Activity timeout'. (See the FURTHER NOTES
section later at the end of this document for more information about
Activity timeout.)

2. Physically connect the Series 3/MC/HC to the PC exactly as for
RCom.

3. Double click on the RPrint icon in the Windows Program Manager to
run RPRINT using the printing settings in RCom Setup.

4. Follow the instructions that follow according to whether you have
a Series 3/3a or MC linked to the PC.

5. Press Control-C when you have finished to exit RPRINT.

Note: See the FURTHER NOTES section at the end of this document for
details of how to suppress printer status messages.

Series 3 users:
---------------

Select the `Printer setup' option from the Special menu in the System
screen (the `Printer' option on the Series 3a). In this dialog, set
the `Printer device' line to `Serial'. 

Then press Tab on the `Serial characteristics' line and set the `Baud
rate' to the `Baud rate' for printing that you set in RCom Setup.

Select the `Remote link' option on the 'Special' menu and set 'Remote
link' to 'Off'.

Open the file that you wish to print.

Use the application's `Print setup' option to set the `Printer model'
and the page layout desired.

Use the `Print' option to print the file.

MC users
--------

Select the `Print Setup' option from the `Options' menu in the System
application to display the `Printer' dialog.

Set one of the configurations to output to Serial.

Click on the SET SERIAL... button and set the `Baud rate' to the
`Baud rate' for printing that you set in RCom Setup.

Make sure that the remote link is set to `Off'.

You can now print as if the printer was directly connected to the MC
by selecting the Print menu item in any application which can print.
As far as the MC software is concerned, it is printing to a Serial
printer.

Using RPRINT on DOS-based PCs
=============================

1. Physically connect the Series 3/MC/HC to the PC exactly as for
RCom.

2. Make sure that the DOS prompt, not the RCom command prompt, is
displayed and that the remote link is set to `Off' on the 
Series 3/MC/HC.

3. If the printer is connected to LPT1, the Series 3/MC/HC 3 is
connected to COM1, and you wish to use 9600 as the `Baud rate' for
printing and 2 seconds as the Activity timeout, you can now run RPRINT
on the PC by typing:

  RPRINT

If you do not wish to run RPRINT using the default settings, you can
use the following switches on the RPRINT command line:

/P to set the PC port to which the printer is attached
/C to set the PC port to which the Series 3/MC/HC is attached
/B to set the Baud rate for printing
/T to set the Activity timeout

For example, to print to a printer attached to the PC port LPT2 from
a Series 3 attached to COM2 at a baud rate of 4800 with an Activity
timeout of 4 seconds, you would type:

RPRINT /PLPT2 /CCOM2 /B4800 /T4

(Alternatively, you can edit the RCOM.INI file - stored in the
directory in which you installed RCom - from which RPRINT takes its
settings to include the settings that you wish to use.)

4. Follow the instructions that follow according to whether you have
a Series 3/3a or MC linked to the PC.

5. Press Control-C when you have finished to exit RPRINT.

Note: See the FURTHER NOTES section at the end of this document for
details of how to suppress printer status messages.

Series 3 users:
---------------

Select the `Printer setup' option from the Special menu in the System
screen (the `Printer' option on the Series 3a). In this dialog, set
the `Printer device' line to `Serial'. 

Then press Tab on the `Serial characteristics' line and set the `Baud
rate' to the `Baud rate' for printing that you set in RCom Setup.

Select the `Remote link' option on the 'Special' menu and set 'Remote
link' to 'Off'.

Open the file that you wish to print.

Use the application's `Print setup' option to set the `Printer model'
and the page layout desired.

Use the `Print' option to print the file.

MC users
--------

Select the `Print Setup' option from the `Options' menu in the System
application to display the `Printer' dialog.

Set one of the configurations to output to Serial.

Click on the SET SERIAL... button and set the `Baud rate' to the
`Baud rate' for printing that you set in RCom Setup.

Make sure that the remote link is set to `Off'.

You can now print as if the printer was directly connected to the MC
by selecting the Print menu item in any application which can print.
As far as the MC software is concerned, it is printing to a Serial
printer.

Printing to a file on the PC
----------------------------

You can also print to a file on the PC using, for example:

  RPRINT PRINT.LIS

Note that PRINT.LIS will be overwritten each time you print.

To test the connection without wasting paper
--------------------------------------------
  
  RPRINT CON

CON is the DOS name for the console (screen). When you then print
from the Series 3/MC/HC, you should see the output appear on the
screen of the PC.

FURTHER NOTES
=============

Activity timeout
----------------

The Activity timeout option is provided for use with printers that
are connected to the PC via a Local Area Network (LAN) where it is
necessary to close and open the print device between each print job.
It is used to specify an inactivity time-out in seconds. If there is
no printing for this period, the print device is automatically
closed.

If the Activity timeout setting is omitted, the print device is not
automatically closed. You can close the print device manually by
pressing any key on the PC keyboard, or by pressing Control-C to exit
RPRINT.

Printer status messages
-----------------------

If you wish to, you can suppress printer status messages.

On Windows-based PCs: highlight the RPrint icon in the Windows
Program Manager and select the `Properties' option from the `File'
menu. Add /Q to the end of the `Command line' line and press Enter.
The next time that you use the RPrint program, printer status
messages will be suppressed.

On DOS-based PCs: type the /Q switch at the end of the command line to
run RPRINT.

PRINTING PROBLEMS
=================

Premature closure of the print device
-------------------------------------

Cause: The Series 3/MC/HC are multi-tasking - while one application
is printing, you can carry on with something else. However, the
background printing can be held up at times, depending on the
processing requirements of the work you are doing. This can fool
RPRINT's inactivity timeout into thinking that the printing has
finished, causing it to prematurely close the print device.

Solution: If you experience this problem, consider increasing the
Activity timeout, or convert to closing the printer device manually
by pressing a key on the PC keyboard.

RPRINT reports printing problems
--------------------------------

Solution: Try reducing the `Baud rate' for printing on the PC and
Series 3/MC/HC to 4800.

