Edit Boxes in OPL
-----------------

In this ZIP file you should have found:

*  README.TXT	this file. This documentation is
		deliberately light on information.
		Full understanding will require
		reference to EB.DOC (described later).

*  EB.OPL	OPL source (requires the SIBO OPL SDK)

*  EB.OPH	header file containing constants and
		definitions

*  EB.DOC 	a Word file extracted from the Psion
		SIBO C SDK which describes the edit-
		box functions from the HWIF library in
		some detail. For a completely thorough
		understanding of the techniques employed
		users are referred to the SIBO C SDK
		which includes the C sources of the
		HWIF library and associated header files.

*  EB.RTF 	an RTF version of the above document.

*  EB.TXT	a text-only version of the above document


EB.OPL
------

EB.OPL is a file which contains the OPL source
to a suite of functions which allow OPL programmers to
create edit boxes for use in their applications. The suite
of functions is designed to mirror the functionality of the
hEBxxx functions in Psion's HWIF library. An edit box created
using this code can have the following features:

*  text is automatically word wrapped at the visible right margin.
   Users can start a new paragraph by pressing the <Enter> key.

*  in multi-line editors, the display is vertically scrolled
   automatically

*  the user can show or hide paragraph ends and spaces

*  the text may be displayed with different attributes (bold, etc.)
   Note that the style cannot vary word to word.

*  the width of the editor may be varied on request

*  a side cursor may easily be displayed. The width of the text cursor
   may be easily altered.

*  the value cut, copy, paste, evaluate, find and replace are all
   easily available.


The procedure
-------------

The minimum steps required to include an editor are:

*  create the editor using a call to hEBOpen

*  when appropriate, emphasise it using hEBEmphasise

*  pass relevant keys to it using hEBHandleKey

*  when the application needs the contents, they can be
   retrieved using hEBSenseText


The functions supplied
----------------------

Note that the function names used would, ordinarily
be illegal in OPL. For the sake of understanding, the
OPL function names have been replaced by symbols which
are, themselves, replaced at translate-time by the OPL
translator (either HHTRAN or ODE). The symbol definitions
are defined in EB.OPH and are:

hEBOpen			EBOPEN%:
hEBHandleKey		EBHK%:
hEBCompleteFormat	EBCnFm%:
hEBSenseText		EBSnsT%:
hEBSetText		EBSetT%:
hEBEmphasise		EBEmph%:
hEBSetSelect		EBSetS%:
hEBSenseSelect		EBSnsS%:
hEBSenseClipText	EBSnsC%:
hEBSetClipText		EBSetC%:
hEBChangeWidth		EBChgW%:
hEBSetCWidth		EBStCW%:
hEBInsert		EBIns%:
hEBReplace		EBRpl%:
hEBEvaluate		EBEval%:
hEBCopy			EBCopy%:
hEBPaste		EBPaste%:
hEBFind			EBFind%:
hEBClearChanged		EBClrCh%:
hEBSenseChanged		EBSnsCh%:
hEBShowSymbols		EBShwSy%:
hEBClose		EBClos%:
hEBSenseDoc		EBSnsDc%:
hEDCapacity		EDCap%:
hEDInsert		EDIns%:
hEBDocChanged		EBDocCh%:
hEBSetMargin		EBSetMg%:
hEBSenseMargin		EBSnsMg%:
hEBPosToXL		EBPosXL%:


Example
-------

The EB.OPL module contains a function to create an
edit box (CreateEditBox:), seed the edit box with
some text (InitText:) and then a short mainloop which
offers examples of:

*  handling keys using hEBHandleKey:

*  evaluating using hEBEvaluate:

*  copying text using hEBCopy:

*  inserting text using hEBPaste:

*  finding text using hEBFind:

*  on exit, sensing the length and start of any highlighted
   text in the edit box using hEBSenseSelect:


Support
-------

This code is supplied on an as-is basis and no offer of
support is expressed or implied. Questions on the techniques
used will, in most cases, be answered by referring to the
contents of EB.DOC or the sources of HWIF.LIB as supplied
with Psion's SIBO C SDK. Questions may be directed, informally
to "ali-manson@psion.com" but, although I shall try to assist
a response cannot be guaranteed.

Prepared 11.2.95 by Alasdair Manson of Psion Inc. Technical
Support. Thanks to Steve and David.





