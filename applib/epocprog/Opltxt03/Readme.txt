OPL32TXT.EXE

Version 2.01b 20th December, 1997

This is a simple utility which will run in MSDOS. You can also run
it in Windows 95, see Appendix A.

The purpose is to convert OPL32 source files from a Psion Series 5
into plain text files. I wrote this so that I could easily view OPL
source files, from my backups, on a normal PC.

OPL32 source files have a header which identifies them to the Program
Editor on a Series 5. The OPL code begins at either address 38 decimal
or 40 decimal, depending on it's size, and is terminated by a byte of 
value 01.

This converter extracts the code from address 38 or 40 decimal until 
a byte value of 01 is found. It also converts any bytes of value 06 into
a standard CR, LF pair.

Error trapping is absolutely minimal. However, it now checks UID 3 for
validity, and will not run if this UID is incorrect.

Use it from a DOS prompt like:

	OPL32TXT infile outfile

Both infile and outfile names are required. Being a DOS program, it
understands only 8+3 filenames, so if you are using Windows 95 remember
to feed it the short version of the filename.

If there is any interest in this program, I may make it more clever.

It was written using an old version of Turbo C++


APPENDIX A

You can run OPL32TXT in Windows 95 using the "Send To" (right click) menu,
or by dragging a file to an icon on the desktop.
Firstly, construct a file OPL32TXT.BAT containing the following 2 lines:

opl32txt %1 c:\temp\temp.txt
move c:\temp\temp.txt %1.opl

This assumes you have a \temp folder.
Now construct a shortcut on the desktop (right-click on the desktop, 
select New, Shortcut) and browse to wherever you keep OPL32TXT.BAT.
You can now convert files by dragging you OPL32 source file to this icon
on your desktop. Also, if you place the shortcut in your Windows\SendTo\
folder, it will appear on your right-click Send To menu.
To prevent the DOS windows from staying open, right-click on the shortcut,
select Properties, Program, and tick Cose On Exit.

DISCLAIMER

Use this at your own risk!

AUTHOR

Roger Muggleton
hzk@cix.co.uk

20th December 1997.

