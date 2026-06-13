CLIPTEXT.OPX Provides a range of clipboard functions to OPL32
programmers for:

Pasting text to the EPOC32 clipboard and retrieving text from the clipboard.
Text held in individual strings, string arrays or as bytes in a long
integer buffers may be pasted or retrieved from the clipboard.
Allows text data to be cut and pasted between an OPL32 program and
other applications or OPL32 programs.

CLIPTEXT.OPX is Copyright EMCC 1998, all rights reserved.


TERMS AND CONDITIONS FOR USING CLIPTEXT.OPX:

1. Personal individual use only.
Free i.e. for applications _not_ to be distributed to others, whether as
shareware, freeware or commercially. The only condition is that the user
sends their name and address details with a brief description of how and
why they are using it to EMCC - contact details are given below.

2. Shareware or freeware applications.
A 50 UKP one-off license fee per application (i.e. per UID) allows
distribution of CLIPTEXT.OPX for use with a specific licensed application.
For each additional application the license fee is 35 UKP. An unlimited
license is available for 100 UKP and allows unlimited distribution of
CLIPTEXT.OPX with any application from the same author.

3. Commercial applications.
A 120 UKP one-off license fee per application (i.e. per UID) allows
distribution of CLIPTEXT.OPX for use with a specific licensed application.
For each additional application the license fee is 85 UKP. An unlimited
license is available for 220 UKP and allows unlimited distribution of
CLIPTEXT.OPX with any application from the same company.

All licenses include the rights to free updates or fixes.


Customers in the European Community should add VAT at 17.5% to the above
prices.

PLEASE NOTE:

CLIPTEXT.OPX is not freeware for anything other than personal use. It takes
considerable time, effort and expertise to develop OPX software. If a wide
range of OPXs is to become available it must make commercial sense to
software development companies such as EMCC.
Our licensing prices are intended to be modest and are designed to recover
the development effort involved in each OPX and to justify the investment
of more time to develop further OPXs. Therefore, individuals or companies
found to be deliberately and persistently contravening the above terms will
be actively pursued through legal means.

Any application found to be using CLIPTEXT.OPX, that has a UID within the
unique range issued by Psion, will be deemed to be licensable.


OBTAINING A LICENSE FOR CLIPTEXT.OPX

Contact EMCC via one of the methods given below. Please provide:

The author/company name and address.
The application UID(s).
A brief description of how and why CLIPTEXT.OPX is being used.

Payment can be made by MasterCard, Visa, Delta, EuroCard, American Express
or by EuroCheque or Bankers cheque in Sterling made payable to EMCC.


CONTACTING EMCC

Tel:	+44 (0) 1606 861027
Fax:	+44 (0) 1606 861032
email:		info@emcc.cix.co.uk
Compuserve:	100533,1726
Web site:	http://www.compulink.co.uk/~emcc/

EMCC
23 Summerfield Drive
Moulton
Northwich
Cheshire
CW9 8PU
UK


USING CLIPTEXT.OPX VERSION 1.00

Copy CLIPTEXT.OPX into the c:\system\opx folder and put CLIPTEXT.OXH into
the same folder as your own application code. You must add an
INCLUDE "CLIPTEXT.OXH" statement in your OPL source file. All of the
functions in CLIPTEXT.OPX are demonstrated by the OPL32 program in the S5
format source file CLIPTEXT included.


CLIPTEXT FUNCTIONS

StrToClipBrd&:(text$)

Copies the string text$ to the clipboard and returns the text length copied
as a long integer. A zero length string can be used,
e.g. StrToClipBrd&:("") will clear the clipboard.

StrFromClipBrd$:(BYREF len&)

Pastes text from the clipboard into a string, the size of the text obtained
is returned in len&. If the text is bigger than the string size an error -112
is returned. If no text is available for pasting an error -33 is returned.

StrArrayToClipBrd&:(BYREF dummy%, startel%, lastel%, numel%, elsize%)

Copies elements of a string array to the clipboard and returns the text
length copied. An integer variable such as dummy% must occur immediately
before the string declaration array and is used to locate the string array
in memory. The parameters startel% and lastel% specify the array elements to
copy. Paramaters numel% and elsize% must be the same as those in the string
array declaration. The total length copied is returned as a long integer.
A return value of -97 indicates an incorrect parameter passed or the absence
of the dummy% variable.

StrArrayFromClipBrd&:(BYREF dummy%, startel%, BYREF lastel%, numel%, elsize%)

Pastes clipboard text to the string array and returns the text length as a
long integer. An integer variable such as dummy% must occur immediately
before the string array declaration, and is used to locate the string array
in memory. The parameter startel% specifies the first array element to use
in the paste. On return lastel% will hold the number of the last array
element used. Existing data in the string array will be over written but the
paste will stay within the limits of the array.
Parameters numel% and elsize% must be the same as those in the string array
declaration. The total length pasted is returned. A return value of -97
indicates an incorrect parameter passed or the absence of the dummy%
variable. Error -112 is returned if the text is too big and would have gone
over the end of the array. Error -33 is returned if there was no text to
paste.

BufferToClipBrd&:(BYREF a&, mlen%)

Copies the contents of the long integer buffer a& to the clipboard and
returns the length copied as a long integer.
mlen% is the amount of text to paste and must not exceed the maximum size of
the buffer in bytes. The first element of a& must not be used for data, it
must be zero or contain an offset in bytes into the buffer, the paste will
start from the offset and go up to an offset of maxlen%.

BufferFromClipBrd&:(BYREF a&, mlen%)

Pastes text from the clipboard into the buffer and returns the length pasted
as a long integer.
Parameter mlen% is the maximum amount of text to retrieve and must not
exceed the maximum size of the buffer in bytes, less 4 bytes.
The first element of a& must not be used for data, it must be zero or
contain an offset in bytes into the buffer, the copy will start from the
offset and go up to an offset of maxlen%. Error -112 is returned if the text
is too big and would have gone over the end of the array. Error -33 is
returned if there was no text to paste.

