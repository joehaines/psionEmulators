

                               Unzip v1.5
                              ============

 1. About Unzip
 ==============
 Unzip is a freeware  user-friendly shell for  the UNZIP.IMG image  file
 from David Palmer. It allows you to  unzip ZIP files on your Psion  3a,
 3c or Siena.  I wrote this  application as I  could never remember  the
 command line options I needed to unzip files. As David had kindly added
 full OPL  launch support  from  version 1.4  of  his UNZIP.IMG,  I  was
 inspired to create this very useful shell for David's unzip utility.

 Unzip can  be used  as a  replacement  for, or  a supplement  to  David
 Palmer's RUN.APP.

 Currently (March 1997)  Unzip is only  available in English,  but I  am
 working on German and other language  variants. Check out my Unzip  web
 page for the latest developments:

 http://ourworld.compuserve.com/homepages/Jochen_Siegenthaler/funzip.htm

 2. Licence Agreement - IMPORTANT
 ================================
 Before using  Unzip, you  must read  and agree  with the  terms of  the
 accompanying file LICENCE.TXT.

 3. Features
 ===========
 * Easy to use dialog with all options available at a glance

 * Supports launch from 3c "Files" and JBLaunch

 * Supports unzip of individual or multiple files

 * Supports selection of destination directory

 * Supports archive test, archive comment, file lists

 * Automatically locates UNZIP.IMG in common directories on any disk

 * All selections are memorised between use

 * Optional online help

 * Very small application file size (around 3K)

 4. Archive Contents
 ===================
 When you unpacked the archive you should have found these files...

  LICENCE.TXT        User licence agreement
  README.TXT         This documentation file
  FUNZIP.OPA         3a & 3c application file
  FUNZIPS.OPA        Siena application file
  FUNZIP.HLP         3a & 3c help file (optional)
  FUNZIPS.HLP        Siena help file (optional)

 5. Installation
 ===============
 NOTE: Depending  on your  Psion model,  you should  install either  the
 3a/3c variant, or the Siena variant, but not both.

 5.1 Remove  any  prior  installation  of  Unzip  by  deleting  existing
     UNZIP.OPA and \OPD\UNZIP.INI files.
     NOTE: check all \APP\ and \APP\UNZIP\ directories on all disks  for
     old UNZIP.OPA  files.  The UNZIP.INI  file  will be  in  the  \OPD\
     directory on your default disk.

 5.2 If it does not already  exist, create a \APP\UNZIP\ directory on  a
     disk of your choice. This  directory will hold the application  and
     help files. These files are suitable for storage on Flash SSDs,  if
     you have one.
     NOTE: the actual name of the directory is irrelevant; you may  name
     it to anything you wish.

 5.3 Copy FUNZIP.OPA (3a/3c) or  FUNZIPS.OPA (Siena) to the  \APP\UNZIP\
     directory.

 5.4 If you require online help, copy FUNZIP.HLP (3a/3c) or  FUNZIPS.HLP
     (Siena) to the \APP\UNZIP\ directory.
     NOTE: Help is optional; if you  already know how to Unzip and  wish
     to save disk space, you can omit the help files.

 5.5 Install Unzip from the System screen with Psion-I (Apps | Install).
     You will have to use Tab  to navigate to the \APP\UNZIP\  directory
     to find the FUNZIP.OPA (or FUNZIPS.OPA) file.

 5.6 If you  do not already  have UNZIP.IMG installed,  then you  should
     install it now.
     There are three installation options:
     a) if  you  are just  using  Unzip  to unzip  files,  then  install
     UNZIP.IMG to the \APP\UNZIP\ directory.
     b) if you wish to use David Palmer's RUN.APP in addition to  Unzip,
     then the UNZIP.IMG file must be in a \RUN\ directory on any disk.
     c) if required by other programs,  you may install UNZIP.IMG in  an
     \IMG\ directory on any disk.
     NOTE: you only need one copy of UNZIP.IMG on your Psion: either  in
     \APP\UNZIP\, or in \RUN\, or in \IMG\.

 6. UNZIP.IMG
 ============
 The UNZIP.IMG image file  from David Palmer is  mandatory for Unzip  to
 work. Unzip will only work with the latest version (v1.50, either S  or
 L variant) of UNZIP.IMG.

 Unzip can detect UNZIP.IMG on any disk (including disk C) in the \RUN\,
 \IMG\ or  the directory  where FUNZIP.OPA  (or FUNZIPS.OPA)  itself  is
 installed.

 NOTE: UNZIP.IMG  is *not*  supplied with  this archive,  however it  is
 freely available as...

      Archive          Version
     S3UZ150L.ZIP       v1.50l (preferred)
     S3UZ150S.ZIP       v1.50s

 These files (or the pre-extracted UNZIP.IMG) can be retrieved from...

 * http://www.psion.com/psionworld/unzip.htm

 * http://3lib.ukonline.co.uk/unzip.htm

 * ftp://sunsite.doc.ic.ac.uk/computing/systems/handhelds/psion/
 icdoc/utils/

 * CompuServe CIS:PSION library 18 (Psion files)

 Note that  FUNZIP.OPA  and UNZIP.IMG  (v1.50l)  are also  available  in
 "ready to use" unzipped format from CompuServe, the 3Lib site, and  the
 PsionWorld  site.

 For a detailed explanation of the difference between v1.50l and v1.50s,
 see "Variants of UNZIP.IMG".

 

 7. Using Unzip
 ==============
 Unzip contains a simple dialog  for selecting the usual unzip  options.
 All options  provided by  UNZIP.IMG v1.50  are supported  by the  Unzip
 shell. Press Esc from the main dialog to exit Unzip.

 When you exit Unzip, all settings  are saved to a \OPD\FUNZIP.INI  file
 on your default disk.

 7.1 Unzip from [Siena: From]
 ----------------------------
 Press Tab to bring up the file  selector to select and navigate to  any
 file. You can also access remote drives on a PC - see "Accessing Remote
 Drives with Unzip".

 7.2 Unzip to [Siena: To]
 ------------------------
 Use Tab to navigate to a desired destination directory.

 7.3 Action
 ----------

 The action options determine the work that the UNZIP.IMG file will do.

 7.3.1 Extract files
     Extracts files from the archive.

 7.3.2 Update existing & create new [Siena: Update files]
     Extracts files only if  they have a  newer timestamp than  existing
     files  in  the  destination  directory  (existing  files  will   be
     overwritten), and also creates any new files that do not exist.
     This is a good choice if you want to unzip an archive directly to a
     flash SSD. Unchanged files will  *not* be overwritten, thus  saving
     precious flash SSD space.

 7.3.3 Freshen (update existing only) [Siena: Freshen files]
     Same as above  except that *no*  new  files  are created. The  best
     choice to update an existing program and *not* unzip files that are
     *not* already installed (eg: text files and optional files).
     A good choice when unzipping a newer program version directly to  a
     flash SSD.
     NOTE: UNZIP.IMG complains if you  choose this action together  with
     the overwrite option "Never".

 7.3.4 Test archive
     Check integrity of the archive's files. Also checks if  compression
     method is known.

 7.3.5 Display archive comment [Siena: Display comment]
     Does just that. Many archives do not use the comment feature.
     NOTE: On the Siena Word is used  to view the comment. On 3a/3c  the
     comment is shown on the screen.

 7.3.6 List files (short)
     This shows just  size, date,  time and name.  of the  files in  the
     archive.
     3a/3c: The file list is displayed directly to the screen.
     Siena: A "\WRD\Filelist.wrd"  file is created  on the default  disk
     (which is overwritten each time) and  Word is launched to view  the
     file list.

 7.3.7 List files (long)
     This does  the same  as the  short  list but  displays a  bit  more
     information about each file.

 7.3.8 View filelist in Word (short) [3a/3c only]
     Creates a "\WRD\Filelist.wrd" file on  the  default disk (which  is
     overwritten each time) and then starts Word to view the list.  Very
     useful to see what files are in the archive. Viewing in Word  gives
     a  better  file  display  then  the  "List  files"  action   above,
     especially when the archive contains many files. You can also print
     the file list from Word.

 7.3.9 View filelist in Word (long) [3a/3c only]
     Same as above but with more information about each file.

 7.4 File types
 --------------
 Press Enter to bring  up the "Specify file  types" dialog. This  allows
 archive actions to be restricted  to files that match certain  filename
 patterns. To specify a pattern not shown in the list, select Custom  as
 the "Filename pattern" and enter a custom pattern on the second line of
 the dialog (case is not important).
 Eg: a  custom pattern  of "*.opa"  and selecting  "Extract files"  will
 extract all .OPA files. Use a full filename if you wish to extract just
 one file.

 7.5 Overwrite
 -------------
 NOTE: overwrite options do not apply to the "Create file list" action.

 7.5.1 Never
     Unzip will *never* overwrite any existing files.

 7.5.2 Always
     Unzip will *always* overwrite existing files.

 7.5.3 Prompt
     Unzip prompts  for  input  if  it finds  an    existing  file  when
     extracting.

     NOTE: on  a Siena,  this information  may be  off the  edge of  the
     display. This is due to a limitation of UNZIP.IMG

 7.6 Options
 -----------

 7.6.1 None
     No options.

 7.6.2 Discard paths
     If the  archive contains  paths, normally  they are  extracted  and
     created when  unzipping. Choosing  this option  will *prevent*  the
     paths from being  created, and all  files will be  unzipped to  the
     same directory.

 7.6.3 Quiet operation
     Prevents some text from being displayed  on the screen or saved  to
     the file list.

 7.6.4 Very quiet operation
     Even quieter than quiet operation.

 8. Variants of UNZIP.IMG
 ========================
 UNZIP.IMG v1.50 comes in two variants:

 v1.50l (large variant, 42160 bytes, *preferred*)
     Can extract files compressed with both PK-ZIP v1.x and PK-ZIP v2.x.
     Install this  variant  if  you have  the  space  available  (highly
     recommended).

 v1.50s (small variant, 32816 bytes)
     Can only extract files compressed  with PK-ZIP v2.x. Attempting  to
     extract files compressed with earlier versions of PK-ZIP will cause
     "unknown compression method" errors.
     On a 3a or 3c, the existence of v1.50s is indicated by the  display
     of "(for PK-ZIP v2.x only)" in the dialog title.

 9. Problem Solving
 ==================
 9.1 Unzip shell errors
 ----------------------
 Error messages that show "Unzip" on the top of the screen and the Unzip
 icon are from the Unzip shell.

 9.1.1 "Unzip.img error  File does not exist"
     The file  UNZIP.IMG  cannot  be found.  Reinstall  UNZIP.IMG  v1.50
     according to the installation instructions above.

 9.1.2 "Unzip.img error  Wrong version"
     The UNZIP.IMG file is not v1.50.

 9.1.3 "Error in FUNZIP\B  File or device in use"
     The FUNZIP.INI file is currently open by another app. Maybe you are
     viewing it in Word?

 9.2 Image file errors
 ---------------------
 Error messages that occur when "UnZip  v5 for EPOC (version 1.50x)"  is
 displayed are from the UNZIP.IMG file.

 9.2.1 "can't allocate the work area"
     Not  enough  free   system  memory  for   UNZIP.IMG.  Close   other
     applications and delete unnecessary files to free up system memory.
     UNZIP.IMG needs about 100k free system memory.

 9.2.2 "Cannot create [file]: write error or (disk full?)"
     UNZIP.IMG could  not unzip  any  more files  as  the disk  is  most
     probably full.  Close  other applications  and  delete  unnecessary
     files to free up some disk space.

 9.2.3 "End-of-central-directory signature not found"
     Archive is not a PK-ZIP compatible archive.

 9.2.4 "error:  invalid response [x]"
     UNZIP.IMG expected a certain key and you pressed key [x] instead.

 9.2.5 "unknown compression method"
     The archive was probably created with PK-ZIP v1.x and you are using
     UNZIP.IMG v1.50s. Install v1.50 variant L.

 10. Illegal Combinations
 ========================
 UNZIP.IMG will complain if you try to freshen files when "Overwrite" is
 set to "Never"

 11. Accessing Remote Drives with Unzip
 ======================================
 Accessing remote drives is possible with either PsiWin or RCom.  Remote
 drives will always appear in the file selector prefixed with "REM::"
 NOTE: The default setting of PsiWin will not allow remote access of  PC
 files from the Psion. You can  change this by editing the  PSIONPRC.INI
 file in the Windows  directory, and removing the  "-x" from the end  of
 the line "engine=prcenwin.exe -x"

 12. Acknowledgements
 ====================
 Full and grateful  acknowledgements go  to David  Palmer of  Edinburgh,
 Scotland, for his wonderful UNZIP.IMG and his excellent documentation.

 13. Change history
 ==================
 v1.5 24-Mar-97
     * New icon (from Serge Shestapolov)
     * Renamed to FUNZIP.OPA (and FUNZIP.HLP)  to work around an OS  bug
     that caused problems with JBLaunch and 3c "Files"
     * Psion+Menu now shows correct app name when launched from  "Files"
     or JBLaunch
     * Minor cosmetic changes to the dialogs
     * 1936 bytes less system memory used when UNZIP.IMG is run
     * Word now used to view filelists
     * New Help file and layout
     * New Siena variant created
     * System screen filelist now updates correctly
     * All options now saved between use

 v1.4 03-Dec-96
     * Added Control+Help support for Help:Index
     * \ZIP\*.ZIP files now appear under  the Unzip icon. You can  start
     Unzip by selecting a file.
     * UNZIP.IMG file can now be in the UNZIP.OPA directory, in \RUN\ or
     in \IMG\ on any disk.
     * Unzip files can now be spread across all disks
     * Unzip now shows correct program name in the dialog when  launched
     by JBLaunch or the 3c "Files" app.
     * Changed file type  to a separate dialog  and added "Custom"  file
     type. You can enter your own wildcards or even individual filenames
     to extract. The names are NOT case sensitive
     * Ini file is now \OPD\Unzip.ini

 v1.3 09-Aug-96
     * Bug fix: File Types = Word now correctly  selects *.wrd files.

 v1.2 21-Jun-96
     * Created series 3 classic variant (v1.2c).
     * Reduced runtime memory requirements by 47%.
     * File selector now shows all files (* instead   of *.zip).
     * Removed support for earlier versions of   UNZIP.IMG to force  the
     user to upgrade to   v1.50 (s or l) and to reduce code size.
     * Added better error handling when trying  to   find or detect  the
     UNZIP.IMG file.
     * Changed the icon.
     * Unzip now remembers last directories used in UNZIP.INI.
     * Added (optional) online help.
     * Added ability to extract certain "File types".
     * Added "Create file list" action.
     * Rearranged the dialog layout and changed some  dialog text.

 v1.1 07-May-96
     * Added support for UNZIP.IMG v1.0/1.2/1.3.
     * Renamed a couple of options and fixed a spelling mistake.
     * Optimised code to make the .OPA as small as  possible.

 v1.0 01-May-96
     * Original version (only for UNZIP.IMG v1.41).

 ==
 Jochen Siegenthaler
 100127.3167@compuserve.com
 Zurich, Switzerland
 24 March 1997