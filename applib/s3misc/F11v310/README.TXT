                     IMPORTANT INFORMATION
                     =====================

     !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
     !  Before unzipping the application files in PRODUCT.ZIP  !
     !  please read LICENS.TXT.                                !
     !  You must not unzip PRODUCT.ZIP only if you do not      !
     !  agree with the contents of LICENS.TXT                  !                                      !
     !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

     Installation
     ------------
     Please see INSTALL.TXT.
     
         --------------------------------------
         ! The unregistered version of this   !
         ! program displays a reminder every  !
         !         500 keystrokes             !
         --------------------------------------


Registration
------------

					
You can either directly register with us (A$19.95)
by filling out form ORDER.TXT and send it to us by mail, email, or FAX.

    Berthold Daum
    lichtbild pty ltd
    21 Margate St.
    LPO Box 6011
    Cromer, Vic 3193
    Australia

    CompuServe 100026,3365
    email: lichtbild@compuserve.com
    WWW: http://ourworld.compuserve.com/homepages/lichtbild
    fax: +61-3-9584 9638

or you can register via CompuServe: GO SWREG.


 Author:            Berthold Daum
 CompuServe ID:     [100026,3365]

 Program Title:     F11 S3A V3.0
 Registration ID:   5036
 Fee (US$):         19.95

 Shipping & Handling Fees: nil


If you have a CompuServe or Internet address we will supply the
registration number by email, otherwise by mail.

Entering the registration number:

   Press Shift-MENU, / , ENTER, ENTER,
   then enter your name and registration number.
   
Note: The entered name should be part of the machines owner name.
      You need a registration number issued on the entered name.



                        F11 Version 3.0
                        ===============

Program Description
-------------------

F11 enhances the inbuilt WORD,DATA,AGENDA, and SPREADSHEET applications
of the PSION Series 3a:

 - change automatically letters from lower case to upper case.
 - insert automatically spaces.
 - doubles automatically hyphens.
 - insert todays date
 - insert non keyboard characters from character table
 - insert standard text phrases from custom defined phrase books.
 - easy typing of national characters (umlauts).
 - Find file function with preview
 - Establish hyperlinks within and between documents
 - Execute KMAC-macros and OPL-functions
 

Platforms
---------

 This program runs on
   - PSION S3a/S3c/Siena
   - ACORN PocketBook II
   
   - S3a-Emulator (480x160 or 640x400 screen size)
   - Siena-Emulator (240x160 screen size)
     Due to the different keyboard layout some key combinations do not work.
     To access F11-Menu or F11-Help use ALT-ESC followed by F9 or F10.

Requirements
------------

   The amount of disc space needed is 62k-73k for the program
   depending on the options and 30k-50k for the manuals.
 
Compatibility
-------------

 - F11 and Macro systems (MacSys/KMAC)

   Using the same hotkeys: The macro system will get the keypress.

   Under certain circumstances the system can "freeze" (see below).
    This seems to happen if you have defined an extreme number
    of hotkeys for macros. (We used 25 defined hotkeys without any trouble)
    Either reduce the number of macro-hotkeys
    (you probably have problems to remember them, too),
    or switch off the F11-hotkeys (Preferences).

   Since F11 can record,run, edit and translate macros of
   both MacSys and KMAC, there is no real need to keep a macro system
   running...
   
-  F11 and DataView
   F11 supports DataView with the following restrictions:
   - DataView cannot be started from the File finder.
   - It is not possible to merge two data files
   - TAB-characters in phrases are converted to blanks when F11 is used
     with DataView


When things go wrong
--------------------

   Problem:   Not enough memory.
   
   Solution:  Switching off F11 (PSION-X in F11 menu or DEL on F11 icon)
              will release some memory.
              Work is continued under the native application.

   Problem:   A system dialog box has appeared on the screen and
              asks you for the Enter or ESC key, but seems not to respond
              to these keys.

   Solution:  Use CONTROL-Enter or CONTROL-ESC to answer the prompt.

   Problem:   You get an error message "Error capturing keys"
              and F11 closes down.

   Solution:  There is probably another application running capturing keys
              that F11 needs to capture (We heard of Pason4C doing so).
              Close that application before starting F11.


The following situation should never occur, however, if they occur:


   Problem:   F11 does not terminate or shows "busy" continuously.
   
   Solution:  Press the SYSTEM button.
              Then apply the system function "Kill application" to F11.
              (You will not loose data). Restart F11.

   Problem:   The system freezes (does not respond to keystrokes).
   
   Solution:  Press Shift-ESC. That should disable F11.
              If this does not help: Reset the machine.
              Press the reset button (the little hole
              left of the SYSTEM button) using a wire or similar. Then
              press the ON button.
              Do not remove the batteries as this would wipe out the
              whole internal disk!

New in version V3.10
--------------------
   - PSION Siena supported. Menu items have been rearranged. On Siena menu titles
     are abbreviated.
   - Quick positioning with alpha key is now possible in the phrase index
     and in the RepPoc-textfunction index.
   

New in version V3.00
--------------------
    Note:
        The format of the F11 configuration file has changed.
        If you had used previous versions of F11 the installation
        process will convert your old INI files into a new CNF file.
        
  General architecture
  --------------------
    F11 V3 features a completely redesigned task management.
    Instead of running a separate F11 copy for each F11 session,
    one global F11 task handles all active sessions.
    You start F11 once and leave it running on your system
    in the background. As soon as F11 is started the F11 functions become
    available, even to open documents. You can start and stop F11 at any time
    - it will attach itself to the application in the foreground.
    F11 will enable typing aids depending on the preferences for
    this application.

    Advantages compared to V2:
    
    - less memory used and better performance when running parallel sessions.
    - support for sessions that are launched by other applications as long
      as they are of type WORD(incl. Text,OPL),DATA,AGENDA,Sheet,DATAVIEW.
      This includes support for READCIS, 3CIS, RepPoc, ProPoc, HyperPoc,
      PocDoc, etc.
    - applications start as fast as without F11.
    - no need to install F11 frontends and to deinstall standard applications.
      original icons, alias lists, program groups and button assignments
      are kept.
    - global history (list of recently opened documents and hyperjumps)
      Very convenient for reopening recent documents.

  Changes to the user interface
  -----------------------------
    To support the S3c better some key combinations were changed.
    
    - PSION-TAB (Command key) has been replaced by PSION-ESC.
    - Delete Phrase function has been combined with UNDO
      PSION-Shift-DEL.
    - PSION-HELP (Insert Hypertext link) has been replaced
      by PSION-ESC followed by Cursor down.

  Typing
  ------
    - Conversion to upper case after punctuation occurs now only
      if the punctuation is followed by blank (or automatic blank)

    - CAPS is no longer used to disable typing aids.

    - Shift-Spacebar disables typing aids for the next word
      (until next Spacebar, Shift-Spacebar, ESC, TAB, Enter).

    - More formats for the Insert-Date-function can now be selected
      via Preferences.

  Changes to the phrase book
  --------------------------
    You can now create a global phrase book for commonly used phrases
    and macros. F11 will search for a phrase first in the application
    specific phrase book, then in the global phrase book.

    Phrases, macros and OPL programs can now be invoked with hotkeys
    (CNTRL-letter or CNTRL-Shift-letter). This is defined in the
    phrase book.

    Link entries to macros and OPL programs can now have a description
    part which is displayed in the phrase index.

  Changes to the macro interface
  ------------------------------
    A new function was added to edit macros. You can now record, edit,
    translate, and run macros from all 3 macro systems
    (KMAC, MacSys2, F11(OPL)) under F11.

  Changes to the OPL interface
  ----------------------------
    The parameter handling had been modified:
    - parameter specified: Parameter is passed to subprogram (as before)
    - no parameter, text selected: Selected text is passed (as before)
    - no parameter, no selection: F11 selects the word at the
      cursor position and passes it to the subprogram.
    - otherwise: empty string to subprogram.

  New subprograms
  ---------------
     BRACKETS, BRACKETS, SEE, UCLC, UNIQUE.

  Hypertext
  ---------
    The hypertext history now works global and logs all sessions
    supported by F11. This is a convenient way to access your
    recently edited documents.
    
    There is now also a hypertext 'Forward' function. Obviously it
    works in the opposite direction as the 'Back' function.

    Hyperlinks can now be included into DATA records and the AGENDA.
    
    Hyperlinks can now point to HTML files. (see below)
    At present only local HTML files supported. Support for URLs
    will follow when a browser for the S3c becomes available.
    
  File finder
  -----------

    The file finder can now display a flat lists of all files including
    the files in the subdirectories, or shows the subdirectories
    as separate entities. Open a subdirectory with ENTER. Move a level
    upwards with '-'.
    There is also now a sort option (slower).

    The file finder now recognises more file types, displays
    additional information about files, and can replay wave (WVE) files.
    
    HTML files can now be opened through the file finder.
    ( HTMLVIEW from DVM Consultants Ltd. required. )

Changes to the documentation
----------------------------
    The manual was split into two parts (F11.WRD and F11MAC.WRD).
    The second describes the Macro/OPL interface and the subprograms.

New in version V2.11
--------------------
    - Support for Tom Dolbilins Macro System 2 added.
      Record and run MacSys macros from F11 (just like KMAC macros).
    - You can now record and run simple macros in OPL-format
      without an extra macro system installed.
    - A history function for hypertext was added.
    - The RepPoc text function support has been enhanced.
    - Address assistant (F11aa) plug-in available (separate file).

New in version V2.05
--------------------
    - Additional options for hyphen control.
    
New in version V2.03
--------------------

    - F11 supports now the Spreadsheet application (F11s).
      By default F11s uses a special phrases book (shtphras.wrd).
    
    - Pressing PSION-TAB will no longer insert chevrons into the current
      document. Instead the chevrons appear in the top left corner of the
      screen to preserve existing selections.

    - There is now a new menu title with 3 separate functions to
        - edit the phrase book (B)
        - to switch to another phrase book (Y)
        - to create a new phrase book (N)

    - You can now open the phrase book from the phrase index (PSION-TAB+TAB)
      by pressing Shift-ENTER.

    - You can now invoke KMAC scripts and macros in the same way as
      any phrase.
      You must have created an entry in the phrase book to link
      the keyword to a macro or script file, and the KMAC module LPC.DYL
      must be installed. Please see the manual for further details.

    - You can now invoke OPL programs in the same way as any phrase.
      You must have created an entry in the phrase book to link
      the keyword to an OPO-module. (You even may call KMAC commands
      from inside the OPO module). Please see the manual for further
      details.

    - You can now record keypresses. The result is stored as a KMAC
      script file (see above). Some actions like merging
      a temporary files into a document are not recorded.
      

Documentation
-------------

Use F11.WRD and F11MAC.WRD as an online documentation under WORD.
For a file list see the end part of English.IPF or Deutsch.IPF

















