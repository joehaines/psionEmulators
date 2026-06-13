      RICH TEXT FORMAT (RTF) DRIVERS FOR THE SERIES 3/3a WORD PROCESSOR
      =================================================================

This directory contains Rich Text Format (RTF) drivers that enable
the Series 3/3a Word Processor to save and load files in Microsoft
Rich Text Format (RTF). This allows you to interchange documents with
other programs which can save and load RTF files, such as:

  -   Microsoft Word for Windows
  -   Microsoft DOS Word
  -   Microsoft Word on the Macintosh

Before you can save and open RTF files on your Series 3/3a, you must
install the RTF drivers (WL$RTF.DYL and WS$RTF.DYL) into a \WDR
directory on your Series 3/3a. You can install them onto the Internal
disk or, if you want to avoid using up internal memory, an SSD in
drive A or B.

Installation
------------

1. Display the RCom command prompt (on Windows-based PCs use the RCom
icon; on DOS-based PCs enter the appropriate command to run RCom).

2. Make sure that the current directory is the one in which you
installed RCom. If RCom is located in c:\psrcom you can type 
CD C:\PSRCOM to make this the current directory.

3. Make sure that the `Remote link' option on the Series 3/3a System
screen is set to `On'.

4. Type RTFINST followed by the new (virtual) PC drive letter that is
set up to represent the Series 3/3a drive on which you wish to
install the RTF drivers. For example, RTFINST I:

This runs the batch file RTFINST.BAT which copies the RTF drivers
into the \WDR directory on the specified Series 3/3a drive. 
 
Note: Don't confuse the \WRD and \WDR directories - \WRD\ is the
directory where Word processor documents are stored, while \WDR is
where the Word processor keeps special files of various kinds
(templates, and add-on software or printer drivers).

Note: WL$RTF.DYL is the software to open RTF files, and WS$RTF.DYL
the software to save them. You don't have to install both drivers, if
you only wish to be able to open OR save RTF files. To install a
single RTF driver: use the COPY command from the RCom command prompt
to copy the relevant file to a \WDR directory on the Series 3/3a,
rather than using the RTFINST.BAT batch file described above.

When the RTF drivers are installed
----------------------------------

You can copy RTF files to and from the Series 3/3a like any other
files, as described in the RCom booklet. When you next use the `Open
file' or `Save as' options in the Word processor, you will be able to
set the `File type' line to RTF, and select an RTF file.

Note: when the RTF file is next saved in the Series 3/3a Word
processor - and this will happen if you switch to a different file -
it is saved in the Series 3/3a Word processor format with the .WRD
file extension, complete with style and emphasis information. If you
wish to send the file back to the PC at a later time, you need to use
the `Save as' option to save the file in the RTF format, creating a
new RTF version of the file.

Direct access to remote RTF files
---------------------------------

You can use the Series 3/3a Word processors `Open file' and `Save as'
options directly on RTF files on the PC, as long as the Series 3/3a
is linked to the PC by RCom. Simply set the `Disk' line to the
appropriate REM:: drive and then select the file. See the RCom
booklet for more details.

Using the HP3 printer driver as a nominal RTF driver
----------------------------------------------------

The HP3 printer driver (with file name HP3.WDR - also supplied on the
RCom disk) is primarily for printing to the Hewlett Packard III laser
printer. However, with RTF, it is worth using HP3.WDR as the printer
driver, even when there is no prospect of printing to the Hewlett
Packard III laser printer, because it contains a good basic font set
(mono-spaced Courier, a scalable serif font and a scalable sans-serif
font) which travels well through the "RTF barrier".

For details of how to install the HP3 driver, see the README.TXT
documentation file in the \PRINTDRV subdirectory of your RCom
directory. This also gives information about the other printer
drivers supplied on the RCom disk.

Restrictions on headers and footers
-----------------------------------

The Series 3/3a Word processor supports only one header, which applies
to the whole document. If an RTF file specifies some other type of
header, such as for even pages only, this may become the header for
the whole document.

The `On first page' settings in the Series 3/3a Word processor's
`Header' and `Footer' dialogs are not affected by opening an RTF
file.

If a header in Microsoft Word has a separate stylesheet size, or any
changes of font within its text, these will be ignored. 

Microsoft Word may not support the %F and %M codes, nor two- and
three-column headers, which can be used in the Series 3/3a Word
processor. Two- and three-column headers will be treated as a
left-justified header in Microsoft Word.

The same restrictions apply to footers as to headers.

Other limitations
-----------------

USE A .RTF FILE EXTENSION FOR ALL RTF FILES - ON THE SERIES 3/3A AND
PC. When you save an RTF file, the Series 3/3a Word processor forces
this extension, but you should also use this file extension when
creating RTF files on a PC.

Content which is beyond that handled by the Series 3/3a Word
processor (such as boxes, graphics, side-by-side formatting of
paragraphs and tables of contents entries) are not transferred into
Series 3/3a Word documents.


Using Stylesheets in Microsoft DOS Word
---------------------------------------

(This information is based on version 5.0 of the MS-DOS version of
Microsoft Word.)

Microsoft Word for DOS saves a document's style information in a
separate file - a stylesheet. It can also, however, handle files
without a stylesheet, using in-line formatting (storing special codes
in the document which specify the various settings for each
paragraph).

If you use a stylesheet with a particular Microsoft Word file, and you
want to transfer the file to and from the Series 3/3a, do not change any
of the styles while on the Series 3/3a. (You can still apply styles to
paragraphs, and you can use emphases as normal.) When you next send
the file to the PC and use it in Microsoft Word, it will ask for the
stylesheet to use. Microsoft Word checks that the document styles
still match the stylesheet. Any paragraphs which have a style that
does not match those in the stylesheet are converted to use in-line
formatting.

If you are a user of the `Alter paragraph' option (instead of the
`Styles' option) for changing the look of paragraphs, this should not
worry you.

Control over fonts with Microsoft DOS Word
------------------------------------------

Microsoft Word for DOS is not as sophisticated in its mapping of
incoming fonts when it reads an RTF file as is Microsoft Word for
Windows. When you load an RTF document using Microsoft Word for DOS
(having created it on your Series 3/3a), you may find that some fonts
have been mapped to those which are not present in the current
printer driver (in which case, they will be printed as Courier).

For greater control over the fonts Microsoft Word for DOS uses, you
have two options:

  1)  To simply use in-line formatting to change the fonts.

  2)  Use an approach involving two separate stylesheets.

The first is simpler to understand but the second solution, once in place,
has the advantage of automatically applying to all files that you
subsequently transfer from the Series 3/3a into DOS Microsoft Word.

To use two separate stylesheets:

1. Choose a representative document, written on the Series 3/3a, that
includes all the styles you use on a regular basis.

2. Save this as an RTF file.

3. On loading it into Microsoft Word, leave the stylesheet selection
blank (delete any suggestion offered to you).

4. You will probably find it easier in what follows to make the style
bar visible (use Options).

5. For each style that you want to record, place the cursor in some
text with that style.

6. Use Format Stylesheet Record (alternatively, Alt-F10).
Give as the Key Code the shortcode used on the Series 3 for the
style.
Leave Usage as Paragraph and type anything you like into Remark
(leave it blank if you wish).
For Variant, in most cases you can accept the default offered to
you, but match `Standard' to `Body Text', and `Heading level' styles
to the appropriate Series 3/3a `Header level' style (especially if you
use outlining).

7. Repeat steps 5. and 6. for all the styles you are interested in

8. When finished, save the resulting stylesheet (use Gallery Transfer
Save), with a name such as "S3".

9. Then in Gallery, look at the fonts specified by each style, and
make any changes required.

10. Save the stylesheet you have at the end of this second pass WITH
A DIFFERENT NAME, say "S3P" (with the "P" standing for "Printing").

From now on, whenever you load an RTF file prepared by the
Series 3/3a into DOS Microsoft Word, all you need to do is:

1. When prompted for the name of a stylesheet, give "S3" (or whatever you
named the first of the two stylesheets).

2. Once the load has completed, use Format Stylesheet Attach to attach
the second stylesheet, "S3P" or whatever, instead.

Use of BOTH stylesheets is essential to the operation of the scheme.

The system takes some effort to set up but you will be amply rewarded with
its power and ease of use in the long run.



