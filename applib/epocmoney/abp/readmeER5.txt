Read me file for abp - a banking program

Freeware (c) Malcolm Bryant 1997-2001
Email malcolm@freepoc.org


Instructions for setting up abp version 5.22 (ER5 version)
==========================================================

Note: this version runs on all Epoc Release 1 to Release 5 computers.
It does not run on ER6 computers such as the Nokia 9210.

The file abp522er5.zip should include the following files:

1. abp5.sis  (installation file)
2. changes.txt (recent changes to abp)
3. sample5.abp (an example abp data file using investments)
4. sample5.txt (instructions for sample.abp)
5. readmeER5.txt (this file)
6. abpfaq (a Psion Word file with Frequently Asked Questions)
7. abpinvestments (a Psion Word file with info about new investment functions)
8. abpInternetImport (A Psion Word file with info about importing QIF/OFC/OFX
files 
   from the Internet)
9. abp4to5.opo (program to convert version 4.xx data file to version 5.xx,
see below)

There may be some additional explanatory files for non-English users

When you have unzipped to your PC, then double-clicking on the sis file will
start the
standard EPOC Install process.  Alternatively, copy the file abp5.sis
to your Psion, highlight it, and press Enter.

On your Psion, you should create a directory c:\abp   which is the default 
location for abp data files.  Don't copy any of the above files into this
directory - abp will automatically place your data file there once
you run the program.

Once you have completed the abp installation procedure, you
will see a new icon (called ABP5) in the Extras section.

When you installed abp, you were given the choice of installing to c: or d:.
However I recommend that you keep your DATA FILES on the c: drive whenever
possible.  This is because the abp writes to disk frequently - good 
for the security of your data, but slow on a flash disk.  Holding your
data files on the c: drive will result in better response time.

For features of abp and instructions, please take a few minutes to
read the help file (press CTRL+SHIFT+H from within abp)


Note for upgraders from abp version 4.xx to version 5.xx
========================================================

abp version 5.00 contains many new features.  Unfortunately, I have
had to modify the abp data file format and included in the 
zip file is a program abp4to5.opo which will update your existing
data file to the new format.  As a safeguard, the program will ask
you for a new filename, so that your original data is left intact.

To use this program, copy it into any convenient directory on your 
Psion (it does not have to be in the same directory as specified
for the main abp program above).  Then run the program and follow 
the onscreen instructions - it's easy!

In version 5.00 you can set up multiple investment accounts.  When you run 
the program abp4to5.opo, if you currently have any investments then 
you will be asked whether you want to place those investments in an
existing account, or set up a new account.  You will be asked this 
question for each investment.  For example, you might want to keep any
foreign investments in separate accounts from your home currency investments.

Once the conversion is done, you should select abp5 from the Extras bar, or 
alternatively select the name of your updated data file from the System
screen.

IMPORTANT NOTE: Since ABP5 is a major version upgrade, it will NOT overwrite 
your previous version ABP4.  You will therefore see both an ABP4 icon and an
ABP5 icon in your Extras Bar.  When you have successfully upgraded your data
file to version 5 and you are happy that everything has gone smoothly, you
will probably want to delete ABP4.  To do so, go to the System Screen and
select Control Panel, Add/Remove.  Then delete ABP4 from the list.



Enjoy abp - the freeware banking and investment management program!  Let me
know 
if you like it.


Malcolm Bryant
malcolm@freepoc.org
