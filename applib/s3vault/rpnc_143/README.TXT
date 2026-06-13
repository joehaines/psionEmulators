RPNCalc for the Series 5 (SHAREWARE)
--------------------------------------

Disclaimer
----------
This version of RPNCalc is version 1.43
Use this software at your own risk. I accept no responsibility 
for loss of data.

History
-------
Version 1.0 beta Released 7/10/97

Version 1.01 Released 9/10/97

	Fixed RAD50 bug - shouldn't be allowed in signed mode
	Added Hotkey and Escape to background

Version 1.02 Released 9/10/97

	Moved stats counter out of M9. Stats count now on key

Version 1.03 Released 10/10/97

	Solved the missing "send to background" message when going to another
        program from the System Task List.

Version 1.04 Released 16/10/97

	Fixed bug due to users setting International-Number decimal-point
	to the "comma" character. This prevented registration. Revamped display
	to create space for financial-functions. I have released this version prior
	to implementation of these functions because of the bug fix.
	
Version 1.1 beta Released 27/10/97

	Many improvements: 4 user defined formulae instead of 2, user-definable labels
        for the constants, conversions and formulae keys, INT, MOD and % functions,
        MAX and MIN statistical functions, financial functions, context help for every
        key, much more usable statistical facilities with the ability to view, delete
        and modify the stats values, etc.

Version 1.2 Released 7/12/97

	Fixed stats update bug (X-register was sometimes ignored)
	Fixed clock bug - clock now obeys the system setting.
	Changed numeric keypad layout due to the requests from users.
	Moved the hexadecimal keys to the left and ordered the keys 1,2,3 etc
	to be more like a standard numeric keyboard.
	Added a feature that lets RPNCalc take over the silkscreen Calc key.

Version 1.3 Released 26/12/97

	The calculator can be "flipped" from left-handed to right-handed
	(registered users only).
	The "inv" key can be locked.
	No longer a file-based app (and never should have been!).
	Includes a utility to display all ascii codes in the selected number base
	(removing the need to also have my Ascii program).
	The unregistered restriction that displayed certain functions in an informational
	message has been removed. I now feel that there are sufficient benefits obtained
	from registering the program, that this restriction is unecessary.

Version 1.4 Released 7/1/98

	Rectangular-Polar conversion added.
	Decimal hours to hours-mins-secs added, allowing time calculations.
	Random number function.
	32-bit binary to IEEE floating or DEC VAX F_Float.
	Twice as many user-definable Constant/Conversion/Formulae keys.
	Cosmetic revamp.	

Version 1.41 Released 12/3/98

	Floating to IEEE hex function added.

	Engineering display mode added - requested by Roland Heuberger.

	Changed X^Y to Y^X - this matches HP calculators and is more intuitive - thanks
	to Marcus Herold for pointing this out.

	Fixed MODULUS flaw. eg 70.9 INV INT displays 0.90000000000000006 not 0.9 - this
	is acually the value returned by EPOC32. I now calculate the MODULUS (fractional
	part) myself. Thanks again to Marcus Herold for telling me about this.

	Changed the key logos for square root, cube root and XrootY. X^-2 is not actually
	the square root of X, but the reciprocal of it! Thanks to Pappas Sotirios for this
	one.

	Fixed an error that occurred when setting up key constants - press SET button
	 - "Set constants keys" - "Set c1 key" - "Select from list 1" - press the "Lists"
	button - gives "Subscript or dimension error". I must have broken this in release
	1.4. Thanks to Anders Nilsson for noticing this. I am amazed it took two months
	to be spotted!

Version 1.42 Released 21/4/98

	Maximum formula length increased from 60 characters to 250.
	Thanks to Tim Wortley for this suggestion.

Version 1.43 Released 25/5/98

	Added 3 shortcuts at the request of Tadashi Homma as follows: "m" = +/-;
	"z" = square root and "x" = 1/x. Key help indicates which keys have shortcuts.
	The "z" and "x" match the HP-200LX, and "m" matches the built-in calculator.

	Corrected a flaw with the stack-handling of X^Y to Y^X. These are dual-operand
	instructions, so should cause the stack to drop, but did not.
	Thanks to Dieter Tackmann for pointing this out.

Installing RPNCalc
------------------
1. Make sure you have access to the System directory. For this, goto the preferences
   in the system screen. Check the 'Show 'System' folder' option.

2. Got the System\Apps directory and create a new directory called 'RPNCalc'.

3. Switch to this directory and copy all the supplied files to this directory (excluding
   this file). 	Ensure that NO conversions are done when copying any of the files.

4. The program should now appear on the Extras bar.

5. If upgrading, delete ALL THE OLD FILES and replace with the new (registered users
   will want to keep their existing RPNCalc.ini file).

Read the supplied help file, which is accessible from the program.

Registering
-----------
RPNCalc is shareware. If you like it and wish to continue to use it you should
register it. You may freely distribute RPNCalc as long as all files in the package are
included.

Registering bring a number of benefits. See the help file for details.

Registering (cash preferred) costs £10 UK, $16 US or the equivalent in other foreign
currencies plus 10%.

Send to:

	Derek Johnston
	14 Roxby Avenue
	Guisborough
	Cleveland TS14 8LF

Alternatively register online from my web page using the RegNet service.

When registering please make sure the following information is sent (if possible):

	+ Your correct e-mail address		MANDATORY
	+ Your name (as you would enter it)	MANDATORY

Once you have registered, you will be sent a registration ID which you enter along
with your name. This will work for all future versions of the calculator.


Derek Johnston.

EMAIL:         decca@enterprise.net
Homepage:      http://homepages.enterprise.net/decca/
