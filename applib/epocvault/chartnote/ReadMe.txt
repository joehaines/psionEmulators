ChartNote

by Kimo Hirayama

ChartNote is a simple patient record program that allows easy,
quick entry of clinic visit notes.  It is not meant to serve
as a full-fledged patient information system.  Once notes are 
entered, they can then be printed out to be entered in the
patient's paper medical record.  

Chartnote allows both free-form entry of notes in the traditional
SOAP format, and entry of information via the use of customizable
plug in scripts that can easily be created using the standard
Psion 5 Data program.

Printing is done via Psion's Printer OPX routines, so allows some
formatting options. 

This is a beta version, and as such, is free.  The program is fully
functioning and allows entry of up to 38 patients per day.  There
are a few niggling bugs present, and I plan to add a few more features
to the final version, but I think it still works quite well as it
stands.  

The main limitations at present are: only one report print format is available; there is no 
option to output to a file; each section of the note can not be 
longer than about 500 characters (though I think this is enough
for most notes!); there is no "find" option to find a patient. 

Any comments, suggestions for improvement or additions are welcome.

Included files:

ChartNote.app
ChartNote.aif
ChartNote.mbm
ChartNote.hlp

Readme.txt

PlugIn template	\
OB.s		 \
OB.o		  \__ these PlugIn files are not required, to 
OB.p		  /   operate the program, but give some indication
General PE.o	 /    of the flexibility and power of the program.
Story.s		/

All files should be installed to the directory /system/apps/ChartNote

Thanks to Alan Richey for developing the RMREvent core that I
used to write this program.

If you have any questions, comments or suggestions, please feel 
free to contact me!

Also, if you develop any good plug-in scripts that you would like
to share, email them to me and I will try to add them to my web site
at:

	 http:\\members.aol.com\Kaichi\Chartnote.htm

Kimo Hirayama
email:	hirayama@u.washington.edu
	kimoh@ichs.com
