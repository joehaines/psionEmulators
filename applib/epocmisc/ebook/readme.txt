EBook v2.3 for EPOC
-------------------
EBook is an electronic book (e-text/e-book) viewer. It supports Palm DOC (Aportis) documents, which are normally used on Palm Computing devices, TCR documents and regular text documents (i.e. ".txt" files, which most word processors can create).

Features:
* Supports compressed and uncompressed Palm DOC files (.prc & .pdb)
* Supports TCR documents
* Text document support  (.txt) files
* Supports Unicode documents (supplied as a different SIS package)
* Auto Scroll
* Scrollbar navigation
* Clipboard copy and copy to file
* Comprehensive bookmark facility
* 80 bookmarks per document
* Palm DOC & TCR recogniser on ER5 handhelds (Revo/Mako/MC218/S5mx/S7/netBook)
* Palm DOC style embedded bookmarks
* Palm DOC stored bookmarks
* Code page 850 support
* EPOC font file format (.gdr) support - allows the use of non-latin 
  fonts etc, such as Russian
* Online help
* Small memory usage, whatever size the document viewed
* Written in C++ for optimum speed


Please read the LICENSE.txt before using.


Installation
------------
To install the application please read the document INSTALLATION.TXT

Usage Instructions
------------------
Help is supplied with the application.

Registration
------------
EBook for EPOC is shareware. It costs £10 UK Pounds to register. Registration details can be found in the EBook Help file.


Creating Palm DOC files
-----------------------
I've ported the popular MAKEDOC utility to EPOC which is available on the homepage of EBook.

This EPOC port requires eshell.exe and STDLIB to run, these are available in the same location.

There are lots of similar utilities on various platforms to create Aportis files.


Issues
------
* EBook has been designed as a book viewer and the formatting engine was designed for top to bottom reading. Formatting of paragraphs can become a little untidy when you go backwards. 
* Opening EXEs, DLLs, etc may crash the program. 
* Document position info may be inaccurate with TCR files, due to the lack of original file size information in this format.
* Corrupt TCR files can crash the program when they are first opened.
* Vertical text justification is not available due to a bug in EPOC.


Changes in v2.3
---------------
New Features:
* 4x speed increase on Series7 and netBooks.
* Justified Text.
* Definable margin around text.
* Improved Bookmark dialogs for Series 5 and Series 7/netBook.
* Cosmetic improvements to UI on Series7/netBook.
* Hot key for skipping Guttenberg etext headers. Press Shift+Ctrl+F to skip the header.
* Last position read navigation.

Changes in v2.2
---------------
New Features:
* Unicode version now available supporting Japanese and Chinese. This is a separate SIS package, please go to the EBook homepage.
* Recent files menu, recalls last five documents opened
* Leaving "Books Folder" blank in preferences now causes last folder opened to be remembered.
* Added file modification date to Document Information dialog, removed Records info.

Unicode additional features:
* Supports and auto-detects ShiftJIS, JIS, EUC, UTF-8, EUCKR (CP949), BIG5 and GB2313 encoded documents

Fixes:
* File recogniser now always installs onto the C: drive, could cause crashes when installed on removable D: drive
* Crash when reading more PalmDOC bookmarks than can be stored

Changes in v2.1
---------------
New Features:
* rename bookmarks
* bookmark navigation hotkeys changed to use single key presses
* changed minimum autoscroll speed to .5 secs

Fixes:
* restored missing spacing between paragraphs with "Group by paragraphs" formatting
* problem with top line being blank on page down with "Obey Original" formatting

Changes in v2.0
---------------
New Features: 
* bookmark navigation enhancements
* Palm DOC bookmarks reading
* scrollbar
* clipboard copy and copy to file
* autoscroll
* "code page 850" support
* EPOC font file format (.gdr) support - allows the use of non-latin fonts etc, such as Russian
* view preferences tidied up

Fixes: 
* improvement to find positioning, now displays from the beginning of a line when it finds a word
* same as above for the goto percentage and scrollbar
* document is always displayed from top of screen, used to have problems with pageup to top
* did not rescan bookmarks if an invalid bookmark file existed "Removed File Error" when opening EBook and previous document is not there
* disabled "Find Next" if no search string has been entered
* occassional line down crash/jump problem with TCR documents


Bugs
----
Send any reports to ebooksupp@bigfoot.com

http://www.geocities.com/ebookepoc/

Cheers
Simon Quinn, UK
