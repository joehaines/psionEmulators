Travlang 1.0, a language translator for the Psion 3a,
Copyright 1995, Michael C. Martin (database and audio files)
Copyright 1997, Chris Walker Travlang program for Psion

We can be contacted via email at:

	Chris Walker - CGH.Walker@ulst.ac.uk
	Michael C. Martin - president@travlang.com

Contents of this package:

	readme.txt      - this file
	travlang.dbf    - Database containing words and phrases in
			  different languages.
	Travlite.dbf    - Smaller version of the above.
	Travbig.dbf     - Large version of Travlang.dbf
	Travlang.opa    - Travlang driving program
	*.wve           - Various audio files

Installation:

	1) Copy travlang.opa to \APP\ on any drive.
	2) Copy travlang.dbf to \DAT\ on any drive.
	3) Install travlang.opa using psion-i from the
	   Psion system screen.
	4) Copy any audio files into their correct location
	   (See below)

Audio files:
There are a considerable number of audio files available with the
package, but they can take up a large portion of memory. When you
select 'Play' after finding the required word, then a message will
appear stating it cannot find a certain file in a certain directory
(e.g. \wve\tl\l5\l78.wve). In order to play the relevant file, you
must:

	1) Create the relevant directories.
	and then either
	2a) Download the file from the travlang website
	    (http://www.travlang.com) and give it the name that
	    the travlang program requires.
	or
	2b) If the relevant file is already available in this
	    package, copy it across making sure you put it in the
	    correct directory.

If you choose option 2a, you will have to convert the audio file
from the Sun/NEXT format (extension .au) to the RIFF format
(extension .wav) and then to the Psion format (.wve). There are a
number of free packages available for converting files from .au to 
.wav format which can be downloaded from the internet. The Travlang
website has a number of links to pages where these programs may be
obtained. The Psion Manager can be used to convert from .wav to
.wve. If you have option 2b available, these conversions will already
have been done. Let Chris Walker know if you convert any of the audio
files, so that the package can be extended for others.

Using Travlang with the database manager
   All of the databases (Travlang, Travlite and Travbig) can be used
with the Database manager. Using Travlite with this approach allows
the least use of memory. Simply type in a search string to find the
word that you require. 
  If you use the freeware package JBdata, then you can delete certain
languages from any database to reduce memory requirements. You can also
delete any record, or introduce new words or languages yourself to create 
a custom database.

Using Travlang program.
   The Travlang program uses the travlang.dbf file placed in the \dat\
directory. You can also use the travlite database, but it's name must
be changed to travlang.dbf for the Travlang program to read it. Any
record (i.e.word) or field (i.e.language) may be deleted or modified, 
except for the first 3 records which the Travlang program requires to 
function properly. If you do delete fields after you have installed 
audio files, then you will have to move certain audio files for other 
languages to new directories, but you won't have to rename those files. 
If you delete records, then audio files can remain in the same directory, 
but you will have to rename files to correspond with the correct 
tranlation.
   The Travbig database cannot be used with the Travlang program as it 
has too many fields and it has too many characters per record. You can
delete certain fields (ie languages) from Travbig (using JBdata)
until Travlang can read the file. Again you must change the name of 
the file to travlang.dbf. About 27 languages is the most that the
Travlang program can read. 

Possible bugs:
The package has not been tested on A and B drives as I don't have a
solid state disk.

Source code:
The source code could be made available on request, as long as you
have a reasonable reason for wanting it (ie extending it's capabilities).

Chris Walker
DABCS
University of Ulster
Cromore Road,
Coleraine,
Northern Ireland
BT52 1SA.

