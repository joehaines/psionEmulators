*** ReadMe file for CodeSafe (7.29) - 14 October 2001 ***

UPDATES

1. Before you upgrade please copy your codes to file. Everything should go
fine, but if not, you can restore the database manually.

2. For users of versions before v.6.3, please note that the database
of your definitions will be reformatted. Therefore, backup, do -1- above,
and be absolutely sure to enter the correct master key the first time
you use this new version.

3. Users of v.5.92 and earlier: please copy your "USERNAME".C$F file from
C:\ to C:\System\Data\. It may be necessary to set
"System | Tools | Preferences | Show hidden files" to Off
.

7.2
*Removed Cliptext dependency
*Updated for Cliptext v.3.0
*Fixed bug preventing printing of 255 character long entries
*Added "Find all"
*Unlimited number of databases
*Improved Timeout checking
*Added configurable Full screen or Windowed (Transparent) operation
*Configuarble disk for Data storage
*English, Dutch, French, German, Danish and Italian language resource files
*Changed "Paste" to "Input", freeing real paste
*Check for input when System is foreground

7.1
*Added "Go to entry no.", configurable "Show entry numbers"

7.0
*Rigorously changed menuing. Now much user-friendlier
*Beeps may be switched on/off, adjustable Time-out time, opens last used
 database
*Preferences stored in CodeSafe.ini
*Added Extract
*Improved program handling
*Unlimited number of entries

6.3
*Easy install using sis-installation.
*Enlarged the "secret code" field to hold 255 characters (multi-line).
*Other enhancements to improve speed and user-friendliness.

6.2
*Added master key verification. For each "User"-database only those entries
will be shown that were stored using the current master key.
*Added automatic copy to clipboard function. The decrypted code can be pasted
into the underlying document using <Ctrl+V>. When connected to the PC with
CopyAnywhere (PsiWin 2.3) active, the secret code is available for immediate
pasting on the PC. This enables insertion of your password into input boxes
e.g. for access to internet sites. The clipboard is cleared when CodeSafe is
closed.
*Closing CodeSafe returns the previous application (instead of System) to
foreground. As this was the main difference between the program and macro
versions of previous CodeSafe releases, version 6.2 is now provided as
program only. Macro users can of course install CodeSafe as a program in
Macro5 and run it using a hotkey.

5.93
*Added timeout function. For safety reasons you need to re-enter the master
key when stepping to the next code after pausing for more than 60 secs.
*The database is now stored in C:\System\Data\. See above for transferring of
your existing datafile.

5.92
*Added search facility




DESCRIPTION

CodeSafe is an OPL program to store and retrieve secret codes and passwords.


MOTIVATION

The ever-increasing number of codes and passwords that one has to have at
hand in order to get around a little comfortably in this high-tech society,
is putting a big drain on many a brain. Especially Psion users, pampered by
the brain-extending abilities that their Psions offer, possess merely
rudimentary skills necessary to memorise this kind of trivia (at least that
holds for me). That is why I decided to write this utility. The idea behind
CodeSafe is to be able to temporarily decrypt the encrypted codes from a
database, using only a single master key. I hope it will prove as
indispensable for you as it does for me.


EXPLANATION OF CODESAFE'S FUNCTIONING

After starting the program CodeSafe asks for a username and master key.
The characters of the master key are converted to their numeric values and
the numbers of this array are added to the numeric values of the subsequent
code or password to be stored. The numbers that result from these
calculations are then converted back to characters and stored in the
database. Upon retrieval of a code from the database, the calculations are
reversed, resulting in a text string. All combinations of letters, numbers
and other characters may be used for the master key, and the master key
may be from one to sixteen characters long. Finally, the Achilles heel of
many password protected computer data is the storage of the (master) key
in some way in the program or data files. CodeSafe does not allow that the
master key itself be stored. The same CodeSafe program can even be used
by different users. As long as each uses a unique master key, he/she has
access only to his/her own data.


INSTALLATION

Before you install the new version backup your codes by running "Write to
file" from the old version of CodeSafe.

Run CodeSafe.sis from the PC running PsiWin and your EPOC machine connected
                                    -or-
run CodeSafe.sis from your EPOC machine.

While running for the first time, CodeSafe will create the database file
("USERNAME".C$F) on the internal drive. If you've set EPOC's System to hide
your hidden files, the *.C$F file will not show up in the System screen.


DISCLAIMER

I've tested CodeSafe on my English Psion S5 and 5mx, where it performed
perfectly. However, needless to say, I don't give guarantees and you cannot
hold me responsible in case anything goes wrong.

BE CAREFUL, if you forget your master key, there is no way I can help
to resolve your encrypted information.

CodeSafe is freeware. You are free to use and copy the program.


ACKNOWLEDGEMENTS

Thanks to EMCC for Cliptext.opx and to Eric de Bruijn for his thorough
testing of version 7.


NOTES

This is an OPL16 macro version of CodeSafe for MacSys on the S3a/c
Psions. Previous Psion S5 versions were running as macros under
SwitchTask/Macro5 and Backlite+Plus.

If you like CodeSafe, please give me feedback. I'm interested in positive
criticism, comments, and ideas. I'll keep on modifying and improving
CodeSafe as I've done in the past. Please visit my website to download the
latest version and subscribe to the ExAbEtAl mailing list for announcement
of updates of CodeSafe and my other programs.


Enjoy CodeSafe

Huub Linthorst
huub.linthorst@bigfoot.com
http://website.leidenuniv.nl/~linthorsthjm/
