                     IMPORTANT INFORMATION
                     =====================

     !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
     !  Before unzipping the application files in PRODUCT.ZIP  !
     !  please read LICENS.TXT.                                !
     !  You must not unzip PRODUCT.ZIP only if you do not      !
     !  agree with the contents of LICENS.TXT                  !                                      !
     !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!


     Ordering
     --------
     Please see ORDER.TXT

     Installation
     ------------
     Please see INSTALL.TXT.

     Features and Performance
     ------------------------
     Please see FEATURES.TXT

     Description
     -----------
     Please see MANUAL.TXT

     Printing
     --------
     Please see PRINTING.TXT

     Manual
     ------
     We ship a 72 page printed manual on registration.
     To preview the manual, please download ProPoM.ZIP (ADOBE Acrobat format).
     
     
     Registration
     ------------

            Berthold Daum
            lichtbild pty ltd
            21 Margate St.
            LPO Box 6011
            Cromer, Vic 3193
            Australia

            CompuServe 100026,3365
            INTERNET: 100026.3365@compuserve.com
            HomePage: http://ourworld.compuserve.com/homepages/lichtbild
            fax: +61-3-9584 9638

     The registration fee is

            A$59.95 + A$6.00 postage and handling
	

     Alternatively, you can register through CompuServe (GO SWREG):


		Author: Berthold Daum
		Program title:	ProPoc V2.0 LITE
		Program name: PROPOL.zip
		RegNo: 11335
		Fee: US$ 49.95


	 -----------------------------------------------------------
	 |   Warning!!!!!!!                                        |
	 |   When used without a registration number the following |
	 |   restrictions apply:                                   |
	 |   1. Number of activities in one file restricted to 15  |
	 |   2. Number of milestones in one file restricted to 4   |
	 -----------------------------------------------------------

Professional version
--------------------

The fee for the professional version consists of

	1. Fee for lite version A$59.95 + A$6.00 postage and handling
	2. Upgrade fee: A$39.95
	

Alternatively, you can upgrade via CompuServe (GO SWREG):


		Author: Berthold Daum
		Program title:	ProPoc V2.0 PROF (UPGRADE)
		Program name: PROPOP.zip
		RegNo: 11605
		Fee: US$ 29.95



Entering the registration number
--------------------------------

   Press MENU, / , ENTER, ENTER
   then enter first name and the registration number.

   Note: The entered name must be part of the machines owner name.
         The registration number must be issued on the entered name.




                ProPoc Version 2.1 - Release Notes
                ==================================

Program Description
-------------------

ProPoc is a project planner.
It can handle multiple projects, activities, milestones, and resources.

Activities and milestones can depend on the completion of other
activities. Activities can share resources, even between
different projects.

ProPoc determines the StartDate and EndDate of each activity,
the ActualDate of each Milestone, the number of slack days,
and if a activity or milestone is critical.

The results are displayed in graphical or table form on screen or
can be printed in ASCII- or WORD-form (tables). The Professional version
allows output to SPREADSHEET and AGENDA, too.

For a detailed comparison between PROPOC V2 LITE and PROPOC V2 PROF
see FEATURES.TXT .


Platforms
---------

The program runs on PSION S3 and S3a, and Acorn PocketBook I & II
in native mode.

ProPoc can also run on the S3a-emulator where it can use
screen sizes up to 640x400.

( Set SERVER_PARAMS -X640 -Y400 -D in HCSERVER.PAR for the big screen)



Requirements
------------

ProPoc LITE:
The disk space requirements are 100-155k depending
on the options.

ProPoc PROF:
The disk space requirements are 150-255k depending
on the options.

When active, the system requires a minimum of about 40k main memory.
Some functions can require considerable more memory.

We recommend to install the system on flash SSD, 
the PROJ folder (data drive) on the internal disk or on RAM SSD.

Compatibility
-------------

ProPoc files from Version 1.00-1.49 are converted into V2 file format.
Files from the V2-beta test version are not converted.


New in version 2.10
-------------------

- Preferences rearranged in 3 groups

- Table styles (Preferences)
  Load/save current the table setup from/to file.
  A table style includes all table definitions
  (Screen/Printer/Textfile/Wordfile  in Outline/Detail mode),
  the table font, and all filter settings (PROF).
  Table style files are stored in the same directory
  as the project files and have the extension .PRS.
  Table style files can be used and reused from any
  project file.

  We have added 3 example table styles:
  Normal.prs    Normal setting (to reset tables)
  Finance.prs   financial overview
  Crit.prs      overview about critical tasks (PROF)

- Summary tasks (PROF)
  Create a summary task with the insert function.
  To create a new task within the summary task place
  the cursor on the summary task or on a task within the
  summary task and use the insert function.
  
  To move a task into a summary task, use the Link function.
  Place the cursor on the task you want to move, invoke the
  Link function and navigate to the target summary task.
  Then press ENTER. ProPoc will automatically update WBS numbers.

  To move a task out of any summary task, use the Unlink function.
  
  You can nest summary tasks. There is not really a limit
  to the depth of nesting, but we suggest to use not more
  than 5 levels.

  You can restrict the displayed level of detail for
  each display mode separately (Preferences, View, Show parameter).
  
  Warning: If you delete a summary task, only the summary
  task will be saved into the clipboard, not its contents!
  
  

New in version 2.04
-------------------

- New printing options
  - Landscape format supported on some printers
  - Number of copies can be specified
  - collated/uncollated on some printers
  - detection of printer time out (out of paper, etc.)

- MPX-support for German version of MS Project added (PROF).
  This module is in beta status. Comments and help welcome.

  MPX-export indicates now code page 850 for correct
  code translation by the importing application.
  MPX-import will now translate ANSI-code (WINDOWS) into
  code page 850 (PSION).

- We have dropped the requirement that task names must be
  unique through the whole file. Now task names must only
  be unique within the same project.
  This has consequences for attached documents. Documents
   are identified by the task name. This allows you to
   use the same document for different, but identical
   named tasks.
   However, you can force documents to be identified
   with a unique task ID by including a special character
   (other than letter, numeric digit, or underscore)
   into the task name.




