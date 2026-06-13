
Clipboard V 1.00 Feb '98 for the Psion Series 5 by Mark Fitzpatrick

CONTACT INFORMATION
~~~~~~~~~~~~~~~~~~~
	If you have any questions, require some assistance,
	or just feel like exchanging some e-mail, I can be
	contacted via email:
	+ For Internet users:-   markf@mssoft.com.au

	+ For Compuserve users:- 100026,2452
				 mark_fitzpatrick

	+ For Snail Mail users:  Mark Fitzpatrick,
				 10 Brandon Place,
				 St Ives, 2075
				 SYDNEY, NSW,
				 AUSTRALIA

OVERVIEW
~~~~~~~~
Clipboard is a small set of 6 OPL functions for getting and putting 
information from / to the EPOC-32 clipboard. The OPL source for 
Clipboard is supplied in full and may be used or abused in anyway
you see fit. It is provided free of any charges or obligations.


CONTENTS
~~~~~~~~
	readme.txt	- This file
	Clipboard	- The OPL Source for the functions
	Clipboard.opo	- The "compiled" clipboard module
	Clipboard.oph	- The file to include in your application
	Cliptest	- OPL Source for a test / demo program

	To Install Clipboard onto your Psion Series 5...

INSTALLATION
~~~~~~~~~~~~
	Well, pretty straight forward actually. Copy the OPH and OPO
files to either the "\SYSTEM\OPL\" directory, or copy them locally to
the directory you are using for developing your application. I would
recommend the first option as being the tidier of the two.

HISTORY
~~~~~~~
	09Feb98	V1.00 Released

DOCUMENTATION
~~~~~~~~~~~~~
	Basically there is none. Well actually, I hope the
Clipboard.oph file is sufficient combined with the test/example
program supplied (cliptest). In any event here is a text copy of the
OPH file to give you a start.

REM 
REM AUTHOR	Mark HJ Fitzpatrick
REM
REM DATE:	February, 1998
REM
REM EMAIL:	markf@mssoft.com.au
REM
REM WWW:	http://www.mssoft.com.au/mark/index.html
REM
REM MODULE:	Clipboard.oph OPL header
REM
REM VERSION:	1.0
REM
REM CREDITS:
REM
REM	Credit goes to the following two people who posted examples of
REM how to do this. This module would not have been possible without
REM them.
REM		Steve Godfrey (steve.godfrey@purplesoft.com) and 
REM		Peter Finch		(peter@finchpj.demon.co.uk)
REM
REM DESCRIPTION
REM
REM		Clipboard supports simple access to both getting and
REM		putting things to/from the Series 5 Clipboard
REM
REM DISCLAIMER
REM
REM	This code is supplied on an "as is" basis. Use it at your own
REM	discretion. No responsibility for any loss of information or
REM	damage that may occur as a result of using it will be
REM	accepted by the author.

REM Puts the String at address pStr& into the clipboard
REM
external CBPutStr&:(pStr&)

REM Gets the contents of the clipboard and returns it as a string
REM
external CBGetStr$:

REM Puts "bufz&" bytes of the contents of the buffer pointed to by
REM "pBuf&" into the clipboard
REM
external CBPutBuf&:(pBuf&, bufz&)

REM Receives the contents of the clipboard or up to "bufz" bytes 
REM (whichever is smaller) from the clipboard into the buffer
REM pointed to by "pBuf&"
REM
external CBGetBuf&:(pBuf&, bufz&)

REM Copies elements from a string array pointed to by "pArray&" to 
REM the clipboard. They are copied from "startElt%" to "stopElt%"
REM inclusive, and "eltz%" is the declared size of each individual
REM element of the string array
REM
external CBPutStrArray&:(pArray&, startElt%, stopElt%, eltz%)

REM Copies the contents of the clipboard into the array of strings
REM pointed to by "pArray&". Copying starts at element "startElt%"
REM and continues until either all of the clipboard is copied or
REM it has filled the last element as defined by peekw(pStopElt&).
REM Each element of the array is filled completely ("eltz%") before 
REM moving on to the next element.
REM
external CBGetStrArray&:(pArray&, startElt%, pStopElt&, eltz%)
