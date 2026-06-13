                         SERIES 3 PRINTER DRIVERS 
                         ========================

PRINTER DRIVERS FOR THE SERIES 3
--------------------------------

This directory contains printer drivers and associated files for
extending the range of printers that you can use with your Series
3/3a. You can install them into a \WDR\ directory on any disk on your
Series 3/3a. The next time you then use a `Print setup' option, you
will find the extra printers available in the dialog.

HP2.WDR - printer driver for the Hewlett Packard Laserjet II.

HP2P.WDR - printer driver for the Hewlett Packard Laserjet IIP/IID.

HP3.WDR - printer driver for the Hewlett Packard Laserjet III.

HPDJ.WDR - printe rdriver for the Hewlett Packard Deskjet.

HPDJ500.WDR - printer driver for the Hewlett Packard Deskjet 500.

HPDJ550.WDR - printer driver for the Hewlett Packard Deskjet 550.

BJ10EX.WDR - printer driver for the Canon BJ-10 (Bubblejet 10)
printer.

BASIC.WDR - basic printer driver for Epsom postscript printers.

CIT_PN48.WDR - printer driver for the Citizen PN48 printer.

PS.WDR, PS.INI, PS.DYL - printer driver (and other necessary files)
which supports a basic set of fonts for any Postscript printer.

PSAPPLE.WDR, PSAPPLE.INI, PSAPPLE.DYL - printer driver (and other
necessary files) for the Apple Laserwriter II series. These support a
wider range of fonts than the other Postscript drivers. You may find
that you can use these files to drive many other Postscript printers
that also have a wider range of fonts.

Installation
------------

You can choose to install single drivers, or all of the drivers.

1. Display the RCom command prompt (on Windows-based PCs use the RCom
icon; on DOS-based PCs enter the appropriate command to run RCom).

2. Make sure that the current directory is the one in which you
installed RCom. If RCom is located at the path c:\psrcom, for
example, you can type CD C:\PSRCOM to make this the current
directory.

3. Make sure that the `Remote link' option on the Series 3/3a System
screen is set to `On'.

To install a single driver:

4. Type PRINST followed by the name of the printer driver you wish to
install, then the new (virtual) PC drive letter that is set up to
represent the Series 3/3a drive on which you wish to install that
driver. 

For example, to install the driver for the Hewlet Packard Deskjet
into the \WDR directory on the internal drive you would type
PRINST HPDJ.WDR I:

You can also use wildcard characters if you wish to install more than
one driver at a time. For example, to install the HPDJ500 and HPDJ550
printer drivers you can use the following command:
PRINST HPDJ5*.* I:

To install ALL of the printer drivers:

4. Type PRINST *.* followed by the new (virtual) PC drive letter that
is set up to represent the Series 3 drive on which you wish to
install the drivers. For example, type PRINST *.* I: to install them
into the \WDR directory on the internal drive.

PRINST runs the installation batch file PRINST.BAT which copies the printer 
drivers that you have requested into the \WDR directory on the specified 
Series 3 drive. 

Note: Don't confuse the \WRD and \WDR directories - \WRD\ is the
directory where Word processor documents are stored, while \WDR is
where the Word processor keeps special files of various kinds
(templates, and add-on software or printer drivers).



