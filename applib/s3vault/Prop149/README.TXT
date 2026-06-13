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

     
Registration
------------

The registration fee is A$29.95 for the shareware version + A$6.00 for postage and handling.
We send you a registration number by email and send you the printed manual
and a floppy with all of our shareware by mail.

Please use the ORDER.TXT. Post it via mail or email or FAX.

            Berthold Daum
            lichtbild p/l
            LPO Box 6011
            Cromer, Vic. 3193
            Australia

            CompuServe 100026,3365
            INTERNET: 100026.3365@compuserve.com

Alternatively, you can register via CompuServe (GO SWREG):

            Author:          Berthold Daum
            Program title:   PROPOC ONE V1.4 S3/S3a
            Registration ID: 3841
            Fee:             US$ 24.95 plus postage and handling

Entering the registration number:

Press MENU, / , ENTER
then enter first name and the registration number.
        

	 -----------------------------------------------------------
	 |   Warning!!!!!!!                                        |
	 |   This program has been "crippled".                     |
	 |   When used without a registration number the following |
	 |   restrictions apply:                                   |
	 |   1. Number of activities in one file restricted to 15  |
	 |   2. Number of milestones in one file restricted to 4   |
	 -----------------------------------------------------------



ProPoc ONE Version 1.49 - Release Notes
===================================

Note:

  With the release of ProPoc Version 2 we have re-released ProPoc V1.49
  as ProPoc ONE as a entry level product at a lower price.
  We hope that this offer is attractive to customers who do not need the
  full power of ProPoc V2 or are restricted in disk space.

  ProPoc ONE is frozen at version 1.49 and is sold AS IS. 
  Ongoing development is based on ProPoc V2, so new features will
  only appear in V2.

  You can upgrade from ProPoc ONE to ProPoc 2 at any time.

    Upgrade fees:  ProPoc ONE to ProPoc 2 LITE   A$40.00
                   ProPoc ONE to ProPoc 2 PROF   A$80.00


Program Description
-------------------

ProPoc can handle multiple projects, activities, milestones, and resources.

Activities and milestones can depend on the completion of other
activities. Activities can share resources, even between
different projects.

ProPoc determines the StartDate and EndDate of each activity,
the ActualDate of each Milestone, the number of slack days,
and if a activity or milestone is critical.

The results are displayed in graphical or table form on screen or
can be printed in ASCII- or WORD-form (tables).

Platforms
---------

The program runs on PSION S3 and S3a, and Acorn PocketBook I & II
in native mode.


Requirements
------------

The disk space requirements are 71-100k depending
on the options.


When active, the system requires a minimum of about 33k main memory.
Some functions can require considerable more memory.

We recommend to install the system on flash SSD, 
the PROJ folder (data drive) on the internal disk or on RAM SSD.

Compatibility
-------------

ProPoc files from earlier versions are converted to V1.4 format.
ProPoc V1.4 files can be read with ProPoc V2.


New features in Version V1.4
----------------------------

- National holiday calendars added

- Lead and lag days for activities and milestones

- Budgets for activities and projects

- cumulated resource schedule
  Printer: table form
  Screen graphic mode: graphical form, use CNTRL-cursor keys to scroll.
  Screen table mode: table form

- TAB/Shift-TAB switches resources in resource schedule.

- Up to 15 resources (instead of 7)


- S3a support:
	- Support of big screen and all 13 system fonts
	- Zoom in both directions
	- Support of diamond key for display mode switching
	- Show and hide links from menu
	- Currency symbol for new project files taken from systems setting

- Navigation in graphical mode: To move from critical activity to critical
  activity switch CAPS on. Don't forget to switch it off thereafter.
- Reorder indicator: If automatic reorder is switched off, a little indicator (+/-)
  in the top right corner of the screen (bottom right corner in table mode)
  will indicate when a reorder is required.
- Flashing cursor
- Table mode: Milestones are now underlined, critical items are bulleted
- Progress report: In addition to "Open,delayed,closed" the status
  of an activity can be set to 10%,20%,30%,...,90% completion,
  if all prerequisites are closed.
  This will be reflected in the graphical display.
  The project statistics will show the completion value for the whole project.
- Diagram-WordFiles can now be printed in landscape orientation (provided your printer can)
- Show function does not longer interrupt link mode
- PSION-Shift-Cursor up/down in table mode leads to project begin/end
 

                ProPoc ONE Description
                ======================

This short description does not intend to replace the printed manual that
is shipped on registration.


Explanation of terms:

File:
-----
        Each file can contain:
                7 projects
                224 activities
                56 milestones
                7 resources
                1 set of preferences(alarm, mode, holidays)

Files are totally independent from each other.
It is possible to copy activities and milestones from
one file to another by using the Copy function, closing
the first file, opening the second file, and using the
Insert function.

Project:
--------

        Name: 
                a unique name
        Description: 
                up to 255 characters of text
        Priority: 1 for high, 7 for low
                Priorities are unique! If you
                specify for instance priority 3
                the other projects in the file are
                pushed upwards or downwards.
                High priorities wins in resource conflicts.
        Start Date:
                Official project start date.
        Alarm advance days:
                How many days before the event you want
                to have the alarm
        Alarm for:
                Type of events generating alarms
        
        End date:
                Computed end date for project.
        Total duration:
                Difference between end date and start date
        Critical:
                List of critical activities
        Hot milestones:
                List of milestones where the actual date is
                higher or equal the planned date.
        Total costs:
                Accumulated fix costs and resource costs.
        Budget:
        		Planned total costs for project.

        		Two figures will be shown in the project
        		statistics:
        			1. Budget as entered for project
        			2. Budget computed from activity budgets.

Item:
-----
        Can be both an activity or a milestone.

        Project:
                to which project it belongs
        Name:
                unique name
        Description: 
                up to 255 characters of text
        Lead:
        		A number of workdays before the
        		activity actually begins or the
        		milestone is actually due.
        		No resources are allocated during the
        		lead time.
        		A negative lead value can be used
        		to overlap depending activities.
        Earliest date:
                Activities may depend on external
                influences, e.g. suppliers. This may
                define an earliest date for activity
                start.
        Duration:
                Duration of Activity in days.
        Lag:
        		A number of workdays after the
        		activity by which depending activities
        		are delayed.
        		No resources are allocated during the
        		lag time.
        		A negative lag value can be used
        		to overlap depending activities.
        Usage of:
                Percentage (0-100) of how much
                the resource is used. Most important
                resource: Yourself. If some activities
                must be done in business hours, create a
                resource business hours - or split yourself
                into a business you and a home you.
        Fix Costs:
                Setup costs for activity.
        Budget:
        		Planned total costs for activity.

        Planned date:
                Planned date for milestone. If the
                actual date exceeds the planned date,
                the activities causing this will go
                critical and the milestone will go hot.

        Links:
                List of activities the current activity
                or milestone is depending on.

        Start date:
                Computed start date for an activity.
                Activites in other projects with higher
                priority can influence the start date
                when they compete for the same resources.

        End date:
                Start date + duration

        Float days:
                Number of days, the activity is allowed
                to float.
                Activites in other projects with higher
                and lower priority can influence the float days
                when they compete for the same resources.
                An activity with null float days is called
                critical.

        Actual date:
                Computed actual date for a milestone

Links:
------
        Milestones and Activities can depend on other activities
        in the same project. To define such a link, use the 
        Link function. Then navigate to the desired activity
        and press Enter. ESC will cancel the link mode.
        To remove links, use the Unlink function.


Associated documents:
---------------------

    Items (activities and milestones) can have associated WORD documents. These
    can be quite complex documents containing very detailed descriptions.

    When you press PSION-A or Shift-ENTER, the WORD processor is activated
    and the associated document is opened (or created if it did not exist).
    Then you edit the document in the usual way (you may even print it). When
    you leave the WORD processor with PSION-X you are back in ProPoc.
    You can delete an associated document with PSION-H out of ProPoc.
    Associated documents are deleted, too, when the item or the whole project
    is deleted (PSION-D or PSION-V).
                
    Associated documents are stored under the following pathname:

             project_folder\project_file\itmname.WRD

    for instance, an associated document for item REGIST in project PROPOC
    would be stored in

                        LOC::M:\PROJ\PROPOC\REGIST.WRD.

    Restrictions:

    The hardest restriction is memory. Each WORD document uses at
    least 700 byte, so 20 documents would use a minimum of 14k. So use
    associated documents sparingly. Use the item description entry for 
    short descriptions.

    The ProPoc print functions will not print associated documents.
    Associated documents must be printed out of the WORD processor.



              
Preferences:
============
        
        The preferences are file specific and can be
        modified with the Preferences function.



Mode
----

        Reorder:
                For small projects choose "automatic".
                For big projects it is better to use "manual"
                and to call the Reorder function from time
                to time.
        Font:
                Different screen fonts to be selected
                small/normal/big (S3), 13 system fonts (S3a)
        Statuswindow:
                on/off (S3), off/big/small (S3a)
        Display:
                normal (1 day = 1 character)
                condensed (1 day = 2 pixels)
                table (spreadsheet like mode)
        Show Links:
                Yes/no
                When set to "yes", links are shown as a line
                from the end of a requisite activity to the
                beginning of the linked item. If one of both
                is not contained in the current screen, the link
                as only shown as a short dotted line.
        Show Grid:
        		Yes/no
        		Shows gridlines at weekstart when set to "yes"

    Holidays
    --------

        By default the system workdays/weekends are used
        and the custom holidays are disabled.
        
        You may, however, change weekend and custom public holidays.


    Resources
    ---------

        Resources are created and edited with the preference
        function.

        When no resources are defined the project planning
        process is straight forward.
        However, when resources are defined, the planning
        becomes a tricky optimisation process. We use a
        heuristic strategy (otherwise computation times
        would be enormous). This strategy may sometimes
        not lead to the shortest possible project duration.

        You can, however, use "Earliest date", dummy links,
        dummy activities, to make manual adjustments.

        You can print or display a resource schedule for the current
        project or for all projects with PSION-Z.

        Name:               Unique resource name

        Cost per workday:   Resource costs

        rated:              - full time (100% during usage)
                            - shared in project (100% but shared between activities)
                            - shared overall (100% but shared between activities)
                            - part time (as defined in Resource usage)

                            For further details see manual.

    Public holiday file:
    --------------------
        Use this function to edit the global holiday file.
        You may disable or enable a holiday, set it to "Custom"
        or choose "Modify" to edit it.

        A holiday can be based on EasterSunday in which case a delay
        can be entered (e.g. -2 for Good Friday).
        Otherwise you simply specify a base date which can be
        modified. Example:
            Base date:      31Oct
            Modification:   Sat
            Direction:      before
            Result:         the last Saturday in October.

        Holidays set to "Always" are enforced through all project files.
        Holidays set to "Never" are disabled for all project files.
        Holidays set to "Custom" depend on the Calendar setting
        in each project file.

                
Display:
========
    The screen shows a window to the current project.
    Switch projects with PSION-T or the TAB key. 
    Switch the Statuswindow on or off with CONTROL-MENU.
    Switch the Display mode (normal/condensed/table) with CONTROL-MENU.
    If an item has an associated document the cursor is displayed
    as a rectangle, otherwise as a triangle.
    
    Graphic mode (normal/condensened):
    ----------------------------------
    Navigate along the time axis with the forward/backward cursor keys.
    The PSION key accelerates.

    Little arrows in the bottom left corner indicate if there
    are more activity lines. 
    Scroll with the upward/downward cursor keys.

    The project name is found in the botton right corner of the main
    window.
    Activities are displayed as horizontal solid 1-pixel lines, if critical as
    solid 2-pixel lines. The name of the activity is found below the line.

    Milestones are displayed as vertical dotted 1-pixel lines, if critical
    as solid 1-pixel lines, if overdue as solid 2-pixel lines. In this case
    a dotted line to the left at the bottom of the vertical line indicates
    the delay. The name of the milestone is found to the right at the bottom
    of the vertical line.

    A calendar is displayed at the bottom of the screen. Saturdays and
    Sundays are marked with a dash, Week start is marked with a dot. 
    The date displayed above refers to that date.
    Holidays (weekends and public holidays) have a dash at the very 
    bottom of the screen. Activities appear dotted in holiday areas.

    During link mode a line is drawn from the origin item to the target item.

    If the cursor is set on an activity you can use the <> keys
    to modify the activity duration.

    Table mode 
    ----------
    Navigate along the time axis with the up/down cursor keys.
    Scroll with the Left/right cursor keys.
    The PSION key accelerates.
    
    If a value appears in a lupe (extra window) it can be modified.
    
    Numeric key:    Add digit
    DEL key:        Remove digit
    ESC key:        Restore old value
    Alpha key:      Select value
    <> keys:        Decrease/increase value or rotate values




Troubleshooting
---------------

Alarms are not ringing or are ringing at the wrong time:

	Alarms are only updated when ProPoc is exitted,
	or when a new project file is opened. Please exit
	ProPoc and restart.

Removing all alarms:

	We recommend not to delete file ALRM.$$$. This would
	also delete alarms set with other ...Poc applications such
	as HyperPoc. ProPoc would also recreate the file in the
	next session.
	Instead, edit each project (PSION-M) and set "Alarms for"
	to "none". Then exit ProPoc. ProPoc will remove all
	alarm entries from ALRM.$$$.
	

Acknowledgements
----------------
Thanks to Gary Barr, Frederic  Caussarieu, Lonnie Dworkin, Tony Jeffrey, Wolfgang Schmid
for help and creative input.





