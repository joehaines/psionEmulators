CONTACT EDGE RELATION MANAGER
Version 4.6, 14/08/1997
Ad Bosch
Korenaar 61
3224 XD Hellevoetsluis (NL)
Tel: ++31 181 31 04 30
E-Mail: adbosch@worldaccess.nl
Homepage: http://www.worldaccess.nl/~adbosch/index.htm

¸1995 Version 2.7
Total Computer Systems
                              
--- HISTORY
--- INSTALLATION
--- CE RELATED PROGRAMS
--- OTHER PROGRAMS
--- CHAT (FAQ/TIPS/KNOWN PROBLEMS)

HISTORY:
New in version 4.6:
- autoreference suggestion in uppercase
- notes are now in WORD-format. Old TXT files will
  automatically be renamed to same with WRD extension. On
  first use of these you will be asked to "overwrite"
  existing files. Please do so. There is no need for
  further conversion.
- adapted Colleague module to take UDL into account
- some cosmetic changes
- added EDGE.GRP which may be used as Group icon, EDGENOTE.OPA
  which may be installed to show and edit the existing notes
  to a client, and CETEXT.ALS, same for templates.
N.B. the templates section is still under development.
However, complexity of the operation may force a major
rewrite of the whole section including some major
parts of other modules. Version 4.6 might therefor be
the final version of Contact Edge, as my major actual
objective will soon be the Series 5.

New in version 4.5:
- inclusion of a basical autoreference suggestion 
  with a new entry
- added default settings option for some fields
- added FAQ/TIPS to this file in order to justify
  WHAT'S NOT NEW
- at request removed obligatory uppercase for town/
  city and country
- cosmetic update of the CE Colleague module

New in version 4.4:
- simplified "about" window in order to save memory
- added shortnote option in history file
- updated of Colleague link

New in version 4.3:
- backup quits correctly when asked to
- logos now display on all pages
- new contact edge icon (looked too much like ...)
- larger postcode field to conform with US standards
- status: bug when nothing entered fixed
- templates: tiny logical update

New in version 4.2:
- at request bigger font in all-address-view
- at request relocated logo in statuswindow
- relocated JBSortB in order to avoid duplicate versions of
  JBSortB installed in different places on the same PSION

New in version 4.1:
- option to run Colleague (read-only) from CE (Colleague is a
  phone directory app specifically designed for company 
  internal dialling circuits)
- bug with enourmous phone numbers fixed
- fixed problem with standard templates; replaced templates by
  new ones
- view page 1 and 2 in one view
- dialling now in editable field
- view all details contact 1 and 2 pressing 1 or 2 (+ bugfix
  no phone at all)
- add help glossary to all menus (instead of main menu only)
- placement UDL.CED under \EDGE\ only fixed
- hard-coded UDF labels to 8 digits max.
- PIC-insertion for logo, etc. added
- logbook for date entered, date last update (incl. option
  in USER.CED to maintain it or not)
- moved USER.CED to M:\OPD\ and called it EDGE.INI
- included option read and delete letters

New in version 4.0:
- small cosmetic updates
- added run database
- added DBF help glossary option (user definable)
- main header label made user definable and depending 
  on status
- 4 contact field labels now user definable and 
  depending on status
- finalised context-sensitive helpfunction
- added option to remove existing notes
- notes get extension .TXT and not .WRD which was wrong
- removed the two .TXT manual files from the basic ZIP
  file (available at request)
  
New in version 3.2:
- improved style and layout
- restored quicksearch and added keyboard search within
  filters also
- added option key 1 and 2 for phone and dialling
  contact 1 and 2
- bug label/backgroung UDF fixed
- bug sorting with default disk other than M: fixed
- filter field reads title instead of filter number
- date to read today's date in date-defined UDF's when these
  have not yet been used
- company in uppercase, underlined, italic
- contacts in italic
- town and country in uppercase
- labels in inverse mode
- improved and context-sensitive help
- bug dailling repaired
- some more small bugs repaired
- tab key enters Client Search mode
- diamond key toggles Cients/Contacts mode
- adapted statuswindow to display diamond key
- enter key defined to display Company Name when in page 2, 3 or 4
- renamed "Sort key" to "Reference" in Index option
- renamed "Phone 2" in Enter/Update Client to "E-Mail"                                                                                                              

New in version 3.1:
- bug in View Appointments fixed
- bug in Filter submenu fixed
- renamed "subset" to "filter"
- some minor cosmetic changes

Version 3.0:
- major repair of bug-empested version 2.7
- new "keyboard" search facility added
- sorting with JBsortB added
- integrated a number of files into less but bigger ones
- added help option

INSTALLATION:
Place EDGE.OPA under \APP\ on any drive.
On the SAME disk create directly under the root 
(A:, B: or Internal, M:): \EDGESRC\
Under \EDGESRC\ create direcory \CEL\ and directory
\TEMPLATE\ to obtain:
ROOT:\EDGESRC\CEL\
and
ROOT:\EDGESRC\TEMPLATE\
Place all files ending on .WRD under the TEMPLATE
directory. 
All other files are to be placed under the CEL directory,
with the exception of:
EDGE.OPA (under \APP\)
JBSortB.IMG (under \APP\ or under \IMG\)
CLINK.OPA (optional data transfer file)
CLINK.TXT (info on how to use CLINK)
README.TXT (this file)

Install as usual. When running the program for the
first time it will create a file USER.CED under the
CEL directory which holds a.o. info on where the
program finds your Agenda and where you want to
keep the data files. You can change these options
later but remember that this file is stored with the
program and not with the data files (in case you
put the program on flash and/or write-protected
disk).

N.B. Contact Edge manual is available in .TXT format 
(updated until version 3.0).
If the help-files are insufficient for you, the manual
can be downloaded from my homepage.

NOTE version 4.2 relocation JBSortB!!!
The sorting routine JBSortB.IMG should be placed
in your \APP\ directory or else in a \IMG\ directory
under the root of ANY disk. If you put it under \IMG\
and you don't want its name to appear under the IMG
icon, you should rename it to "SYS$JBST.IMG".

UPGRADE from 4.0 to 4.1 upwards
Relocate USER.CED (under edgesrc\cel\) to
M:\OPD) and rename it EDGE.INI. Or let the
program handle it and enter your user details 
anew.


RELATED PROGRAMS:
- Colleague: company internal phone/extension database utility.
  Works both independently from CE as well as (in read-only mode)
  from within CE. SIENA compatible!
- Clink: helps you to convert your existing DATA contacts file to
  CE format (read CLINK.TXT first!)
- Creref: if you are dissatisfied with references the you, or Clink
  created client references in CE, you may rereference them with this
  utility (it creates references consisting of the first three
  chars of your client plus a numerical suffix; links to any
  existing CE history or Logo file will be lost, as will the links
  to Notes for specific clients) (read CREREF.TXT first!)
  
OTHER PROGRAMS:
- MyCar fuel consumption manager: to most informative costs monitoring
  program available for S3A/C (SHAREWARE)
- CDs: keeps track of your CD, CDI, CD-ROM, etc. collection (FREEWARE)
- Books: book collection manager (FREEWARE)
- Classics: CD collection manager for classical music (FREEWARE)

All available from my homepage (see above).

CHAT:
- Referencing:
  I expect users to carefully create client's references.
  As an exemple I use the first three characters in name
  and a three digit numerical (bos001). These references
  are used to interrelate between clients, contacts, history,
  etc. Therefor once created you will not be able to edit 
  references. Do not expect future versions of CE to include
  such a feature.
  If you must edit references you can open the various *.CED
  under Psion Data, JBData, etc. Make sure you change the right
  details exactly as you want them to be (a difference such as
  upper/lowercase may cause problems)!
- Remains of updated records: f you update a client you will
  notice that it is added as a last new record, the old number
  now being non existant (e.g. updating record 6 out of 10
  puts record 6 on position 11, and record number 5 is now
  followed by record 7). This situation is solved simply by
  closing CE and starting it again. Another option is to
  sort (re-index) the file by running index.
- The problem with printing templates remains: is there anybody
  out there able and willing to address this problem? If so
  please contact me!
- When you have to enter a series of clients sharing the same 
  fields (e.g. same company, address, country, etc.)
  set your user defaults to these shared fields.
  (I know it's not Copy/Bring/Paste, but it does help)
   
