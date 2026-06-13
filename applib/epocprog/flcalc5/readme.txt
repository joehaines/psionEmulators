---------------------------------------------------------
FlCalc 5 - A programmer's calculator.
(C) 1997, 2000 David Rushall
Freeware. All rights reserved.
---------------------------------------------------------

INTRODUCTION

FlCalc5 is a programmer's calculator with a difference.

Originally written to compliment the Calc application on
my Series 3a as I couldn't find anything I liked that did
what I needed and ported to the Series 5 for the same
reason.

Features include

  o  32-bit integer expression evaluation.
  
  o  Multiple simultaneous output formats including 
     decimal, hex and binary.
  
  o  ASCII and EBCDIC character tables.
  
  o  Multiple memories.
  
  o  Repeatable statements.
  
  o  Pen-enabled on-screen keypad.
  
  o  Copy and paste.

CONDITIONS OF USE

  1) FlCalc5 is Freeware and copyright; it is not public
     domain. The rights to this package remain the
     property of the author.
     
  2) You may use this software, free of charge, for an
     indefinite period and at no obligation to the
     author.
     
  3) Permission is given to publically distribute this
     software provided the distribution contains all, and
     only, the unaltered files of the original
     distribution, the ownership of the rights to this
     software is clearly stated and any fee charged is
     extremely nominal.
     
  4) Use of this software is entirely at the user's own
     risk. The user must accept responsibility for any 
     direct or indirect loss or damage arising from the 
     use (or misuse) of the package. This software is 
     supplied "as-is".

  5) Note: The OPX file employed by this software (and
     included in this distribution) is the copyright of
     EMCC. Please see their WWW site,

     http://www.compulink.co.uk/~emcc/

     for more details.

INSTALLATION

  1) Close all applications on your Psion.
  
     Note: If you do not close all applications before installing
     the OPX *.sis package in the next step, you may have problems
     with OPX versions, later.     

  2) Using PsiWin2 or the "Add/Remove" icon on the EPOC control
     panel (if you have it), install the third-party
     OPX *.sis file:
     
        Cliptext.sis

     NOTE: You will be asked whether you wish to install
     the documentation for the Cliptext.opx. After the OPX
     is installed, a directory called "cliptext" will
     have been created in the root directory of your
     machine. If you do not wish to develope you own
     applications using the OPX you can delete this
     directory and its contents without affecting the
     operation of FlCalc.

     IMPORTANT: The supplied OPX is for ARM-based machines,
     such as the Series 5, Series 5mx, Series 7, Revo, etc. If
     you wish to run this application on the EPOC emulator then
     please contact the OPX author, EMCC (web address
     above) for the appropriate versions.

     If you get a message saying that a later version than
     the one supplied has already been installed on your machine
     then don't allow the older version to overwrite the newer.

  3) Using PsiWin2 or the "Add/Remove" icon on the EPOC control
     panel (if you have it), install the main application
     *.sis file:
     
       FlCalc.sis
     
     If you get a message that the OPX SIS file has
     not been installed at the correct level then return to the
     previous step.
          
  4) Finally, look in the "Extras" draw on the machine for the new
     "FlCalc5" icon. (See following section if FlCalc reports
     an error and fails to start)

Please consult your Psion manuals for more details of these
operations, but let me know if you're having problems.

IF YOU HAVE PROBLEMS WITH OPX VERSION

In order to provide copy and paste support, FlCalc requires
the supplied version (or later) of the Cliptext.opx. Installing
the supplied *.sis files should ensure you have the required
levels installed.

Unfortunately, there may be some older and misbehaved applications
out there which do not supply the OPX correctly and these may
cause problems with FlCalc and other applications that require a
newer version. In addition, you may have problems if another
application is running and has locked the OPX file when you
install FlCalc.

If you experience messages about incompatible OPX versions,
or missing OPX files when starting FlCalc, please consider the
following tips.

  1) Close all applications and reinstall the supplied OPX
     *.sis file on to your Internal (C:) drive. Attempt to
     restart FlCalc.

  2) If you still have problems, backup your machine (if
     possible). If you have installed the "Add/Remove" icon on
     your Series 5 Control Panel, use it to remove the package
     with a name similar to "Cliptext OPX EMCC", if present. Next,
     remove manually \System\OPX\Cliptext.opx from both C: and D:
     drives, if present.
     Now, reinstall the supplied OPX *.sis file on to your
     Internal (C:) drive and attempt to start FlCalc again.

NOTE: I recommend keeping a copy of Cliptext.sis
handy in case installing another misbehaved application in the
future should regress the level of the OPX and cause problems
with FlCalc and other applications.

UNINSTALLING

If you have finished with this software you can remove the
application as follows:

  1)  Make sure the FlCalc application is not running.

  2)  Using the "Add/Remove" icon on the EPOC control panel,
      remove the "FlCalc" application.

After you have completed these steps, FlCalc will have been
removed from your machine's memory and "Extras" bar.

NOTE: I DO NOT recommend removing the small OPX file,
installed by the additional supplied *.sis package, as this
may also be used by other applications.

BASIC USE

In its basic form, FlCalc is simple to use. Simply choose
the application from the Extras bar, enter your calculation
and press Enter.

For example, try...

    5+(7-2)

By default, you can enter numbers in decimal or hexadecimal
(other formats are available using special notation), as
indicated by the second button in the status area (under the
input line). To switch between them, simply tap the button,
or choose the appropriate item from the main menu.

The results of you calculations are displayed in the large
window, with each value being displayed in multiple formats,
e.g. both hexadecimal and binary. The selection of output
format is made using the third button in the status area,
or appropriate menu items.

See the online help file (accessable from the "Tools" menu
within the application) for detailed instructions for use,
including details of operators and advanced expression syntax.

A summary of operators follows...

    BINARY OPERATORS
    +   :ADD    - add
    -   :SUB    - subtract
    *   :MUL    - multiply
    /   :DIV    - integer divide
    %   :MOD    - modulus (remainder)
    &   :AND    - logical and
    |   :OR     - logical or
    ?   :OR     - logical or
    ^   :XOR    - logical xor
    <<  :LEFT   - shift bits left
    >>  :RIGHT  - shift bits right

    UNARY OPERATORS
    -   :NEG    - negative (2's compliment)
    ~   :NOT    - logical not (1's compliment)
    #   :MIRROR - mirror image of bits
 
CHANGES

The following changes have been introduced in version 1.5:

  *  Revo support;
  *  Copy/paste support;
  *  Full input buffer editing;
  *  Improved history buffer operation;
  *  Minor bug fixes and enhancements.

CONTACT

If you have any problems, comments or suggestions about this
software, you can contact me at the addresses below.

I hope you find this software useful,

Dave Rushall, 2000

---------------------------------------------------------
dave@piecafe.demon.co.uk
http://www.piecafe.demon.co.uk/
---------------------------------------------------------
