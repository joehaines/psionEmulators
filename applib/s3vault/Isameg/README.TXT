Example of calling ISAM from OPL
================================

Files in this zip
-----------------
README.TXT      This documentation
ISAM.DYL        Copy of ISAM.DYL from SDK
LIST.OPL        Example Opl program (source)
L_FILE.OPH      Required header file
ISAM.OPG        Required header file
LIST.OPO        Example Opl program (object)
MAKEDATA.OPL    Can generate database with this
MAKEDATA.OPO    Can generate database with this
INTRO.TXT       Text version of chapter from SDK
FUNC.TXT        Text version of chapter from SDK

To see the power of ISAM
------------------------
Copy one of your favourite databases to M:\DAT\Data.dbf.
Copy ISAM.DYL into the root of M:\
Copy LIST.OPO into some \OPO\ directory and run it.
To start with, it will say "Building index".
Then it will show an ordered list of all the records in the database
that start with "A".
There's a simple editor at the bottom of the screen.
Type characters into this and see the list of records displayed
change.  Eg type "BRO" and the list shown will subset in turn, as you
type, to show those starting with
  B
  BR
  BRO

If you don't have a suitable database to hand
---------------------------------------------
You can use the supplied program MAKEDATA to make one (with random
entries - as many as you specify).

What's going on
---------------
The "index" created by LIST is a file (called Data.btx in this case,
in the root of M:\) that allows subsequent searches to take place,
within the database, very quickly.

You can delete Data.btx after you've finished experimenting.

The power of ISAM
-----------------
Even though the database file gets significantly larger, it won't
slow down the speed of searching by any noticeable amount (once the
index file has been built).

More database theory
--------------------
I can't go into the whole theory of ISAM here - there are whole books
written about it.  In case anyone's wondering, ISAM was invented
totally independently of Psion.  ISAM is a section of Computer
Science and general database theory in its own right.  But the actual
ISAM.DYL that implements this code DOES come from Psion.  It's part
of the C SDK and also part of the v2 Opl/G SDK.

If you want to use ISAM, you will have to learn a few of the basic
principles of this general theory.  But to start off with, you can
experiment with the supplied example.

The source code of LIST.OPO
---------------------------
The LIST.OPL supplied is intended to be translated using S3aTRAN.
The "include" files ISAM.OPG and L_FILE.OPH need to be found by
S3aTRAN otherwise the translation will fail.

If you're desperate, you can change the code in LIST.OPL so that all
the #define's disappear (eg you replace "FOPEN or FUPDATE" with the
value $100, which you work out from reading L_FILE.OPH).  Also you
change the C style comments into Opl style comments.  Then you can
translate LIST.OPL on an S3a.  But frankly, if you need to program
with ISAM, you ought to get yourself set up for S3aTRAN.

On the S3
---------
The program LIST.OPL presupposes S3a Opl, eg by using keywords like
enterSend and loadlib.  You can, however, change this so that it runs
on the S3 or on the HC instead, by replacing these lines of code with
their equivalents using OS or CALL.  The v2 Opl/G SDK has "macros" to
help automate this kind of thing.

Brief note on LIST.OPL
----------------------
This program illustrates ISAM usage only and should not be used as a 
template for writing an application.

Where do you go from here
-------------------------
You'll need to read some of the SDK documentation on ISAM.
I've included textualised versions of the two relevant chapters from
the C SDK with this ZIP.  They're not very pretty to read, but if you
persist, you can reconstruct what the original Word/DOS text looks
like.

Even better is to read it straight out of a paper copy of the SDK.

Suggestions
-----------
Modify LIST.OPL so that
   it puts the index in another place
   it can find ISAM.DYL in another place
   it indexes on another field, other than the first
   it uses others of the ISAM methods
   ...

These notes prepared by
-----------------------
David Wood, Psion Product Development, 23rd March 1994.
