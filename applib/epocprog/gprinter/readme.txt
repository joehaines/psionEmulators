GPRINTER.OPX Version 1.10
Copyright 1998-1999 Twiddlebit Software
All rights reserved

CONTACT
-------

Note that this OPX is provided on an "as is" basis with no guarantee
of support.

For the latest version see the following www page:

	http://www.twiddlebit.com

INTRODUCTION
------------

Psion supply a PRINTER.OPX built into the EPOC32 ROM. This OPX allows OPL
programmers to print documents in a similar way to the built-in Psion
applications. This Psion supplied OPX provides standard page setup and print
setup dialogs and a print/print preview facilty.

The Psion PRINTER.OPX is explicitly designed for printing textual/bitmap
type data formated into paragraphs, i.e. normally associated with a word
processor. It is not suitable when you want to print vector graphics,
charts or tables where you need to precisely control the position of text
or vector graphics on the printed page. We have therefore written a
GPRINTER.OPX which is similar to the PRINTER.OPX supplied by Psion but
is designed for this alternative type of printing.

If you want to print textual data formated as paragraphs then you should
use the Psion supplied PRINTER.OPX. If you want to print any other type
of data then you may wish to consider using the GPRINTER.OPX supplied here.

We wrote this in order to enable us to support the printing of Gantt and
PERT charts in Plan5. Check out how Plan5 works if you want to see the
GPRINTER.OPX in action. We have released GPRINTER.OPX into the public
domain as freeware for you to use in your own applications.

If you are familiar with the OPL graphics then you will quickly be able to
pick up how to use the OPX, since many of the functions are similar to the
standard OPL graphics functions. The gprinter.zip file contains everything
you need to start using the OPX in your own code.

If you have the Psion EPOC32 C++ SDK and would like a copy of the C++
source code for the OPX then drop me an email.

OVERVIEW
--------

A Psion application normally has a "File->Print" menu with 4 options.
These four options correspond to the following 4 OPX functions:

	GPPageSetupDialog:
	GPPrintRangeDialog:
	GPPrintPreviewDialog:
	GPPrintDialog:

The following OPX function may be used to set the initial default page
size, orientation and header/footer text. The user may overide the
values using the page setup dialog accessed from the GPPageSetupDialog
OPX function.

	GPPageSetup:(page_w&, page_h&, margin&, orient%, header$, footer$)

All units used with the OPX (page sizes, x/y coords, pen widths, etc)
are in twips, where...

	20 twips = 1 point and 72 points = 1 inch.

There are two main OPX functions for printing and print preview.
When calling these OPX functions the OPX will callback to an OPL
procedure to generate the actual graphics. There are two such OPL
procedures that are required for this:

	GPpages&:()
	GPdraw:()

If necessary the names of the callback OPL functions can be altered
using the OPX function:

	GPSetCallback:(num%, proc$)

The GPpages& function is used by the OPX to determine how many pages
there are in the document. The procedure should return the number of
pages. The GPdraw function is used to draw the graphics onto a page.
The remaining OPX functions may be used to generate these graphics.

If there is an error in either of the above 2 functions then because
they have been called from the OPX it can be difficult to track down.
The following may be used to determine which OPX function call failed:

	GPErrorMsg$:

See the sample.opl file for a sample of how to use the OPX.

OPX FUNCTIONS
-------------

GPSetPenColor:(red%, green%, blue%)
GPSetPenWidth:(width&)
GPSetPenStyle:(style%)
GPSetBrushColor:(red%, green%, blue%)
GPSetBrushStyle:(style%)
GPSetDrawMode:(mode%)
GPSetClipRect:(x&,y&,w&,h&)
GPCancelClipRect:

GPAt:(x&, y&)
GPMove:(x&, y&)
GPLineBy:(x&, y&)
GPLineTo:(x&, y&)
GPBox:(width&, height&)
GPFill:(width&, height&)

GPSetFont:(font$,height&)
GPGetFontSpec:(BYREF array&)
GPTWidth&:(text$)
GPPrint:(text$)
GPPrintb:(text$, width&, align%, top&, bottom&, margin&)
GPPrintClip:(text$, width&)

GPSetCallback:(num%, proc$)
GPErrorMsg$:
GPPageSetup:(page_w&, page_h&, margin&, orient%, header$, footer$)
GPPageSetupDialog:
GPPrintPreviewDialog:
GPPrintRangeDialog:
GPPrintDialog:
