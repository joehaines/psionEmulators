                The PC OPL translation programs
                ===============================

The \OPL directory contains the following programs which can be used
to translate standard Series 3/3a and HC OPL files into OPO files
which can be run on the computer concerned:

 S3TRAN.EXE    for translating OPL files to be run on the Series 3
 S3ATRAN.EXE   for translating OPL files to be run on the Series 3a
 HCTRAN.EXE    for translating OPL files to be run on the HC

Using the translation programs on Windows-based PCs
---------------------------------------------------

1. Display the DOS prompt window, or select the `Run' option on the
`File' menu in the Windows Program Manager.

2. On the DOS command line, or on the `Command line' line of the
`Run' dialog, type the drive and directory in which you installed
RCom, then \OPL, then the name of the translation program you wish to
use, then a space followed by the file specification of the OPL file
that you wish to translate. For example:

C:\PSRCOM\OPL\S3ATRAN.EXE C:\OPL\MYPROG.OPL

Using the translation programs on DOS-based PCs
-----------------------------------------------

1. Display the DOS prompt and make sure that the current directory is
the directory in which you installed RCom.

2. Type CD OPL to change to the \OPL subdirectory where the
translation programs are stored.

3. Type the name of the appropriate translation program and then the
path of the OPl file to translate. For example:

S3ATRAN C:\OPL\MYPROG.OPL
 
About the translation programs
------------------------------

Any existing .OPO file of the same name will be overwritten by the
tranlation process.

You can use wildcards in the file specification, if you wish. For
example, S3ATRAN *.OPL  translates all .OPL files in the current
directory.

If the translation fails, the translation program will report the
first error it finds in each procedure. If you use the Brief
programmers' editor, note that the error messages displayed are in
the format required by Brief. (For more details, see your
documentation for Brief.)

The translation programs contain a preprocessor which allows you to
use C-style comments, header files, symbol definitions and
conditional translation.

Comments
--------
In addition to the REM statement in OPL, comments may be placed
between `/*' and `*/'. Such a comment can spread over many lines -
e.g.:

    /* this is a comment which
    is 2 lines long */

This may be useful for temporarily "removing" a block of code during
debugging, or when writing long comments.

Symbol definitions
------------------
Use  #define  to give meaningful names to numeric constants - for
example:

    #define DISKFULL -37

You can then use this symbol definition instead of the number - for
example  RAISE DISKFULL  - making your program easier to understand.

Symbols are also used to define parameters which may change in
different versions of a program, such as:

    #define SCREENWIDTH 40

You can define a symbol to save repetitive or error-prone typing:

    #define CHECKMSG "Checking files"+chr$(7);

You can also define a symbol to control conditional compilation - see
below.

Conditional Compilation
-----------------------
If you use, say,  #define DEBUG  (you don't need to define it AS
anything) you might later use debugging code, like this:

    #ifdef DEBUG
      AT 1,1 :PRINT "c% is currently",c%
    #endif

Here, the translation program translates the line(s) between  #ifdef 
and  #endif  only if DEBUG has been defined as a symbol. When you
want to make a final version of your program, you can remove the 
#define DEBUG, and debugging statements such as this one will not be
translated.

If you use  #ifndef  instead of  #ifdef  the subsequent code will
only be translated if a symbol has NOT been defined.

You can use  #if  to control compilation in a similar way to 
#ifdef/#ifndef. It evaluates some condition based on your symbol
definitions - for example:

    #if SCREENWIDTH<40
      PRINT "Not supported on this screen"
    #endif

You can use brackets and most standard C operators in these
conditions. These operators are as in OPL:
    >  <  >=  <=  *  /  -  +
These are different to OPL:
    ==  equals
    !=  not equals
    &&  logical AND
    ||  logical OR
    !   logical NOT

You can use  #else  and  #elseif  with  #ifdef/#ifndef/#if  in the
same way that you use ELSE and ELSEIF in OPL.

Header files
------------
You can tell the translation program to include a different file at
a given point in a program, by using #include. For example:

   #include "commsdef.oph"

includes a file called `commsdef.oph' from the current directory.

Typically you would use  #include  with header files (also called
"include files") containing symbol definitions which you want to use
in more than one OPL file.

Command line options
--------------------
Use the  -i  flag to define a path where header files may be found -
e.g.:

    S3ATRAN *.OPL -ic:\src\h\

To include such headers, you should use angle brackets in the 
relevant  #include, in place of quote marks - e.g:

    #include <general.oph>

If your program includes filenames using quote marks in the #include,
the translation program will still look for it in the current
directory.

Use a separate directory for header files if more than one OPL
project needs to use the same OPL headers.

The  -d  flag allows you to define up to 9 additional symbol
definitions for this one translation to use. You might, for example,
type this:

    S3ATRAN MYPROG -dDEBUG -dSCREENWIDTH=80

to produce a particular version of your program. Note the use of an
`=' sign, when defining a value for a symbol.

Computers supported
-------------------
S3TRAN, S3ATRAN and HCTRAN can translate the full Series 3
specification of OPL; if you use an HC you should use only the
commands and functions available in its version of OPL. If a
translated OPL program contains an instruction specific to the
Series 3, and you try to run it on a different computer, it will
stop when this point is reached.

None of the tranlation programs are for use with Psion Organiser II
OPL programs.

