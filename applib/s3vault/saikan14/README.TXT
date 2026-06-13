            Saikan V1.4 -- Kanji Dictionary for Psion 3a/3c

                 Copyright (C) 1996,1997 Jamie Packer


INTRODUCTION
============

Saikan is a Japanese kanji character dictionary. For more information see the
file MANUAL.TXT. 

To install this application you will need a total of approximately 960 KBytes
of free disk space for the essential programs and data, plus an additional 23
KBytes to load the optional files (help, etc).

This application was developed and tested on a Series 3a. It also appears to
work on a 3c. The program will _not_ run on a Series 3 or the Siena.

This program is freeware -- no charge is made for using it. However, if you
find it useful, please let me know and I will inform you of new releases and
any other developments.

For more information, and the latest version, check out the Saikan web page:

		http://www.gingko.demon.co.uk/saikan/


COPYRIGHT
=========

The program and data files are free but they are NOT in the public domain. You
may make copies of this program and the data files for redistribution, etc.
These copies must be complete and unaltered. In particular, the file MANUAL.TXT
and the file KANJIDIC.DOC MUST be included complete and unaltered. No charge
may be made for this program and the data files other than reasonable costs of
media.

See the section `References And Copyright' in the file MANUAL.TXT for more
details of copyright etc.


INSTALLATION
============

To install this application you will need a total of approximately 960
KBytes of free space for the essential programs and data, plus an
additional 23 KBytes to load the optional files (help, etc).

The archive SAIKAN.ZIP contains the following files:

	File		Directory	Description
	-------------------------------------------------
	readme.txt	-		This file
	manual.txt	-		The user guide
	kanjidic.doc	-		Text file describing kanjidic
	saikan.opa	\app		The application code
	radkanji.dat	\app\saikan	Radical index data file
	saikan.hlp	\app\saikan	Online help file
	saikan.pic	\app\saikan	Background image for main screen
	k2r.dat		\dic		Kana to romaji conversion data
	r2k.dat		\dic		Romaji to kana conversion data
	kanjidic.opd	\dic		Kanji dictionary data file
	pjis208.24	\fon		24x24 bitmap font file

The files should be copied to the directories shown above. They can be
installed on any disk. Once the files have been copied to the Psion, use the
"Install" menu option on the system screen to install the Saikan application.

NOTE: The file \app\saikan\radicals.pic from earlier releases is NOT needed 
anymore and should be deleted.

The files KANJIDIC.OPD and RADKANJI.DAT are discussed in the section
`References And Copyright' in the manual.

The files SAIKAN.PIC and SAIKAN.HLP are optional. They can be omitted to save
disk space.


CHANGES FROM V1.1
=================

New Features

- Added "JIS table" lookup function.

- Much faster kanji drawing code (and removed the 'pause display' code which
  was a real mess).

- Readings can now be displayed in either romaji or kana.

- Esc key now returns to previous search step -- this seems to be much more
  usable.

- Added "redraw" command to the kanji information display menu.


Bug Fixes

- PageUp in kanji display for index lookup went forward instead of backwards.

- 'Back' and 'Previous search step' from cross-ref or radcal-info behaved 
  differently. The 'Back' menu has been removed as it was redundant.

- Fixed bug with readings of the form 'miti' (where 's' could be any consonant)
  which caused them tp be displayed as 'myt'.

- Fixed a bug which crashed the program when attempting to display information
  on certain radicals from the kanji display screen.

- What I thought was just a minor problem with the status window not being
  updated when switching pages of kanji turned out to crash the program in
  certain circumstances.

- Added option to display readings in kana ('on' readings in katakana, 'kun'
  and 'nanori' readings in hiragana). This doesn't always work too well because
  of the font size.


Other Changes

- Made some cosmetic changes to some windows and menus.

- Updated the doucmentation and help file.


                    = Jamie Packer (5/12/97) =
                    =  jamie@bristol.st.com  =
                = http://www.gingko.demon.co.uk/ =
                -_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_

