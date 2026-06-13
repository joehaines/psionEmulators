RMRBUFFER.OPX
-------------
Copyright (c) Otfried Cheong and RMR Software 1998

If you going to distribute the OPX further then please read Part 5 first. This is VERY IMPORTANT.

README file
-----------
Contents:

1.	Introduction
2.	File Details
3.	Installation
4.	Using the OPX
5.	Distributing the OPX
6.	Registration
7.	Other Programs from RMR Software

INTRODUCTION
------------
This OPX gives you access to a number of functions in the RMRBUFFER Suite, to enable you to access
them from within a OPL program.

FILE DETAILS
------------
The archive consists of the following files:

README.TXT		This file
RMRBUFFER.SIS	This is the main OPX file in SIS format
RMRBUFFER.OPX	This is the WINS version of the OPX
RMRBUFFER.OXH	This is the header file
RMRBUFFER.OPL	This is a demonstration program that shows you how the OPX can be used

INSTALLATION
------------
1.	Install RMRBUFFER.SIS

2.	Copy RMRBUFFER.OXH into the \System\Opl\ folder on either the C: or D: drive

3.	Copy RMRBUFFER.OPL any where you like

USING THE OPX
-------------
1.	First compile and run the RMRBUFFER.OPL file to make sure everything works and it communicates with
RMRBUFFER Suite correctly.

2.	To use the OPX in your program add the following line to the top of the code, immediately after
the APP...ENDA and before the first procedure

	INCLUDE "RMRBUFFER.OXH"

3.	You can now use the following additional procedures in your program.

BufferCopy:(target&, source&, length&)
======================================
Copies the area of length length& from source& to target&.  The case of overlapping source and target
is handled correctly.


BufferFill:(buffer&, length&, char%)
====================================
Fills the buffer with the character.


BufferAsString$:(buffer&, length&)
==================================
Returns the contents of the buffer as a string.


BufferFromString&:(buffer&, length&, string$)
=============================================
Assigns the buffer from the string, returning the length of the string in the buffer.


BufferMatch&:(buffer&, length&, pattern$, fold&)
================================================
Matches the buffer with the pattern$. The pattern may contain the usual wildcards ? and *. Returns
the position (starting with zero) of the first matching character, or -1 if the pattern is not found. 

If fold& is 1, characters are folded before comparison, if fold& is 2, they are collated, and if fold&
is zero, they are compared directly.

NOTE: To test for the existence of a pattern within a text string, the pattern must start and end
with an `*'.

Example:  Buffer contents is "abcdefghijklmnopqrstuvwxyz"

BufferMatch&:(buffer&, len&, "*ijk*", 0)       returns -> 8
BufferMatch&:(buffer&, len&, "*i?k*", 0)               -> 8
BufferMatch&:(buffer&, len&, "ijk*", 0)                -> -1
BufferMatch&:(buffer&, len&, "abcd", 0)                -> -1
BufferMatch&:(buffer&, len&, "*i*mn*", 0)              -> 8
BufferMatch&:(buffer&, len&, "abcdef*", 0)             -> 0
BufferMatch&:(buffer&, len&, "*", 0)                   -> 0
BufferMatch&:(buffer&, len&, "*y*", 0)                 -> 24
BufferMatch&:(buffer&, len&, "*i??k*", 0)              -> -1


BufferFind&:(buffer&, length&, string$, fold&)
==============================================
Searches for the string$ in the buffer. Returns the position (starting with zero) of the first
occurrence, or -1 if the string is not found.

If fold& is 1, characters are folded before comparison, if fold& is 2, they are collated, and if
fold& is zero, they are compared directly.


BufferLocate&:(buffer&, length&, char%, fold&)
==============================================
Searches for the character with code char% in the buffer. If fold& is non-zero, characters are
folded before comparison.  Returns the index of the first char found, or -1 if the character is
not found. 


BufferLocateReverse&:(buffer&, length&, char%, fold&)
=====================================================
Same as above, but returns the index of the *last* char found, or -1 if the character is not found.

DISTRIBUTING THE OPX
--------------------
If you wish to distribute the OPX as part of your program, then you need to include the unchanged
RMRBUFFER.SIS in the ZIP archive or SIS package for your program. Note that you may not, UNDER NO
CIRCUMSTANCES, distribute the unpacked RMRBUFFER.OPX file. 

If you do not follow this rule, you disable the EPOC version control over the OPX, and your
application may break when the user installs a different version of the OPX. Worse, installing
your application may break other applications, and you can imagine the reactions this may cause you.
Don't say you haven't been warned!

You may not rename the OPX that you distribute, and you may not redistribute RMRBUFFER.OXH or RMRBUFFER.OPL,
they are simply for use on the developers machine. (The first rule makes sure that multiple copies of
OPX with the same UID cannot happen, the second rule is to avoid a proliferation of outdated copies of
our OPXs. Please refer interested parties to the RMR website.)

Shareware using this OPX must include this information in the "About" screen. (A line like "Contains
RMRBUFFER.OPX © Otfried Cheong" or similar.)


REGISTRATION
------------
RMRBUFFER.OPX is free for personal use and for use in Freeware programs. If you wish to distribute it in a
Shareware Package, then we ask that you register it by E-Mailing us at opx@rmrsoft.com. We are asking
for a nominal fee of twice the registration fee of your program. This also includes full backup support,
such as a WINS copy, e-mailing of enhancments and influence over future development of the OPX. Hope you
think this is acceptable, we are not trying to make money on this, just cover out costs.


OTHER PROGRAMS FROM RMR
-----------------------
If you like the look of this OPX, why not have a look at our programs.

A full list is as follows:

S5BANK			: A Personal Accounts Suite
RMRTASK			: An Extended Task (ToDo) Manager.
RMRNOTES		: A Note Taker/Jotter program
S5HOME			: A Home Inventory program.
S5FUEL			: A Fuel Consumption Monitor.
RMRUTILS		: A Utility/Conversion program
RMRZIP			: A Compression/Archive Utility
VACTRAC5		: A Holiday/Leave Tracking program
RMRSOL			: The classic "Solitaire" patience game
RMRFILE			: The premier file manager for EPOC machines
CONTACT			: The only Contact Manager available for EPOC machines

Some of these are also available in other languages, such as French, German, Spanish etc.. See the
Home Page http://www.rmrsoft.com/ for details.