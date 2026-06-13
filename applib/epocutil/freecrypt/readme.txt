Instructions for setting up FreeCrypt 1.02
==========================================


What is FreeCrypt?
------------------

FreeCrypt is a small freeware program for Epoc computers which encrypts
and decrypts your sensitive data.  The program was originally written
by Roger Burton West and FatCatz, and has now been updated and converted
for ER6 machines by Malcolm Bryant and FreEPOC.


Objective of the program
------------------------

FreeCrypt uses the secure RC4 algorithm to encrypt your files using a
user-supplied password.  Since many people use their Epoc computers
for sensitive data (eg. credit card details etc), FreeCrypt prevents 
this information being accessed if your computer falls into the wrong hands.


Advantages of FreeCrypt
-----------------------

Unlikely some other "security" programs which insist on storing your
data in a proprietory format, FreeCrypt will encrypt and decrypt any
Epoc file (eg. Word, Sheet, Data etc).  Since the source code is available
you can check for yourself that there are no "back-doors" in the program.
Because the program uses no OPX plug-ins, it will work on both the Epoc
R5 Emulator and the Nokia 9210 emulator.


Disadvantages of FreeCrypt
--------------------------

It can be quite slow to process large files.  It works best on small
files.


Warning
-------

We accept no liability for any data that may be lost using this program.
In particular, if you lose or forget your password, we cannot restore your
data (as stated above, there are no "back-doors").  Always take backups
of your valuable data!


Setting up FreeCrypt
--------------------

To install, double-click on FreeCrypt.sis in Windows Explorer or transfer 
the file to your Epoc computer and install it from there.  If you 
have installed the files successfully, then you should see the
FreeCrypt program appearing in your Extras bar.

Important: before using the ER6 (Nokia 9210) version of FreeCrypt, you must install 
the OPL runtime environment which is available from http://www.symbiandevnet.com/.  


Running FreeCrypt
-----------------

Select FreeCrypt from your Extras bar.  Select the file that you want to encrypt
and choose whether you want to delete the original.  You will be asked
to specify a password of at least 5 characters.  Files which have been
encrypted using FreeCrypt will appear with a padlock icon.  To decrypt
an encrypted file, just highlight it in the System screen (or in File
Manager for ER6 computers) and press Enter.  Once you have specified
the correct password, your data will be restored.


I hope you find the program useful.


Malcolm Bryant
February 2002

malcolm@freepoc.org

