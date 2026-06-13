

                 Unzip v1.4

 About Unzip
 -----------
 Unzip is a freeware user-friendly shell for
 the UNZIP.IMG image file from David Palmer.
 I wrote this application  as I could  never
 remember the command line options I  needed
 to unzip files. As  David had kindly  added
 full OPL launch support from version 1.4 of
 his UNZIP.IMG,  I  was inspired  to  create
 this very  useful shell  for David's  unzip
 utility.

 Unzip can be used as a replacement for,  or
 a supplement to David Palmer's RUN.APP.

 Licence Agreement - IMPORTANT
 -----------------------------
 Before using Unzip, you must read and agree
 with the  terms  of the  accompanying  file
 LICENCE.TXT.

 Features
 --------
 * Easy installation

 * Very easy to use dialog with all  options
 available at a glance

 * Files in \ZIP*.ZIP  are listed below  the
 Unzip icon

 * Supports  launch from  the 3c  Filer  and
 JBLaunch

 * Shows lists of files in the archive

 *  Supports  unzip  of  any  single   file,
 multiple files, or all files of any type

 *   Supports   selection   of   destination
 directory

 * Supports archive test and view of archive
 comment

 * Automatically locates  UNZIP.IMG file  in
 common directories on any drive

 * Last accessed 'unzip from' and 'unzip to'
 directories are memorised between use

 * Optional online help

 * Very small program size (<3K).

 Archive Contents
 ----------------
 When you  unpacked the  archive you  should
 have found these files...

  LICENCE.TXT        User licence agreement
  README.TXT         This documentation file
  UNZIP.OPA          Main application file
  UNZIP.HLP          Help file (optional)

 Also Required
 -------------
 You need to have either v1.50s or v1.50l of
 the  UNZIP.IMG  file   from  David   Palmer
 installed on your  Psion. UNZIP.IMG can  be
 either in the Unzip directory, in \RUN\, or
 in \IMG\. UNZIP.IMG is _not_ supplied  with
 this archive.

 Installation
 ------------
 Unzip will  install and  run on  the  Psion
 series 3a and 3c. The dialog is too big for
 the Sienna.

 1. Create a  new directory on  any disk  to
 hold the  Unzip  files  -  I  recommend  an
 \APP\UNZIP\ directory.

 2. Copy 'UNZIP.OPA' to the Unzip directory.

 3. If you want to install online help, copy
 'UNZIP.HLP' to the Unzip directory.

 4. Install  Unzip  from the  System  screen
 with Psion-I or select Apps | Install  from
 the menu (for English Psions)

 5. If you do  not already have  'UNZIP.IMG'
 installed, then you  should install it  now
 in either the Unzip directory, in \RUN\, or
 in \IMG\.  Unzip will  only work  with  the
 latest  version  (v1.50,  either  S  or   L
 variant). of  UNZIP.IMG, which  is part  of
 the  complete  unzip  package  from   David
 Palmer, available as...

      Archive         UNZIP.IMG Version
     S3UZ150L.ZIP       v1.50l (preferred)
     S3UZ150S.ZIP       v1.50s
     S3UNZIP.ZIP        v1.50l & v1.50s

      File             Version
     UNZIP.IMG          v1.50l

     ...in  the CompuServe  Palmtop  forum's
 library 18.  You  can  also  get  UNZIP.IMG
 v1.50 by anonymous ftp from...

 ftp://sunsite.doc.ic.ac.uk/computing/system
 s/handhelds/psion/icdoc/utils/s3uzip15s.zip

 ...or...

 s3uzip15l.zip

 For   a   detailed   explanation   of   the
 difference between v1.50l  and v1.50s,  see
 the section "Variants  of UNZIP.IMG"  later
 on in this file.

 Using Unzip
 -----------
 Unzip  contains   a   simple   dialog   for
 selecting  the  usual  unzip  options.  All
 options provided  by  UNZIP.IMG  v1.50  are
 supported by the  Unzip shell.  If you  use
 UNZIP.IMG  v1.50s  then  the  Unzip  dialog
 title will  show  "(for PKZIP  v2.x  only)"
 indicating that only archives created  with
 PKZIP version 2.0 and later can be unzipped
 (see "Variants  of UNZIP.IMG"  later on  in
 this file).

 Pressing [Esc]  from the  main dialog  will
 exit  Unzip   and   save  the   last   used
 directories to an \OPD\UNZIP.INI file.

 Unzip from file
 ***************
 Unzip displays  files  initially in  the  \
 (root)  directory  on  your  default  disk.
 Press [Tab] to bring  up the file  selector
 to select any file  you wish. You can  also
 access  remote  drives  on  your  PC  using
 either PsiWin  or RCom  - see  the  section
 "Accessing Remote Drives with Unzip"  later
 on in this file. When Unzip exits, it saves
 the last  used directory  in the  UNZIP.INI
 file.

 Unzip to directory
 ******************
 Unzip will  initially unzip  to \  on  your
 default disk.  Press  [Tab]  for  the  file
 selector to select any other directory. You
 can also unzip to remote drives on your  PC
 using either  PsiWin  or  RCom  -  see  the
 section  "Accessing   Remote  Drives   with
 Unzip" later on  in this  file. When  Unzip
 exits, it saves the last used directory  in
 the UNZIP.INI file.

 Action
 ******
 Extract files (default)
 This will extract all  the zipped files  in
 the archive.

 Update existing & create new
 This will unzip  files only  if the  zipped
 files have a timestamp which is newer  than
 the   existing  files  in  the  destination
 directory     (existing   files   will   be
 overwritten), and it  will also create  any
 new files that do not exist. This is a good
 choice if  you  want to  unzip  an  archive
 directly to  a flash  SSD. Unchanged  files
 will  *not*  be  overwritten,  thus  saving
 precious flash SSD space.

 Freshen (update existing only)
 This does  the same  as above  except  that
 *no* new   files are created.  This is  the
 best  choice  if  you  wish  to  update  an
 existing program but you  do *not* want  to
 unzip files that you do not have  installed
 (eg: text files and optional files). A good
 choice  when  unzipping  a  newer   program
 version directly to a flash SSD.
 NOTE: UNZIP.IMG  complains  if  you  choose
 this action  together  with  the  overwrite
 option "Never".

 Test archive integrity
 Test to see if the archive's files are  not
 corrupted and if they can be unzipped OK by
 UNZIP.IMG .

 Display archive comment
 Does just that.
 NOTE: many archives do not use the  comment
 feature.

 List files (short)
 Displays  all  the  files  in  the  archive
 showing just size, date, time and name.

 List files (long)
 This does the  same as the  short list  but
 displays a bit more information about  each
 file.

 Create file list (short)
 Creates a "\WRD\Filelist.wrd" file on  your
 default  disk  containing  the  short  file
 list. The  file  list is  overwritten  each
 time. This is very useful to see what files
 are in the archive, and as the file list is
 viewed in  Word,  gives  a  better  display
 (especially when the archive contains  many
 files) then the  "List files"   above.  You
 can also print the file list from Word.

 Create file list (long)
 The same as above but with more information
 about each file.

 File types
 **********
 Press [Enter] to  bring up  a small  dialog
 where you  can  select  All  (*.*),  Readme
 (readme.*), Text  (*.txt) and  Custom  file
 types. Enter a  Custom type  on the  second
 line of  the dialog;  you  may use  a  full
 filename if you  wish to  extract just  one
 file.

 Overwrite
 *********
 NOTE: overwrite options do not apply to the
 "Create file list" action.

 Prompt
 Unzip will ask you what  to do if it  finds
 an  existing file when extracting.

 Never
 Unzip will *never*  overwrite any  existing
 files.
 NOTE: UNZIP.IMG  complains  if  you  choose
 overwrite together  with  "Action:  Freshen
 (update existing only)"

 Always
 Unzip  will  *always*  overwrite   existing
 files.

 Options
 *******
 None
 No options.

 Discard paths
 If the  archive  contains  paths,  normally
 they  are   extracted  and   created   when
 unzipping.  Choosing   this   option   will
 *prevent* the paths from being created, and
 all files  will  be unzipped  to  the  same
 directory.

 Quiet operation
 Prevents some text from being displayed  on
 the screen or saved to the file list.

 Very quiet operation
 Even quieter than quiet operation.

 Variants of UNZIP.IMG
 ---------------------
 UNZIP.IMG v1.50 comes in two variants:

 v1.50l   (large   variant,   42160   bytes,
 *preferred*) Can  extract files  compressed
 with  both  PKZIP  v1.x  and  PKZIP   v2.x.
 Install this variant if you have the  space
 available (highly recommended)

 v1.50s (small variant, 32816 bytes)
 Can  only  extract  files  compressed  with
 PKZIP v2.x.  Attempting  to  extract  files
 compressed with  PKZIP  v1.x  will  produce
 "unknown compression method" errors.

 Unzip  knows   if   you  have   v1.50s   by
 displaying the text "(for PKZIP v2.x only)"
 in the dialog title.  Install this  variant
 if you are short on memory space and do not
 need to unzip PKZIP 1.x archives.

 Solving Problems
 ----------------
 Error messages that show "Unzip" on the top
 of the screen and  the Unzip icon are  from
 the Unzip shell. Common error messages are:

 "Unzip.img error    File  does  not  exist"
 means that  the  file UNZIP.IMG  cannot  be
 found on  your Psion.  Reinstall  UNZIP.IMG
 v1.50   according   to   the   installation
 instructions above.

 "Unzip.img error  Wrong version" means that
 your UNZIP.IMG file is not version v1.50.

 "Error in UNZIP\A  Invalid name" or  "Error
 in UNZIP\A   End  of file"  means that  the
 UNZIP.INI   file    is   corrupt.    Delete
 \OPD\UNZIP.INI and  the  INI file  will  be
 recreated automatically the next time Unzip
 is run.

 "Error in UNZIP\A   File or device in  use"
 means that the UNZIP.INI file is  currently
 open by another app. Maybe you are  viewing
 it in Word?

 Error messages  that occur  when "UnZip  v5
 for EPOC (version 1.50x)" is displayed  are
 from  the  UNZIP.IMG  file.  Common   error
 messages are:

 "can't allocate the work area" There is not
 enough free memory  for the UNZIP.IMG  file
 to run. Close other applications and delete
 unnecessary files  to  free  up  some  disk
 space.  UNZIP.IMG  needs  about  100k  free
 memory to run.

 "Cannot create [file]: write error or (disk
 full?)" UNZIP.IMG could not unzip any  more
 files as the  disk is  most probably  full.
 Close   other   applications   and   delete
 unnecessary files  to  free  up  some  disk
 space.

 "End-of-central-directory   signature   not
 found"  The file you are trying to unzip is
 not a PKZIP compatible archive. Choose  the
 correct file.

 "error:   invalid response  [x]"  UNZIP.IMG
 was asking you to  press a certain key  and
 you  pressed  key  [x]  instead.  Press   a
 correct key.

 "unknown compression  method"  The  archive
 was probably  created with  PKZIP v1.x  and
 you are  using  UNZIP.IMG  v1.50s.  Install
 v1.50 variant l.

 Illegal Combinations
 --------------------
 UNZIP.IMG will complain if you select  both
 "Action: Freshen  (update  existing  only)"
 AND "Overwrite: Never"

 Accessing Remote Drives with Unzip
 ----------------------------------
 Accessing remote  drives is  possible  with
 either PsiWin or RCom. MCLink has not  been
 tested. Remote drives will always appear in
 the file selector prefixed with "REM::"
 NOTE: The  default setting  of PsiWin  will
 not allow remote  access of  PC files  from
 the Psion. You can  change this by  editing
 the  PSIONPRC.INI  file   in  the   Windows
 directory, and removing  the '-x' from  the
 end of the line "engine=prcenwin.exe -x"

 Acknowledgements
 ----------------
 Full and  grateful acknowledgements  go  to
 David Palmer  of Edinburgh,  Scotland,  for
 his wonderful UNZIP.IMG  and his  excellent
 documentation.

 Change history
 --------------
 v1.4 03 Dec 96
 *  Added   [Control]+[Help]   support   for
 Help:Index
 * \ZIP\*.ZIP  files  now appear  under  the
 Unzip  icon.   You  can   start  Unzip   by
 selecting a file.
 *  UNZIP.IMG  file  can   now  be  in   the
 UNZIP.OPA directory, in  \RUN\ or in  \IMG\
 on any disk.
 * Unzip files can now be spread across  all
 disks
 * Unzip now shows  correct program name  in
 the dialog when launched by JBLaunch or the
 3c Filer.
 NOTE:   when   launching   from   JBLaunch,
 UNZIP.OPA and UNZIP.IMG are not allowed  to
 have the same filename. You must rename the
 UNZIP.OPA  file;   i.e.:   FUNZIP.OPA   and
 UNZIP.IMG will work OK
 * Changed file  type to  a separate  dialog
 and added "Custom" file type. You can enter
 your  own  wildcards  or  even   individual
 filenames to  extract.  The names  are  NOT
 case sensitive
 * Ini file is now \OPD\Unzip.ini

 v1.3 09-Aug-96
 * Bug fix: File Types = Word now  correctly
 selects *.wrd files.

 v1.2 21-Jun-96
 * Created series 3 classic variant (v1.2c).
 * Reduced  runtime memory  requirements  by
 47%.
 * File  selector  now shows  all  files  (*
 instead   of *.zip).
 * Removed support  for earlier versions  of
 UNZIP.IMG to force the  user to upgrade  to
 v1.50 (s or l) and to reduce code size.
 * Added better  error handling when  trying
 to   find or detect the UNZIP.IMG file.
 * Changed the icon.
 * Unzip now remembers last directories used
 in UNZIP.INI.
 * Added (optional) online help.
 * Added  ability to  extract certain  "File
 types".
 * Added "Create file list" action.
 * Rearranged the dialog layout and  changed
 some  dialog text.

 v1.1 07-May-96
 * Added support for UNZIP.IMG v1.0/1.2/1.3.
 * Renamed a couple  of options and fixed  a
 spelling mistake.
 * Optimised code to make the .OPA as  small
 as  possible.

 v1.0 01-May-96
 *  Original  version  (only  for  UNZIP.IMG
 v1.41).

 --
 Jochen Siegenthaler
 100127.3167@compuserve.com
 Zurich, Switzerland
 03 Dec 1996