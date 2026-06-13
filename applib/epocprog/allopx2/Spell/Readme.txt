SPELL.OPX
-----------
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
This OPX gives you access to a number of functions in the SPELL Suite, to enable you to access
them from within a OPL program.

FILE DETAILS
------------
The archive consists of the following files:

README.TXT		This file
SPELL.SIS		This is the main OPX file in SIS format
SPELL.OPX		This is the WINS version of the OPX
SPELL.OXH		This is the header file
SPELL.OPL		This is a demonstration program that shows you how the OPX can be used

INSTALLATION
------------
1.	Install SPELL.SIS

2.	Copy SPELL.OXH into the \System\Opl\ folder on either the C: or D: drive

3.	Copy SPELL.OPL any where you like

USING THE OPX
-------------
1.	First compile and run the SPELL.OPL file to make sure everything works and it communicates with
SPELL Suite correctly.

2.	To use the OPX in your program add the following line to the top of the code, immediately after
the APP...ENDA and before the first procedure

	INCLUDE "SPELL.OXH"

3.	You can now use the following additional procedures in your program.

Spell$:(string$)
================
Spell checks every word in the string.  It returns an empty string if the spelling appears to be correct.
Otherwise, it returns the first word that is misspelled.


SpellBuffer$:(buffer&, length&)
===============================
This checks the spelling of every word in the buffer at address buffer&. The length of the buffer is passed
in length&. This is a similar format to dEDITMULTI, but the length is passed separately (to make it easy to
spell check parts of a longer buffer). Return values are as for Spell$:


SpellWhere&:
============
Returns the position (starting from 1) of the last misspelled word found. Useful to restart spelling, or for
marking the incorrect word.


OpenSpeller:(userDict&)
=======================
Starts a connection with the spell checker.  You HAVE to call this before calling Spell$: or SpellBuffer$:.
The argument determines whether the user dictionary is used (&1 for yes, &0 for no).


CloseSpeller:
=============
Call this when you are done with the spell checker.  It will release the checker. This is mostly important
when using the user dictionary, as it cannot be shared with another application.


SpellUserDict&:
===============
Returns 1 if the built=in Spell application uses the user dictionary, 0 otherwise.


SpellAlternatives:(word$, max&, "Alternative")
==============================================
This looks for alternative (more correct) spellings of the word$. If spellings are found, the first max& of
these are delivered to OPL by calling the function "Alternative&" (or whatever you choose). The callback
function can decide to stop by returning &1. See the example program.


SpellWildcards:(word$, max&, "Alternative")
===========================================
Given a word$ containing the wildcard characters "*" and "?", this looks for spellings matching the pattern.
If spellings are found, the first max& of these are delivered to OPL by calling the function "Alternative&"
(or whatever you choose). The callback function can decide to stop by returning &1. See the example program.


SpellAnagrams:(string$, len&, max&, "Alternative")
==================================================
Finds Anagrams of minimal length len& using the letters in string$. The first max& anagrams are delivered to
OPL by calling the function "Alternative&" (or whatever you choose). The callback function can decide to stop
by returning &1.  See the example program.

DISTRIBUTING THE OPX
--------------------
If you wish to distribute the OPX as part of your program, then you need to include the unchanged
SPELL.SIS in the ZIP archive or SIS package for your program. Note that you may not, UNDER NO
CIRCUMSTANCES, distribute the unpacked SPELL.OPX file. 

If you do not follow this rule, you disable the EPOC version control over the OPX, and your
application may break when the user installs a different version of the OPX. Worse, installing
your application may break other applications, and you can imagine the reactions this may cause you.
Don't say you haven't been warned!

You may not rename the OPX that you distribute, and you may not redistribute SPELL.OXH or SPELL.OPL,
they are simply for use on the developers machine. (The first rule makes sure that multiple copies of
OPX with the same UID cannot happen, the second rule is to avoid a proliferation of outdated copies of
our OPXs. Please refer interested parties to the RMR website.)

Shareware using this OPX must include this information in the "About" screen. (A line like "Contains
SPELL.OPX © Otfried Cheong" or similar.)


REGISTRATION
------------
SPELL.OPX is free for personal use and for use in Freeware programs. If you wish to distribute it in a
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