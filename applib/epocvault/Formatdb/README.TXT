Format Psion Database, a PC program to support the Psion Series 5

Purpose
-----------

A simple program for anyone who wants to import a number of text files for review on the Series 5. The program takes a list of text files, and formats the files into a single file which may be imported into Psion Data. I wrote the program to fulfil a particular need, and then found myself using it quite a lot. I have tidied it up and made it available in case this is useful to anyone else.

The files are formatted into two fields - a "header" field which contains the filename, and the "Memo" field which contains the text of the file.

A Psion data file is included which will take imports from this program.


Usage
--------

Select the files to be read. Select a directory and input a name for the output file. Alter the text mark character (this is the character which Psion Data uses in its import to decide that the field has ended). Any instance of this chosen character which is in this file will be filtered from original text - the "~" character is selected by default as an unlikely character to be used in text files. This may be changed to any other character you wish (though make sure you can type it from the Psion keyboard).

Press the "Process" button. File names will appear in the Results box. This may be reviewed upon completion.

Move the completed merge file to your Psion 5. Load up the Data file for import (the supplied file is designed for this purpose). Select File/More/Import Text File. Select the name of the file and the appropriate  Text Qualifier in "Options" (this should be the same character as was chosen when using the PC program). The Psion should then import the files into the new database.

I have successfully used this program to import 200 text files, with lengths of 1 to 40k per file, and had no problem in using the Data app to review and read them at leisure. Data is a lovely app!


Known Limitations
------------------------

The program is written in VB3, so no long filenames are supported. Works fine in Windows NT/95 otherwise.

When I get hold of a description of the Psion 5 Data format, I may rewrite this program to output Psion Data files directly. In the meantime, I hope the program is useful.


Version
----------

Version 1.0 March 1998


Disclaimer
------------

In using the supplied program, you agree to the following conditions:

This program is FREE, and supplied without any warranty of any kind as to its function. 

If it works for you, great. If it scrambles all your data, you accept you have no claim on the author.


Author
----------

Any problems, contact Cornelius.Mawby@TheOffice.Net

