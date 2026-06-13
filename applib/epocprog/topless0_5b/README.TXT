#############
# T-OPL-ess #
#############


History:
--------

25/08/98 : v05b : Quick modification for so-called 'protection programs'
                  based on forward goto, then random scrambled string.
                  Modified the 'getstring' function for code<32 or >128.

10/02/98 : v04b : Modified README (Tabulation)
                  There was inversion for Cmp== and Cmp!= codes
		  Removed old serie3 OPL codes

29/12/97 : v03b : Some Op-code added
                  Tryed to add reference tables (seems to work...)
                  Added IEEE conversion code (to check)
                  Improved README.TXT
                  Addendum to PSIONICS FILE - OPO.FMT
                  (C code is uglier than ever, so I decided not
                  to distribute it any more)
                  
17/12/97 : (yes, the same day)
           v02b : Some Op-code added
                  OPX ID and Rev. corrected
                  Memory5 (my working reference) can now be completly converted

17/12/97 : v01b first release

1/ WARNING
----------

Note : THIS IS NOT AN OPL32-DETRANSLATOR !!!

This program only converts (and not completly for the
moment) OPL32 to Pseudo-code (refered as Q-code in OPO.FMT
from Psionics file).

This program is based on adaptation of information found in this
file and adapted to Psion5.

Some of the OP-codes have been checked, some other are taken straight
from the OPO.FMT document. Some OP-codes have been added such as
POPUP, GETEVENT32...
The OPL OP-code table is not complete and I am waiting for your
comment in order to improve it.

2/ USAGE
--------

TOPLESS name_of_opo_file >name_of_text_file

Still no full syntax check & no options in the command line
(No arguments = debug mode, 1 argument = normal mode)

3/ SOURCE
---------

Before v03b:

Source is given so that you can improve it (in particular for
parameter printing).
It was compiled using DJGPP 2.x (TOPLESS.EXE) and (TurboC Atari, coz
I only have that computer at home : TOPLESS.TTP).
It should compile with any other compiler...

No comments in the C source ! (Really badly programmed !!!)

After v03b:

I doubt Source code will be of any use to anybody except me...
since it is really really really ugly...
I even doubt I will continue maintaining it if I don't get much
much more feedback...

4/ INTEREST
-----------

First interest of this program is to look exactly how the translator
is doing its job :
My first comment is that it is a very simple translation without any
optimisation and thus optimisation of OPL32 code could be a very
interesting task to do.

First example : you program ' print 3.0*4 '

Translator will output : Push()* 3.0    (9 bytes)
			 Push()% 4      (2 bytes)
			 %to*           (1 byte)
			 Mult*          (1 byte)
			 Print*		(1 byte)
			 PrintCR	(1 byte)
(Total 15 bytes)

Second example : if you define a constant (good idea) like this

		' CONST Key_Pressed=&400 ' (bad idea)
		
Then any comparaison will be translated as follows :

		Push%() value to be compared
		%to*
		Push()* 0x400 coded as float (9 bytes !!!)
		Cmp!=*
		
Thus, don't forget in your constant declaration to put & or %

		' CONST Key_Pressed&=&400 '
		
it will save 8 bytes for float coding (by comparaison) and its
execution will probably be faster.		

Last example : Start of a program using TOOLBAR.OPO

    0 -> PushL^%   3f -> Global decl. : SCRWID%
    3 -> Ext:gWIDTH 
    5 -> Assign% 
    6 -> PushL^%   41 -> Global decl. : SCRHGHT%
    9 -> Ext:gHEIGHT 
   11 -> Assign% 
   12 -> Push$ 'z:\system\opl\Toolbar'
   35 -> LOADM 
   36 -> Push$ 'Main'
   43 -> Push%    3
   45 -> Call(name$)   35 -> Procedure call : TBARLINK (1 parameters)
   48 -> Void* 
   49 -> Ret* 
   
is the translation of :

Proc Example:
GLOBAL SCRWID%,SCRHGHT%

SCRWID% =gWIDTH			(PushLocalPointer SCRWID%,gWIDTH,assign%)
SCRHGHT%=gHEIGHT		(PushLocalPointer SRCHGHT%,gHEIGHT,assign%)
LOADM "z:\system\opl\Toolbar"   (PushString"z:\...",LOADM)
TBARLINK("Main")		(PushString"Main",pushType3,Call$"TBARLINK",Void*)
endp				(Ret*)
   
5/ OPL32 format
---------------

Differences with OPL16 is the first header format (ID,ID,ID,??,ofset to table)
Second header is also different (ID,version,version,path,procedure,OPX).
The remaining part is (I think) quite similar.

If you take the PSIONICS FILE : OPO.FMT as a reference, then
modify the first tables by :

The first header has the following format :
-------------------------------------------
Offset  0 (long) : ID 0             }
Offset  4 (long) : ID 1             }    This header seems to be
Offset  8 (long) : ID 2 (ID of APP) }    common to any Psion5 file.
Offset 12 (long) : ????             }

Offset 16 (long) : pointer to second table

Second table format :
---------------------
Offset  0 (long) : ID 3 (ID of translator ???)
Offset  4 (word) : (Could be Translator version : $200A)
Offset  6 (word) : (Could be required runtime version : $ 200A)
Offset  8 (long) : Offset to PATH string
Offset 12 (long) : Offset to procedure table
Offset 16 (long) : Offset to OPX table

Procedure table :
-----------------
Procedure table has the same format as the one described in the
OPO.FMT file.

OPX table :
-----------
Offset  0 (word) : number of OPXs

then (nb of OPXs times)
/ qstr : name of OPX
| long : OPX's ID
\ word : revision of OPX


6/ For more...
--------------

Contact : jjm@han.daewoo.fr

                                                   Jean-Jacques MICHEL


PS: Why TOPLESS ? Just because of the inclusion of OPL in the word
(and LESS...)
