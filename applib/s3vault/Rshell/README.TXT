
                   File Manager for Psion computers
                     Version 1.00 March, 29, 1997

                Copyright (c) Konstantin Saliy, 1996-97

                                   

                             Introduction
                             ------------
Brief history
-------------
I started to write this program in the May, 1996. The primary intention
was to make  a file manager  with support of  Ranger Disk, 3.5"  floppy
drive with  serial  interface. Its  name  -- RShell  --  means  exactly
"Ranger Shell". It was almost ready,  but then I didn't find  potential
users, and launching of Cyclone drive in June killed this idea.
I almost dropped  this project, but  then was told  by some people  who
downloaded this program from my Web site that they like some ideas  and
found RShell useful as a file manager. To continue improvement  RShell,
I had to disable RD support at all.
Now it became powerful  file manager with  some unique features.  Maybe
one day I'll restore RD support, because all in all I have that device.
Due to lack  of spare time  I was writing  RShell too slow.  As a  side
effect its name became well known, so I decided not to rename RShell.
I didn't implement all the ideas from my to-do list in the first public
release. I'll continue development and hope  you will like it and  wait
for new versions.

Copyright
---------
RShell is protected by Russian copyright laws and international  treaty
provisions.
Full  ownership  rights  to  RShell  are  the  exclusive  property   of
Konstantin Saliy, RShell author.

Conditions of use
-----------------
NO WARRANTY AT ALL. Although it was tested during more than 6 month,  I
take no responsibility on any possible damages or data loss. The entire
risk as to the quality and performance of the program is with you.
RShell is shareware program. You may evaluate it during 30 days, but if
you'll continue use it after 30 days period, you must register it.
You may copy and distribute  copies of RShell in  the same form as  you
receive it, without any files being removed, altered or added. You  may
not copy it in any other form. See Installation chapter for the list of
files in the RShell package.
Unregistered version  has only  one restriction:  some commands  (copy,
move, rename, change file attributes) work only on one panel (right for
vertical orientation). You always able to delete files and format disks
on both panels. Registered users will receive a code which will  unlock
disabled functions.
Please, read  the section  "How to  register" below  if you  like  this
program and want to register it.

                            Basic features
                            --------------
File Manager' screen has a header and 2 windows (panels) each of  which
shows you file list of any  local (A,B,C,M,ROM) drive, remote disks  on
the IBM PC, connected with 3Link. You can walk through directory  tree,
make directories, copy,  rename, delete files  and directories,  format
disks, run applications. You may search for files using many criteria.
Any file or directory on local disks (excluding ROM) may have a comment
up to  255 characters  long.  Copy and  move operations  will  preserve
comments, delete command deletes comments as well.
It requires about 40Kb  free RAM with default  settings and depends  on
your preferences (maximum number of files and cache size).
It was written using  assembler and operating  system calls instead  of
OPL operators which made it fast.
It is compatible and was tested with Psion Series 3a, 3c and Siena. I'd
recommend to use horizontal panel orientation  on Siena if you need  to
see file attributes.
It is compatible in  most cases with Cyclone  drive. The only  reported
problem (and I  don't know  how to  make it  work) is  to format  DS/DD
floppy (720K) which was unformatted or totally erased before. When  you
copy or move files and there is no enough free space, RShell gives  you
opportunity  to  insert  new  disk  and  continue,  and  will  preserve
directory structure. It makes more easy e.g. to backup large Flash  SSD
to floppies.

Warning about ROM drive on Siena
--------------------------------
All versions of Siena I saw have more than one SYS$CTRY.CFO file in the
ROM, i.e. more than one (I saw up to 4) file with the same name. If you
will e.g. copy either of  them, only first file  will be copied, and  I
couldn't prevent it.

Warning for non-English users
-----------------------------
RShell has an ability to search for files which contain a string. Then,
when you launch built-in text editor (Word, Program editor or any other
alias of  Word  application) for  a  found file,  RShell  sends  "Find"
command to Word with this search  string. On most language versions  of
Psion RShell sends this  command directly. But  on some versions  (e.g.
Sweden or Dutch  S3a) RShell  sends key  sequence to  navigate to  this
command using menu. If you'll  start pressing keys just after  editor's
window appeared, you may make changes to the file.
The logic to determine whether to  use a command or menu access  method
does not depend  on particular language  code, so I  couldn't tell  you
which way RShell will work on your computer.

                             Installation
                             ------------
RShell comes as a ZIP archive with 3 files:
     RSHELL.OPA - application file.
     RSHELL.HLP - online help file.
     README.TXT - this file.

Copy RSHELL.OPA  into \APP\  directory and  install its  icon from  the
System screen usual way.
If  you  need   online  help,  copy   RSHELL.HLP  file  supplied   into
\APP\RSHELL\ directory on the same drive.

                             Menu commands
                             -------------
File
----
Copy              (Psion-C) copy  file  or directory  under  cursor  or
                  tagged files. You can rename files while copy.
                  (Psion-Shift-C) asks for  the destination  directory.
                  If it does not exists, it will be created.
Rename            (Psion-N) rename file  or directory  under cursor  or
                  tagged files.
Move              (Psion-M) move  file  or directory  under  cursor  or
                  tagged  files  into  another  directory.  Move  files
                  across disks is exactly copy and then delete.
                  (Psion-Shift-M) asks for  the destination  directory.
                  If it does not exists, it will be created.
Info              (Psion-I) display information about  a file. You  may
                  add a comment (up to 255 characters long) to any file
                  or directory.
View              (Psion-V) view  contents of  the file  under  cursor.
                  Implemented only for  Psion bitmap files,  regardless
                  of  extension.  For  directory  this  command   shows
                  statistics.
Edit              (Psion-E) runs text editor (not Word processor) for a
                  file under cursor.
Attributes        (Psion-Shift-A)  change   attributes  for   a   file,
                  selected files, directory itself or all files in  the
                  directory. To change  attributes for specified  files
                  in a directory tree, place cursor to the "Parent Dir"
                  entry.
Date/time stamp   (Psion-Shift-S) change  date/time stamp  for a  file,
                  selected files, directory itself or all files in  the
                  directory. To change stamp  for specified files in  a
                  directory tree,  place  cursor to  the  "Parent  Dir"
                  entry.

Directory
---------
Make              (Psion-Q) create new directory. On any local disk you
                  can create directory tree with a single command (e.g.
                  TMP\MYFILES).
Compare           (Psion-O)  compare  two  directories.  Then  in  each
                  directory some  files  may be  tagged:  either  newer
                  versions of existing files or new files.
Copy              (Psion-C) copy  file  or directory  under  cursor  or
                  tagged files.
                  (Psion-Shift-C) asks for  the destination  directory.
                  If it does not exists, it will be created.
Rename            (Psion-N) rename file  or directory  under cursor  or
                  tagged files.
Move              (Psion-M) move  file  or directory  under  cursor  or
                  tagged  files  into  another  directory.  Move  files
                  across disks is exactly copy and then delete.
Info              (Psion-I) display information about a directory.  You
                  may add a comment (up to 255 characters long) to  any
                  file or directory.

Disk
----
Select disk       (Psion-D) selects a disk for the active panel.
                  (Psion-Shift-D)  selects  a  disk  for  the  inactive
                  panel.
Re-read           (Psion-R) use this command  when disk was changed  or
                  if file system was changed by another application.
Find file         (Psion-F) finds files by some criteria.  "Hexadecimal
                  string" means a string of pairs of hexadecimal digits
                  without separator (e.g. "0D0A"  represents CR and  LF
                  codes)  To  set   advanced  search  options,   select
                  "Advanced options" string and  press Enter. Then  you
                  may press  Esc  if you  don't  want to  use  advanced
                  search.
Name disk         (Psion-Shift-N) change volume  label for the  current
                  disk.
Format            (Psion-Shift-F) format  disk. As  usually, all  files
                  will be destroyed during format operation.
Info              (Psion-Shift-I)  display  some  information  about  a
                  disk. For Flash SSD, it calculates free space,  which
                  will be available if you backup files, format SSD and
                  then restore  files. It  does not  count disk  space,
                  recoverable by  compressing  Data and  Agenda  files.
                  Note that sometimes it can  be less than the  current
                  free space.  For RAM  disks allocation  unit size  is
                  shown.

Special
-------
Edit New          (Psion-Shift-E) creates new text file.
Swap panels       (Psion-W) exchange panel places.
Set preferences   (Psion-P) runs preference menu (see below).
Zoom in           (Psion-Z) increases font size.
Zoom              (Psion-Shift-Z) decreases font size.
Remote link       (Psion-L) switches  remote  link  on  and  off.  With
                  remote  link  switched  on,   remote  disks  can   be
                  accessed. Only PC remote file system is supported.
                  Note: you  can switch  remote link  on and  off  from
                  System screen as well.  This is only replacement  for
                  easy access.
About             (Psion-A) displays copyright  information, my  e-mail
                  address.
Exit              (Psion-X) exits  File  Manager.  Use  it  instead  of
                  Psion-Esc, if possible.

Preferences
-----------
General           select sort criteria, confirmations, maximum  numbers
                  of files to be shown in a directory etc.
Display           various options to display data in panels.
Associations      you can associate up to  4 applications with a  given
                  pattern. Association for "*.*" is used only if  there
                  is no other record for  that file. Special value  '*'
                  for application  means that  the  file itself  is  an
                  application and should be run.
                  You may add a  comment to any file  type. It will  be
                  shown as default  comment for that  file type in  the
                  File Info (Psion-I) dialog.
Search path       file manager  looks for  application  to run  on  the
                  disks A, B, M, C  (in this order) in the  directories
                  in the search path. Default value is "\APP\". Use ';'
                  as delimiter. Do not  forget to specify trailing  '\'
                  in the directory name.
Hot key           set  a  hot  key  to   bring  the  file  manager   to
                  foreground. Any  letter  with any  modifiers  can  be
                  chosen, but this key combination will be disabled  to
                  other applications.  E.g. if  Psion-A is  set as  hot
                  key, no application will recognize it as a menu short
                  cut.
Save options      Saves preferences.

Found File
----------
Almost all menu items are the same as above.
Jump to           (Psion-J) returns to File Manager and makes the  file
                  under cursor the current.
Save list         (Psion-S) saves list of files found into a text file.
Set preferences   (Psion-P) only one option  is available now:  whether
                  to send "Find" command to  a editor that was  started
                  with found file.
Exit Find         (Psion-X) returns to File Manager.

                              Other keys
                              ----------
Delete            deletes file or  directory under  cursor or  selected
                  files.
Shift-arrows      switches tag  mark  of  the current  file  and  moves
                  cursor.
Space             switches tag mark of the current file.
Tab/Diamond       switches between panels.
Enter             goes inside subdirectory under cursor or runs a file.
'+', '-'          tags/untags files using pattern.
'*'               inverts selection.
\                 Jump to the root directory.
Control-Up        Jump to the parent directory.
Help              Online help.
Control-Help      Help Index.
Left, Right       scrolls file attributes window.

To fast scroll to a  file you need you can  start to type its name.  If
you don't remember a letter in the  file name, type '?' to replace  it.
To remove text cursor,  press Esc. To jump  to the next matching  name,
press Control-arrow down.

                             Useful notes
                             ------------
To interrupt copying or deleting file or group of files press Esc while
copy/delete in  progress. You  may have  to press  Esc quickly  several
times if copying of a directory tree is in progress.
Copy, Move, Delete, Info commands can be used with special "Parent Dir"
entry.

Pattern may  contain wildcards  ('*'  or '?')  or  may not.  To  rename
selected files you must use wildcards in the pattern so that  resulting
file names will be unique.

Associations
------------
You can define  up to 4  associations with  a pattern to  use when  you
press Enter, Control-Enter,  Shift-Enter or  Psion-Enter. File  manager
searches for first fit pattern  and use its associations regardless  of
whether they are  defined or  not. The  only exception  is *.*  pattern
which is applied last.
The symbol '*' has a  special meaning when it  is entered as the  first
character in any  field for  associated applications. It  means that  a
file itself is an  application and should be  run. This is the  default
value e.g. for *.APP and *.IMG patterns.
Some .IMG files (e.g. UNZIP.IMG) requires special for of command  line,
as for DOS and  Unix programs. You may  specify these parameters  after
the '*'  symbol, using  at  least 1  space  as separator.  See  default
settings for *.ZIP as an example.
As an application, you may specify its name with or without  extension,
with full or partial path. If there is no extension, RShell tries  APP,
OPA, ALS, IMG, OPO in this order.  If there is no drive, it tries  ROM,
A, B, M, C in this order. If there is no full (from the root) directory
name, it searches in the  directories from Set preferences/Search  path
option.
For instance, if you want to run HexView (my hexadecimal viewer/editor)
for  any  file  by   pressing  Psion-Enter,  and   its  full  path   is
LOC::M:\APP\HEXVIEW.OPA, you may type either from
     HEXVIEW
     HEXVIEW.OPA
     \APP\HEXVIEW
     LOC::M:\APP\HEXVIEW.OPA
and many other combinations on  the appropriate line for pattern  "*.*"
and all other patterns.
If  your   application   is   in   a   non-standard   directory   (e.g.
\APP\READCIS\JBCIS.APP -- CompuServe reader) you may enter
     READCIS\JBCIS
for a corresponding pattern (e.g. MAIL*.OUT for outgoing mails).
If you have typed a comment for  a pattern, it will be used as  default
for all  files that  satisfy this  pattern and  have no  comments  (see
Comments below).
UNZIP.IMG is special case of associated application -- RShell  produces
a dialog box before  run it, where you  may specify other command  line
parameters and a target directory.

What does Directory Info show?
------------------------------
Every file occupies more space on a disk then its exact size, and  disk
space required to keep a file depends on disk type, file size and other
factors.
On any disk type,  some space is required  to keep information about  a
file -- its name, size, attributes etc.
On the Internal disk and RAM SSD disk space is allocated by fixed  size
units, so called "clusters". You may see cluster size in the  Disk/Info
dialog. With cluster  size 512 bytes,  a 513 bytes  long file  occupies
1024 bytes, i.e. 2 clusters.
On the Flash SSD disk space  is allocated by variable length  portions,
less  than  64Kb  each.  Every  portion  has  a  header  which  require
additional space.
Directory/Info dialog  shows you  how much  space this  directory  tree
require to be copied to different  types of disks, assuming that  there
is no directories with the same  name and no destination files will  be
overwritten.
On the Internal disk or RAM SSD  required space is less if one or  more
files will  be overwritten.  On  any disk  required  space is  less  if
directory or one or more subdirectories already exist.

Comments
--------
Using File/Info or Directory/Info menu  commands you may add a  comment
up to 255 characters long to any file or directory on any local (M,  A,
B and  C) drive.  At the  time of  writing, disk  C is  accessible  for
writing (and thus for comments) only if it is Cyclone floppy drive.
In the Set preferences/Display menu dialog turn 'Show file comments' to
'Yes' to  see your  comments while  browsing disks.  With this  option,
RShell will show comments in the appropriate corner if you  highlighted
an entry and didn't press any key more than a second.
Turn 'Sync file info'  to 'Yes' (menu  Set preferences/General) if  you
want RShell  to  keep  comments  with  files  during  copying,  moving,
renaming, and delete them with files and directories. This option slows
down such operations.
RShell does  not copy  file  comments to  remote disks.  However,  full
backup  and   then  restore   will  copy   comments  data   base   file
(~dirinfo.~~~), one  per  disk,  and  thus keep  them.  Copy  and  Move
commands preserve  comment for  target file  if a  source file  has  no
comment.
RShell has predefined  set of  comments for some  directories and  file
types, and show them if you have no your own comments for that entry.

Hot key
-------
You may  choose  virtually any  combination  of  a key  from  the  main
keyboard and  Control, Shift,  Psion modifiers  in any  combination  to
bring File  Manager to  the  foreground. On  Siena, 'Fn'  modifier  and
characters that required  it are not  supported yet. This  can be  done
from the Set preferences/Hot key menu:
   1. Press a key or type a character in the 'Character' field.
   2. Turn 'Psion', 'Control' and 'Shift'  fields to 'Yes' if you  need
     to press them for the hot key chosen.
   3. Turn 'Psion  required' to  'Yes'  if that  character can  not  be
     entered without pressing Psion key (e.g. '@').
   4. Turn 'Enable' to 'Yes' and press Enter.
Note: if you have running more  than one instances of RShell, only  the
first started senses the hot key. RShell  does not sense hot key if  it
has menu bar or a dialog box active.

Configuration files
-------------------
File manager keeps  association information in  the RSHELL.EXT file  in
the \OPD\ directory, initially on the internal drive. It is plain Psion
Data file and you can edit it (e.g. to change order) with built-in Data
application or other data base tools, but it is not recommended.

File manager keeps  all settings in  the RSHELL.INI file  in the  \OPD\
directory,  initially  on  the  internal  drive.  Both  RSHELL.INI  and
RSHELL.EXT can be  moved to  \OPD\ directory  on any  drive using  file
manager itself. File manager looks for these files on drives A, B, M in
this order every time to read or to update.

File manager keeps comments to files in ~DIRINFO.~~~ files in the  root
directory on each drive. It is plain  Psion Data file and you can  edit
it (e.g. to change order) with built-in Data application or other  data
base tools, but it is not recommended

If you  have renamed  or deleted  files with  associated comments  from
another  file  manager  or   with  disabled  comment   synchronization,
corresponding comments become superfluous. But then when you'll  delete
any file or  directory on the  same disk from  this file manager  (even
with disabled comment synchronization!), these comments will be deleted
from ~DIRINFO.~~~ file.

                      Known bugs and limitations
                      --------------------------
Copy and Move commands  don't check for free  space on the  destination
drive. Please,  check it  manually using  Info commands  for disks  and
directories.

This application was not tested by  me with Cyclone 3.5" floppy  drive.
Please, let me know if you have any problems.

View directory  statistics  command  presume, that  Internal  disk  has
cluster size 256  bytes, RAM  SSD -- 512  bytes, Flash  SSD has  24-bit
references. In  some cases  this  is not  true  and actual  disk  space
requirement may be larger or smaller.

                      Want to see my To Do list?
                      --------------------------
This list is provided "as is" without any promises to implement it,  in
no order. Just to prevent multiple postings :-)
   1. Launch panel.
   2. File names only in multiple columns.
   3. Text viewer.
   4. File display filters.
   5. Sort order (asc/desc)
   6. Panel with all disks and volume labels.
   7. Sort of associations.
   
                            How to register
                            ---------------
RShell is a shareware  program. You may use  it for evaluation  purpose
during 30 days period. Then you  must either destroy it or register  if
you like it and want to continue to use it.
I (living  in Moscow,  Russia) couldn't  accept mails  with cheques  or
cash. I'm no longer CompuServe member and thus can't use SWREG service.
The best way to know how to register RShell is to e-mail me and ask.
Now you may register RShell via
     1. CompuServe SWREG, ID=14997, registration fee is $25.
     2. By sending  a cheque  to Michael Baas,  PSIOLogic, in  Germany.
Registration fee is 40 DM. Please send a cheque to the address below:
     PSIOlogic
     Taunusstrasse 4
     D 63589 Linsengericht,
     Germany
Please, make your cheque payable to Michael Baas.
Being Psion  distributor in  Germany, Michael  Baas is  able to  accept
credit cards. You may call him +49 06051 470065 or e-mail:
     Internet:<106044.2274@compuserve.com>
     CompuServe:106044,2274
Do not  forget to  notify  me by  e-mail  about your  registration  via
collectors. I'll send you your registration code which will unlock  all
the features disabled in the trial version then.

                            How to reach me
                            ---------------
By e-mail:
     Internet: <kis@aha.ru>
     CompuServe: 106062,203
You may also  visit my  Web site  <http://www.aha.ru/~kis/>. It  always
contains latest versions of RShell and my other programs.
All comments, suggestions and even bug reports are welcome.
Please, use the word "RShell" (with possible additions) in the  Subject
line of all mails about this program.
