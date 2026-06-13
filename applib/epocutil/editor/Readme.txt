Symbian Text Editor Application v1.35(040)
==========================================
Copyright (c) 1998-2000 Symbian Ltd. All rights reserved.

DISCLAIMER OF WARRANTY
======================
SYMBIAN PROVIDES NO WARRANTY, TO THE EXTENT PERMITTED BY APPLICABLE LAW. EXCEPT WHERE OTHERWISE STATED IN WRITING, SYMBIAN PROVIDES THIS SOFTWARE AND/OR DOCUMENTATION "AS IS" WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, STATUTORY OR OTHERWISE, INCLUDING, BUT NOT LIMITED TO, ANY IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THIS SOFTWARE AND/OR DOCUMENTATION IS LICENSED TO YOU WITHOUT FEE AND ACCORDINGLY YOU ACCEPT THAT THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE SOFTWARE AND/OR DOCUMENTATION IS WITH YOU AND YOU AGREE NOT TO TAKE ANY INCONSISTENT POSITION. SHOULD THE SOFTWARE AND/OR DOCUMENTATION PROVE DEFECTIVE, YOU ASSUME THE COST OF ALL NECESSARY SERVICING, REPAIR OR CORRECTION OF THE SOFTWARE AND/OR DOCUMENTATION AND OF ANY PRODUCT OR APPLICATION.

IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW WILL SYMBIAN BE LIABLE TO YOU FOR DAMAGES, (WHETHER ARISING IN CONTRACT, TORT, NEGLIGENCE OR OTHERWISE) INCLUDING ANY LOST PROFITS, LOST MONIES, LOST TIME, LOSSES ATTRIBUTABLE IN WHOLE OR PART TO ANY DEFECTS IN THE DESIGN OR PERFORMANCE OF THE SOFTWARE AND/OR DOCUMENTATION OR ANY PRODUCT OR APPLICATION, OR SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OR INABILITY TO USE THE SOFTWARE AND/OR DOCUMENTATION (INCLUDING BUT NOT LIMITED TO LOSS OF DATA OR DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY THIRD PARTIES OR A FAILURE OF THE SDK TO OPERATE WITH PROGRAMS NOT DISTRIBUTED BY SYMBIAN), EVEN IF SYMBIAN HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES, OR FOR ANY CLAIM BY ANY OTHER PARTY. NOTHING IN THIS AGREEMENT LIMITS SYMBIAN'S LIABILITY FOR DEATH OR PERSONAL INJURY CAUSED BY ITS NEGLIGENCE.

File Listing
============
The following files should have been included with this distribution:

Readme.txt	This text file
Editor.sis	MARM installation of the Editor application
EditorW.zip	WINS release of the Editor (zip file includes paths for easy extraction)

About This Software
===================
This application is a plain text editor EPOC. It originally started life as an internal Symbian tool. With the advent of ER5 and Java it became clear some 3rd party developers who were members of the Symbian Developer Network (www.SymbianDevNet.com) would find a text editor useful for editing their Java code under EPOC. The application was therefore tidied up and made available for release.

This software has been tested under both ER3 and ER5 and no major problems have been found. ER3 users should not that no file recognizer is supplied at the moment, so .TXT files will not get associated with Editor. Under ER5, Editor recognises text files by MIME so you *will* be able to double-tap them from System, for example, to edit them.

Installation Instructions
=========================
For MARM, simply copy the Editor.sis file to your EPOC device and double tap it. The installer technologies should take care of the rest.

Under WINS, extract the contents of EditorW.zip onto your SDK drive, ensuring you have the option enabled in your unzipper which will 'restore paths' (or similar).

Editor has been tested on the Psion Series 5mx, Psion Revo and ER5 WINS emulator. Whilst not tested on other devices, it should run without issue on any machine running ER1-ER5.

Known Issues/Omissions
======================
1. The Editor should save the name of the last used file in its .ini file and try to open that file when you launch it from Extras. This currently does not happen.
2. There is no Help file included with this release.

Future plans
============
1. Fix the above issues
2. Add a 'Go to line' option
3. Add an option to display the current line number at the side of the page

History
=======
- v1.20(027)	24 January 2000		Initial public release on the Technology Supplement CD
					for the US Developer Conference, February 2000.
- v1.35(040)	30 June 2000		Second public version with several minor bug fixes,
					released to the Symbian Developer Network website.