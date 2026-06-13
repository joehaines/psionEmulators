
	    ConVert Version V1.2-10 for Psion Series 3a.
	    ===========================================

--------------------------------------------------------------------------------
 **** You must read and agree to the "Conditions of Use" below PRIOR to use ***
--------------------------------------------------------------------------------

Author: David Howard-Jones, 23rd October 1996, All rights reserved
E-mail: davidhj@pins.co.uk

See "History" for details of all modifications. See "Files" for installation
instructions.

Introduction
============
	Convert provides:
	o	Conversion for common units of measurement e.g.:
		Distance, Weight, Speed, Temperature, Area, Volume,
		Pressure..and others.
	o	Easy means to calculate complex conversions
	o	Currency conversion (inc. taxes, etc.) allowing you to
		+ add, list, modify, and delete currencies
		+ select up to 14 currencies to be simultaneously
		  displayed and converted
		+ easy modification of currency information
		+ unlimited size of currency database (file and
		memory restrictions excepted)
	o	Full use of HELP, MENU, and DIAMOND features
	o	Ability to import and exports values to the
		Psion Series 3a Calculator
	o	History - the ability to recall previous conversions
	o	Choice of UK or US imperial measurements
	o	and more....

	See "Help" for more details.

New for Version V1.2-10
=======================
	o	Additional units and measurement categories
	o	Input may take the form of calculations
	o	New preferences including
		- Inverted currency input
		- Bias in favour of metric input
		- New shortcuts
	o	Minor fixes
	o	Support for A and B drives for all files
	o	Reduced code size!!!!
	o	and more.....

Conditions of Use
=================
My granting you permission to use ConVert is subject to you agreeing to
the following conditions:

1. The software may be used freely and without royalty provided
it is not used for commercial purposes or financial gain.

2. You may only copy and distribute copies of this software in the
same form as you receive it, without any files being removed,
altered or added.  

3. You agree to not attempt to reverse compile, modify, translate
or disassemble this software, in whole or in part.

4. The author will not be responsible for the use or inability
to use this software under any circumstances, and is not liable
in any event for any damages and consequences (including any
lost profit, lost moneys or loss of data) following the use of this
software.

5. This software is supplied as is, and without warranty of any
kind, either express or implied. You accept sole responsibility
and risk for the quality, reliability, and suitability of the
software at all times when it is used.

6. You agree that the author retains all rights to the software.

If you do not accept the above, then DO NOT USE THIS SOFTWARE.
Should you wish to use ConVert in a commercial role, then you must
seek the author's permission for which I reserve the right to
charge or refuse permission.

If you accept the above then I grant you the right to use this
version of ConVert, and hope you find it useful. Please find a
few moments to offer feedback and constructive suggestions on how
it could be improved!

Files
=====
ConVert is distributed in the form of a .ZIP file containing the
following files:

	readme.txt	this file
	cnvert.opa	executable code
	cnvert.rsc	help file

To use ConVert, place cnvert.opa in your \APP directory,
cnvert.rsc in your \OPD directory and install the program via
the "Install" option in the Series 3a's system Menu in the usual
way.

Convert will create a cnvrate.odb file in M::\OPD if this is
the first time you have used it. You can place any file on other
drives provided you keep the same directory name. THE ONLY
RESTRICTION is that cnvrate.odb should NOT be a Flash
drive. See "Disk Space Hints" in 'Help' for further details.

If you are upgrading, simply overwrite the existing files. Any
existing currency database is also fully compatible with the new
version.

If you are pushed for space, the help file can be deleted. However
I strongly recommend you keep the file for a few weeks until you
are familiar with ConVert's functionality.

History
=======

Version	Edit	Description
-------	----	-----------
V1.2	10.	Support for A and B drives
====	9.	New preferences and shortcuts including..
		- toggle UK<>US+Internation
		- toggle Weight system
		- toggle Scientic zeros
		- increase/decrease decimal places
		- switch Imperial/Metric as favoured input
		- switch to inverted currency input
	5.	Add new categories and measures
	7.	Use "Evaluate" for user input to allow complex calculations
	6.	Fix reported bugs(including only conversion reported namely
		cu. yard out by factor of 1000)
	5.	Tidy display to allow greater number of measurements
V1.1	4.	US Pressure display now shows US Ton sq.In and sq.Ft
====	3.	Fixed bug which sometimes meant values arn't exchanged
		with the Psion Calculator and caused occasional Divide by
		zero on start up.
	2.	Changed currency database field names & help:-
		* More descriptive names
			"Full name" rather than Country
			"Short name" rather than "Code"
			"Display priority" rather than "Class"
		* Short names now up to 9 characters long not 6
		* Improved Help

V1.0	1.	First release

Background
==========
I wrote this software to replace the various conversion tables
supplied at the front of the numerous diaries I have previously
owned prior to purchasing a Psion 3a. I wrote it to suit my
tastes and I no idea if it suits anyone else's! I did investigate
the various conversion factors but if mistakes may have been made
PLEASE LET ME KNOW.

I do not intend to charge for this version but reserve the
right to do so with future releases should I wish! Requests
for extra functionality, will probably mean that version
V1.3 will become "Shareware"

As a new release of software please ensure you take the usual
precautions you would (or should) take when installing new
software - backups old versions etc.

Many people have mailed me with words of thanks and suggestions,
well many thanks to you! Some requests I have not been able to
include in this version, but these are on the list for the next
version
	* Import/export currency information with other "banking"
	  software
	* Sort currency display
	* Additional units, and categories etc.
	* User customisable category

Finally and as an acknowledgment to those folks that have
made their programs and examples freely available to the rest
of us, many thanks indeed. I hope this software is of use to
one of you.

If you have any comments or suggestions about ConVert, please
mail me. I cannot provide formal support or even guarantee a
response, however I would be pleased to hear from you. See the
"About ConVert" option on how to mail me.

Finally, if I have forgotten any acknolwedgements (in error), then
please forgive me!

Regards and enjoy,
David Howard-Jones.