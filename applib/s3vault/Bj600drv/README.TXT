        Printer Driver for the Canon BJ600 Colour Bubblejet Printer
        ===========================================================

     This ZIP file contains BJ6X001a.WD and BJ6X001a.WDR, as well as this
ReadMe.TXT file.  The .WDR file is a printer driver to be placed in the \WDR 
directory of the Series3 or 3a which needs to print colour on a Canon BJ600c
(or similar) printer.  Once the file is in that directory, any new application 
created/opened afterward will have the 'Canon BJ600 colour1' printer added to 
the printer selection dialog of applications using the built in print manager.

     In order to print colour, several of the style code have had to be
replaced by colour settings.  Here are the colours available in the supplied
driver, and the styles you need to set in the text to produce the colours :

         Italic -      Blue
         Subscript -   Cyan
         Superscript - Red

     The base colour for this driver is black.  In order to change the base 
colour you will need to modify the driver, such that at the end of each 
coloured section, the printer reverts to the base colour you require.  This 
simply means that you need to insert a 'change colour' code into the
'Italic Off' etc. sequences.

     The .WD file in this archive is the source code for the printer driver,
which is modified from the Epson FX driver.  To recompile it, use the 
WDTRAN program available from Psion.  This may also be available on
bulletin boards such as CIX or CompuServ.

     The driver also supports the Script and Orator fonts available on BJ600,
although the character spacing has not been checked, so justified text may 
not have a straight right margin.

     While I have taken reasonable steps to ensure that this printer driver is
free from errors, I cannot be held responsible for incorrect operation or any
loss arising from the use of this driver or its source code.

     You may find it necessary to change the default printer emulation such
that the priinter accepts the Epson command set.

NOTE: Due to the way in which Word formats its text, you may find that colours
don't always work the way you would expect.  For instance, you will need to 
re-assert the colour of any text which spans over a page break.  Using one
colour within another is also a problem area.  To print Red text within Blue,
you wil need to highlight both red sections, rather than the whole block
followed by highlighting the middle section in blue.

Unless/until specified otherwise, this driver may be freely distributed,
provided the full contents of this archive is distributed at the same time.

Simon Rea 25/06/95
(perseus@cix.compulink.co.uk)
