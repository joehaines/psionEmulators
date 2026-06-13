Psion 3a/Pocketbook II FORTH language
=====================================

BETA TEST VERSION 1.29 - 17/04/95

This software is copyright Steve Godfrey

These notes assume you are familiar with the Forth language.

Installation:

Copy Forth.opa into any \APP\       directory
Copy Forth.bin into any \APP\Forth\ directory
Copy Forth.ini into any \APP\Forth\ directory
Copy  Demo.sav into any \APP\Forth\ directory

and install Forth.opa in the usual way. Saved files are listed
below the Forth icon on the system screen.

The demo has the following words defined: (most pointless!)
-----------------------------------------

variable count
variable primes 200 allot
init                        (used by go)
show                        (used by go)
check                       (used by go)
go                          works out 200 prime numbers and prints them
bars                        draws bars on screen (shows use of graphics commands)
2dup                        (used by fact)
2drop                       (used by fact)
d1-                         (used by fact)
d0=                         (used by fact)
fact                        (used by n!)
n!                          works out factorial of number on top of stack (upto 13)
primes                      displays prime numbers (uses variables 'primes' and 'count')
rnd                         displays 100 random sized/placed boxes on screen.

Please inform me of any suggestions, bugs etc by email to:

stevegodfrey@cix.compulink.co.uk
stevegodfrey@warp10.demon.co.uk

or arcade user #897

****** WARNING: Mis-use of the Forth language could
                crash your psion. Please save any
                unsaved data in other applications
                before using.

The core of this FORTH language is written entirely
in assembler for maximum speed. An OPL front end is
used for simplicity. All commands entered are compiled
before being executed. Any new definitions you create
are compiled ready for future use.

Later versions will allow the use of ROM routines
for graphics drawing, serial i/o, sound etc.

Words defined in version 1.29 of Psion 3a Forth
-----------------------------------------------

forget   time     +        -        *        /        /mod     mod
.        u.       d.       ."       at       cr       do       loop
+loop    if       else     then     drop     dup      emit     i
i'       j        >r       r>       r@       over     pick     roll
rot      swap     vlist    2*       2/       2+       2-       1+
1-       and      or       not      xor      <>       0>       0<
0=       d-       leave    cls      negate   >        <        =
<=       >=       sp@      s0       @        stack    variable !
draw     box      fill     d+       d*       d/       beep     allot
seed     rand

Notes:
======

A maximum of 32 nested do..loops can be used.

A maximum of 8 if statements can be nested.

The IF statements can be used with or without the ELSE statement, but if the
ELSE statement is present, it *must* be between an IF and a THEN.

There are currently no checks on the state of the stack except with the
. u. and d. functions. It is up to you to make sure that the stack has sufficient
entries for the function used ie 'rot' requires 3 numbers on the stack but
will work without. It may, however, crash the computer.

Mis-use of the language will usually give a harmless Panic 60 error, but worst case
could require the psion to be reset, which could lose any unsaved data in
other applications that are running.

Each word you enter must be seperated by a space.

Defining new words:
===================

You may define new words as follows.

: <word> <func1> <func2> <func3> ... ;

ie to define a function to print a given number of stars, you could use:

: stars 0 do 42 emit loop cr ;

You can then type '5 stars' to print 5 stars, or '10 stars' to print 10 stars etc.

You can use any defined word in furthur words, eg: another word could then be
defined to print a triangle of stars as follows:

: triangle 10 1 do i stars loop ;

Entering 'triangle' would produce:

*
**
***
****
*****
******
*******
********
*********

Remember that forth uses reverse polish notation for calculations.

To add and print the sum of 5 and 7, enter

5 7 + .

To print (5*6)+(9-7)*88, enter:

5 6 * 9 7 - 88 * + .

/ returns the int of the division. mod returns the remainder.
/mod returns both the remainder and the quotient on the stack.

Any number of do..loops can be nested, eg:

: test 10 0 do 10 0 do i j * . loop cr loop ;

which prints the multiplication tables from 0 to 9 (no print formatting
available yet).

Strings are used as follows:

: sqr dup dup ." The square of " . ." is " * . ;

Which produces:

>5 sqr
The square of 5 is 25

Any space before the terminating " will be printed as part of the string.
The space is not required to work, but you must have a space after the ."

You can do a reverse counting loop using +loop. Ie to print 10 to 1 enter

1 10 do i . -1 +loop

Or to count in 2s, use

10 0 do i . 2 +loop

For long words you can split the definition over several lines, eg:

>: stars
    10 1
    do
      i
      0
      do
        42 emit
      loop
      cr
    loop
    ;
>

Each definition must end with a ; and start with a :

If you try and use an unknown word in a new definition, the new entry
is aborted with an error.

If you have an unequal number of dos and loops or ifs, elses and thens
an error will be reported and compilation aborted.

You can save the words you have defined using the menu. Any saved
files in the \APP\Forth directory with extension .sav will appear
under the system icon. Starting the language with a saved file
will load the saved definitions. Saved files can be loaded
into DATA for editing. Forgotten words are saved in the
data file, but ignore when reloaded. They can be retrieved
using DATA.

SETUP
=====

The setup parameters only take effect when you next run FORTH.

As a rough guide:

Parameter stack uses 2 bytes per item.

Return stack uses 4 bytes per do..loop. Return stack of 256 allows 64 nested
do..loops.

Dictionary size. Each stored word uses 5 bytes+length of name eg

: test 42 emit 34 emit ;

uses 9 bytes in the dictionary.

Code size is the space allocated to compiled words. 4k should be enough.
(The forth code itself is only 4k-ish!).

-----------------------
Steve Godfrey, 17/04/95
-----------------------
