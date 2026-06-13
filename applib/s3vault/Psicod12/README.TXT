

                PsiCode v1.2
                ============
 

 About PsiCode
 -------------
 PsiCode is a UUencode/UUdecode utility  for
 the Psion 3a/3c.

 UUencode/decode  (UNIX-UNIX  encode/decode)
 is one  of  the  'industry  standards'  for
 attaching files to e-mails, and is found in
 all common e-mail programs.
 UUencode/decode, also  called  UUcode,  was
 developed  initially  for  UNIX   machines,
 where system administrators needed to  send
 files to each other  via e-mail. The  files
 are 'UUencoded' by the sender, sent via  e-
 mail,   and   then   'UUdecoded'   by   the
 recipient.

 PsiCode now brings  you the flexibility  of
 UUencode to  your  Psion.  You  can  easily
 encode and attach files  to an e-mail  file
 in one simple step.

 PsiCode also lets  you UUdecode and  detach
 files from  any  UUencoded e-mail  you  may
 receive (after  all,  what good  is  encode
 without decode?!), and removes the embedded
 UUencoded data after a Detach to help  save
 precious Psion  memory space.  PsiCode  can
 also automatically launch Unzip (or PsiZip)
 if the decoded file is a zip file.

 If you use  your Psion for  e-mail, then  a
 UUencode/decode utility is a must!

 Licence Agreement - IMPORTANT
 -----------------------------
 Before using  PsiCode,  you must  read  and
 agree with  the terms  of the  accompanying
 file LICENCE.TXT.

 Features
 --------
 * Very fast.  Main encode/decode  functions
 are written in C.

 * Compatible  with most  existing  UUencode
 programs. Tested with UNIX  UUencode/decode
 (Sun release 4.1), UUlite for MAC,  WinCode
 v2.7.2g, WinZip v6.2, Netscape v3.0.

 * Fully automatic  decode. UUdecodes  files
 from any source (Psion, MS-DOS, UNIX, MAC),
 and even  decodes  files encoded  with  the
 older Psion  UUcode  program.  Any  invalid
 Psion filenames (ie: from UNIX systems) are
 automatically renamed.

 * Launches  any  .APP or  .OPA  application
 when a zip file is found. Can be used  with
 Unzip or PsiZip to streamline the unzipping
 of files received by PsiCode.

 * Finds up to  50 embedded UUencoded  files
 and   allows   individual   files   to   be
 extracted.

 * Ignores all  UNIX directories and  always
 tries to recover the proper file name  when
 decoding.

 * User-selectable en- and decode filenames.
 You are prompted to enter a new filename or
 you can accept the proposed name.

 *  Attaches  files  to  ReadCIS  MAILxx.OUT
 files, automatically UUencoding them in the
 process. A great feature for sending  files
 with ReadCIS.

 * Detaches files  from ReadCIS *.CIS  files
 (ie: MAIL!.CIS), automatically removing the
 embedded UUencoded data  in the process.  A
 great  feature  for  receiving  files  with
 ReadCIS.

 * Can append any  two text files  together,
 regardless of file size.

 * Selectable maximum encode line length.

 *  Selectable  Psion/MS-DOS,  UNIX  or  MAC
 UUencode file types.  Useful when you  know
 what platform the recipient has.

 *  Selectable  removal  of  UUencoded  data
 after a  successful  Detach.  Ensures  that
 precious Psion memory space is not wasted.

 *  All  preferences   and  last   directory
 accessed are memorised between use.

 * Psion-compliant menus  and dialogs.  Uses
 standard hotkeys and  menu layout for  easy
 intuitive usage.

 * Context-sensitive online help  (optional)
 for all menus and dialogs.

 * Small program size (only ~8 k).

 Archive Contents
 ----------------
 When you  unpacked the  archive you  should
 have found these files...

  LICENCE.TXT        User licence agreement
  README.TXT         This documentation file
  PSICODE.OPA        Main application file
  PSICODE.DYL        Library file
  PSICODE.HLP        Help file (optional)

 Installation
 ------------
 PsiCode will install and  run on the  Psion
 series 3a and 3c.

 1. Create a  new directory on  any disk  to
 hold the  PsiCode files  - I  recommend  an
 \APP\PSICODE\ directory.

 2. Copy 'PSICODE.OPA' and 'PSICODE.DYL'  to
 the PsiCode directory.

 3. If you want to install online help, copy
 'PSICODE.HLP' to the PsiCode directory.

 4. Install PsiCode  from the System  screen
 with Psion-I or select "Apps | Install" (on
 English Psions) from the menu.

 Using PsiCode
 -------------
 File | Encode file
 ******************
 This will UUencode  any file  of any  size.
 You are prompted  to enter  a filename  for
 the UUencoded file.  The initial  directory
 displayed is "\" and the prompted  filename
 for  the   UUencoded  file   is  the   same
 disk/dir/filename as  the source  file  but
 with a .UUE extension.

 File | Decode file
 ******************
 This searches  through  the  chosen  source
 file  for   the  first   or  all   embedded
 UUencoded files  - up  to 50  files can  be
 found. A dialog  then appears allowing  you
 to select an individual file to decode.  If
 no embedded files are detected, the message
 "No  files  found"   appears.  If  a   file
 encountered  during  the   search  has   an
 invalid Psion filename  then the file  will
 be renamed to BADNAME.xxx, (where xxx is  a
 number   from   001   to   050,    assigned
 sequentially).
 The preference  "Search until"  can  select
 whether the source file should be  searched
 through until either "All files found" (all
 embedded  UUencoded  files  are   detected,
 search goes right to the end of the  source
 file), or "First file found" in which  case
 PsiCode will stop searching  as soon as  an
 embedded UUencoded file is detected.
 You can  also  open  a  received  zip  file
 automatically after  it has  been  decoded.
 First set the preference "Open zip file  if
 found" to "Yes"  and select an  application
 to "Open  zip file  with". At  the time  of
 writing,   there   are   only   two   known
 applications that can open  zip files on  a
 Psion:  Unzip,  and  PsiZip.  However,  the
 Psicode  sends   a  standard   command-line
 argument to the application file, and so it
 should work with any application.

 File | Append file
 ******************
 You can append  any two text  files of  any
 size  together.  The   Append  feature   is
 designed to  allow you  to append  existing
 UUencoded files to MAILxx.OUT files. If the
 file selected  is not  a text  file then  a
 "Wrong  file   type"   error   message   is
 displayed.

 Mail | Attach file
 ******************
 This can attach  any file to  a ReadCIS  e-
 mail file. The file to Attach is  UUencoded
 and appended  to  the  selected  MAILxx.OUT
 file.
 NOTE: You do NOT need to first UUencode and
 then Attach.
 CompuServe will allow up to 2MB per e-mail,
 so the  only  constraint with  attaching  a
 file is the available memory space on  your
 Psion.
 NOTE: You can only  attach to text files  -
 if you use Word to write your e-mails  then
 the resultant  MAILxx.OUT  file will  be  a
 Word file. If you try to attach a file to a
 Word file then PsiCode  will show an  error
 message. Use  ReadCIS  to change  the  Word
 file to Text and then try again.
 NOTE: The attachment will increase the size
 of the e-mail file  by 1.33 * <source  file
 size>. If  you  do  not  have  enough  free
 memory during the  attachment process  then
 the Attach  will fail,  however the  e-mail
 file will not be damaged.
 CAUTION: viewing  or editing  a  MAILxx.OUT
 file after  you have  attached a  UUencoded
 file to it *may*  cause the UUencoded  data
 to be word-wrapped and thus corrupted  (the
 format of the data is crucial to be able to
 UUdecode the  file). Try  not to  edit/view
 the  MAILxx.OUT   file  after   an   Attach
 process.

 Mail | Detach file
 ******************
 This is similar  to "Decode  file", but  is
 specifically designed to work with ReadCIS,
 and  allows  you   to  easily  select   the
 MAIL!.CIS file to Detach from.
 If you choose to "Remove data after Detach"
 then the embedded  UUencoded data from  the
 mail  file   will   be  deleted   after   a
 successful Detach. This reduces the size of
 the e-mail  file  dramatically,  and  saves
 precious Psion  memory  space.  A  note  is
 inserted into the e-mail file to remind you
 where the embedded file was detached to.
 The preference  "File  detection"  and  the
 ability to open a zip file directly applies
 exactly the same way as in Decode.

 Using With ReadCIS
 ------------------
 If  a  ReadCIS  WRAP80.ON  switch  is  set,
 CompuServe  will   format   mail   messages
 received  by  ReadCIS  to  a  maximum  line
 length of 80 characters, which is fine  for
 PsiCode. However, if no WRAP80.ON switch is
 set then  ReadCIS instructs  CompuServe  to
 format mail with a  maximum line length  of
 40  characters,   which   can   result   in
 corrupted UUencoded files due to CIS trying
 to wrap  lines  whenever  it  encounters  a
 space character.
 Selecting a UUencode maximum line length of
 less than  40 characters  helps to  prevent
 word-wrap from occurring when the e-mail is
 received by a ReadCIS user.
 Although  PsiCode  has  been   specifically
 designed to prevent word-wrap problems, you
 may receive  UUencoded files  generated  by
 other  UUencode  programs  (such  as  UNIX,
 WinCode or  WinZip).  To  ensure  that  all
 UUencoded  files  from   all  sources   are
 correctly received, you should  permanently
 set a WRAP80.ON switch.
 Setting a UUencode  maximum line length  of
 less than 40 characters will work with  all
 e-mail systems and all UUdecoders, and will
 guarantee that  CompuServe will  not  word-
 wrap the UUencoded  data. If you  encounter
 problems with file corruption, then set the
 UUencode maximum line length to 37.

 Limitations
 -----------
 * Maximum number of text lines that can  be
 read when  searching for  embedded files  =
 32,767. If the lines average 80  characters
 this results  in  a maximum  file  size  of
 32,767 * 80 = 2,621,360 bytes.  Considering
 Psion memory  size this  limitation  should
 not be a problem.

 * Maximum detectable files when decoding  =
 50.  This  limitation  is  forced  by   the
 dCHOICE maximum length  of 255  characters.
 The  amount  of   filenames  that  can   be
 detected in practice is somewhat less as it
 is dependent upon the  total length of  all
 of the embedded filenames.

 * Maximum CompuServe e-mail message size is
 2MB  (for   text   messages).   Considering
 typical file size  and also memory  storage
 constraints of the Psion, this should never
 be a problem.

 Change history
 --------------
 v1.2 06 Dec 1996
 * Added  ability  to remove  embedded  data
 after a Detach. This removes the data  from
 the e-mail  file  only after  the  embedded
 file has been successfully UUdecoded,  thus
 saving a lot of storage memory space on the
 Psion, especially when the e-mail file will
 be archived afterwards.
 * Added ability  to open  an app  if a  zip
 file is found. You  can select any .OPA  or
 .APP app to launch, the app must accept the
 'O'  (open  file)  command  line  argument.
 Needless to  say, it  works with  Unzip.opa
 v1.4 <g>.
 * Added  file  checking to  Append.  Append
 will only work on text files, and any  non-
 text file  will  now cause  a  "Wrong  file
 type" error.
 * Added context-sensitive help, and updated
 help  file.   The   correct   help   screen
 automatically  appears   when   you   press
 <Help>.  <Control   +  <Help>   now   works
 correctly.
 * Changed Decode  dialog so that  uuencoded
 files (*.UUE) automatically  appear in  the
 file selector. You can always use <Tab>  to
 select any other file.
 * Improved menu and dialog layouts
 * Changed code so  that it works  correctly
 with the 3c 'Files' app and also JBLaunch.
 * Fixed 'String too  long' and 'Record  too
 long' errors when  decoding UUE files  with
 long lines of text  inserted in the  middle
 of the UUE data.
 *  Reduced   code  size   considerably   by
 rewriting a lot of the code and using  some
 new tricks learnt in Palmtop Issue 9. Great
 little journal!!. Temporary  files are  now
 no  longer  used,  this  results  in  large
 memory savings.
 * Less  system memory  used as  DYL is  now
 only loaded  when  required,  and  unloaded
 afterwards.
 * Dramatically increased speed when seeking
 the  start  of  the  UUencoded  data  after
 having found all embedded files. It makes a
 huge difference  when  dealing  with  large
 MAIL!.CIS files.
 *  New   icon   courtesy   of   Fred   Luck
 [100675,3647]. Thanks Fred :-)

 v1.1 05 Nov 1996
 * DYL upgrade from v1.0 to v1.1 The new DYL
 uses the  grave-accent instead  of a  space
 character to prevent word-wrapping by  CIS,
 JBCIS and Word.

 v1.1 03 Nov 1996
 *  Fixed  bug   that  caused  an   'Invalid
 arguments' error when decoding.
 * Fixed bug  that allowed the  entry of  an
 empty   filename   in   the   decode/encode
 confirmation dialog.
 * Changed  default UUencode  'Maximum  line
 length' to  36  characters,  and  corrected
 documentation, as ReadCIS sets line  length
 to 40 characters (not 50) when no WRAP80.ON
 switch is set.
 *  Added  check  of  MAILxx.OUT  file  when
 attaching. Attach  is only  allowed if  the
 MAILxx.OUT is a text file.
 * Reduced OPA code size by 17 bytes through
 optimising IF...ELSE...ENDIF code.
 * Changed 'No begin line' error message  to
 'No files found' to make it easier for  new
 users to understand.
 * Improved 'end' detection. Now both  'end'
 and '`' will stop the decode, and any other
 invalid line is ignored.

 v1.0 28 Oct 1996
 * Initial release.

 To Do
 -----
 * Handling of UUencoded files split  across
 multiple or single files, especially  with.
 ReadCIS and ftpmail.
 * MIME (base64) format encode and decode

 Author
 ------
 Jochen Siegenthaler
 100127.3167@compuserve.com
 Zurich, Switzerland
 06 Dec 1996