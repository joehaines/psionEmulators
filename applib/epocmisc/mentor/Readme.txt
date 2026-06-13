Mentor - the future of task management
======================================
Release 05.064 production

Installation
------------
Double-click on the "mentor.sis" file on a PC with PsiWin installed, or
copy the file to the Psion, and tap on it from the System screen.

Help and Documentation
----------------------
"What's this?" style help available through CTRL-SHIFT-H
Full HTML manual available as a "ZIP" file from:
   http://www.wuli.demon.co.uk/wulisoft/mtmanual/manual.zip

Major Changes Not Yet Documented in the Manual
----------------------------------------------
General:
 Reorganised preferences dialog (with more preferences)
 Context pop-ups show structure of role/goal hierarchy
Task/Schedule View:
 Much improved filters
 Added "Sort", "Find", "Find Next", "Goto" functions
 Improved Notes - auto deleted if empty, accessed via pen ...
Task View:
 "Tree" pop-up window added to navigate composite task hierarchy
Schedule View:
 Replacement of "type" column with a "due date" column
 Improved navigation using space bar and "Goto"
Role View:
 Improved appearance in medium zoom (usage bars)
 Preference to display 2 rows instead of 3 rows

Full Release History
--------------------
05.064  new "rebuild" option on reporting an error

05.062  bug fix - problems with opening some tidied files
	bug fix - tidy file and upgrade indexing updated
	bug fix - fixed usage bar display in RV medium zoom
05.059	bug fix - fixed problems with filters when no context defined
	bug fix - blocked entry of filters with no name
	extended CTRL+SHIFT+J DB repair to fix filters
05.058	context header button with mask and crossing out
	extended CTRL+SHIFT+J to fix more time slot problems
	bug fix - CTRL+SHIFT+J recovery slot scanning fixed
	bug fix - case sensitive find problems fixed
	bug fix - schedule view refresh on s5 fixed
05.056	fixed ordering by context in Role View
	sorting in Schedule/Task view (CTRL-S)
	bug fix - Find updated to use new filters
05.055	advanced filters
	added file secure plus compression after major DB changes
	put goal indenting on a preference
	improved feedback messages for secure file/scheduling
	bug fix - default context set to "undefined" (not last in list)
	bug fix - context deletion selects correct goal
	bug fix - composite task edit from menu refreshes popup
05.053  bug fix - stopped "Find" in schedule view finding undated entries
	bug fix - replaced "tasks scheduled" with "allocated size" for complete slots
	bug fix - blocked contexts with no name being entered
	bug fix - maximum number of contexts and text sizes policed correctly
	context pop-up - improved to show structure
05.051  bug fix - problem with refresh in TV/SV if task out of filter
        reorganised preferences dialog
        added "numerals for due date" preference (SV)
05.050  longer minor version numbers to cope with the release frequency!
        added Find/Find next in all views
        added Goto/CTRL+G in TV/SV/RV
        added CTRL-Q/CTRL-SHIFT-Q forwards/backwards
        notes - automatically deleted if empty
        notes - attempted "are you sure?" for pressing Esc - but can't do it :(
        notes - accessible via the pen
        notes - not highlighted when opened - the cursor is placed at the end
        changed "tasks scheduled" in time slot dialog to text fields (to avoid confusion)
        changed scheduling GIPRINT to BUSY - more accurate busy feedback
05.49   Bug fix : Cannot create composite without tree view ever being activated
        Type column changed to indicate time to due date in SV
        Changed usage bars in RV (removed used symbol, extended bar)
        Changed RV medium zoom to usage bars (especially good with 2 week preference)
        Request : Removed initial "\" from hierarchic task name
05.47   Bug fix : flattening (Task View) caused error
        Bug fix : CTRL-SHIFT-J problem if within composite task
        Space bar always goes to Today from other dates in SV
        TV/SV feed back date to which task has been scheduled (unless current SV date)
        Completed time slots displayed in grey
        Logo and tree window centered better on screen (especially series 7)
        A spelling correction within displayed text
05.46   Bug fix : notes window crashed with empty description field
        Bug fix : left-right movement fixed at start/end of rows in RV
        Bug fix : error when moving tasks to new composite using Tree function
        Bug fix : SV refresh when today set automatically
        Feature : "Today" made more distinct in RV
05.45   Major new release - particular emphasis on "tree" features
        Task view:
           navigate to any point in the hierarchy via PopUp dialog (space bar)
           "Advanced" and "Tree" buttons within New/Edit task give new features
           preference (default ON) to display full "tree" for current composite task
        Schedule view:
           same "Advanced" and "Tree" buttons within New/Edit task dialogs
            - "Tree" allows position in hierarchy to be set easily from Schedule view
           space bar toggles dates (set by "Goto"/Role View)
           after reschedule, "Today" is set, or accessed via space bar
           preference (default OFF) to display full "tree" for all tasks
        Role view:
           preference (default 3) to display 2 weeks rather than 3 (so more entries/day)
        Bug fixes:
           fixed bug with Tidy file when deleting entries
           fixed bug where context symbol changes not displayed until Mentor restarts
           fixed bug where RunApp& could cause crash (e.g. 1st use from Extrabars)
           improved randomness of internal random file numbers
        Note:
           the tree PopUp window is treated as any other dialog 
            - Mentor does not respond to system events while displaying dialogs
05.42   Fixed 5mx refresh bug, further automated file recovery extension
05.40   Fixed Tidy file bugs, improved automated file recovery
05.39   Minor bug fixes - CTRL+SHIFT+J recommended
05.38   Full reschedule on date change (rather than incremental) by default
        Full reschedule option for time slot completion
        Task Bold set/clear using CTRL-B in Task/Schedule modes
        "Force" option improved to force new dates if they are changed
05.35   Minor bug fixes, checkbox to allow new file to be created in "Tidy"
05.34   Change in the way license keys are processed.
05.33   This is the first shareware release of Mentor.

Rebuild Option
--------------
If you have an error reported from Mentor, follow this process:
   try to simply reopen the file - Mentor may auto repair itself
   if the file opens but the problem persists, use CTRL+SHIFT+J
   if the file will not open, select "rebuild" from the error dialog
The rebuild option causes the database to be rebuilt (then repaired)
when it is next opened (Mentor exits when you press the button).

This facility has been provided due to index corruptions caused by the 
operating system when the system runs out of memory.

New Features
------------
Many more new features are planned - these will form part of later releases.
These include:
   Help (manual) available from within Mentor
   Synchronise with Agenda
   Printing

            *******************************************************
            *********** Please read the on-line manual ************
            *******************************************************

Legal Stuff
-----------
Mentor is copyright (c) Ben Thornton 1996-1999.
All rights reserved.
The software is supplied "as is" - use the software at your own risk.

(C) Copyright Ben Thornton 1996-1999

Ben Thornton
WuLi Software
