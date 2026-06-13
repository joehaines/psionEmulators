+------------------------------------------------+
| THIS VERSION IS FOR THE WINS EMULATOR ONLY     |
| DO NOT INSTALL ON A PSION OR OTHER EPOC DEVICE |
+------------------------------------------------+

PsiDat 1.20

1. To install, copy the PsiDatW.SIS file over to \EPOC32\WINS\C folder, run the WINS emulator, locate the file in the root directory of the system screen, and double-click it.
2. Install PsiDatW.SIS before installing any PsiDat-<Language>.SIS files.
3. WebCol.SIS installs a Websafe216 palette tool, if desired.

v1.20 - CHANGES SINCE 1.19
Modified the HTML option dialogs and added a simple preview option (the '?' button). This allows the colour scheme to be seen but will not show special fonts and features such as background images.
Added <FONT> tag option to HTML options so that user can specify the font size and other attributes.
Added extra HTML colour options. (Note for translators: there are significant changes to the category 20 resources in this version)
Modified the HTML defaults so that they use the websafe 216 palette.
Produced a standalone tool 'WebCol' for colour users to assist with defining HTML colours.
Added two more default colour schemes and changed the call method so that it uses a choice field plus one button instead of a button for each scheme. Also added an option to copy the OPL code to generate the current scheme to the clipboard.
Modified HTML export so that soft line breaks are handled correctly in normal text fields.
Added <DT1>, <DT2>, & <DT3> field modifiers for doublefloat fields. These display a value in seconds as a datetime, date, or time component string respectively.
Added <HNM> field modifier. This is used to seed the filename dialog for HTML single record output based on the value of this field. Also made it easier to select single or multiple (tabular) HTML output by adding a separate menu cascade item and keyboard shortcuts.
Enabled a user-routine to alter the default HTML export filename just before calling the dialog. A user-defined procedure is called as follows:-
userhtmlname$:(<filename>)
<filename> is the PsiDat-generated default. If this procedure exists in a user's custom OPL routine then it should return an alternative filename or the <filename> parameter originally passed. This allows more sophisticated naming rules to be generated and always runs after the routine used by the <HNM> field modifier.
Improved the HTML export dialog so that the linkback text and URL are retained by default until the current table is changed.
Improved the quality and compatibility of the HTML output - especially with Mozilla and other compliant browsers.
Added a recovery routine to cope with tables that have damaged indexes (a rare occurence but worth it)

v1.19 - CHANGES SINCE 1.18
Fixed minor bug in field properties routine that prevented the correct field number being shown.
Improved the datetime accuracy; it's now possible to set and obtain the number of microseconds and the calculation limits on very low and high dates have been removed.
Added a 'Filename' option to the text field 'Special Use' options. This enables a fileselector when the field is tapped, allowing a filename to be selected or the field to be blanked.
Resource file editors should note that resource item 181 is now deprecated and has been replaced by a new item 457.
Fixed bug in listview that could cause the Data.OPX to panic if a new record has just been created in a table with an index.
Improved the simple search facility so that the user can enter their own wildcards.
Improved the sound-playing facility so that a sound can be cancelled by pressing a key.
Improved the table lookups and split list view so that you can jump to a position by pressing the first letter of the required entry.
Fixed minor bug in pen routine that could cause a subscript error if a pen tap occurred above the first field.
Tidied up some problems with calculations, searches and SQL queries (including auto SQL filters) involving strings that include single and double quotes.
Improved the memory handling when launching embedded files.
Improved the handling of custom OPL source code when stored in the 3rd record of zPsiDatDefaults. When this file is launched, modified and translated, the option to replace it will update the .opo executable in record 2 as well as the changed source in record 3.
Improved the Text export routine so that it can included LongText8 (Memo) fields. There is an additional option hidden away in the Options | General | > dialog called 'EPOC *.TXT'. When this is ticked, exported text will use &06 for lineends instead of &0D&0A (crlf) combinations. This makes it easier to export text for the standard Data application.

v1.18 - CHANGES SINCE 1.17
Improved the predefined SQL Query selector so that it uses a lookup-style window instead of a dialog box. The picklist button has been changed to the tab key so that it is easier to call up the picklist.
Added a split view mode. The autowidth list view is still available using shift-V but the far left screen pen action and tab key now call up a modal split-screen view. While navigating the list, the left and right arrow keys alter the field displayed. The favoured column is remembered while the table is open and a default can be set using the <SPV> field modifier. During split view, the toolbar buttons and position bar are disabled; any pen event outside the list or record scrollbar will return PsiDat to the normal card view.
Added a feature so that a user-defined pattern background can be used instead of a solid colour - copy an EPOC picture named 'background.mbm' into the PsiDat application directory and it will be loaded instead of any colour selection if the 'Use Wallpaper' option is set. On greyscale machines, a 16 colour background tile will automatically switch PsiDat to 16 colour mode on startup.
Added url:() and img:() string calculations to aid web page developments.
Improved the handling of LongText8 fields when in editmode so that the user does not attempt editing without switching to the memo dialog.
Added options for setting the preferred list view width and maximum start position for the field editboxes.
Improved the 'N'ew Record keypress action so that a <NOW> DateTime or Lookup/Help with <CD> modifier escapes the dialog.
Improved the lat/long fields so that if RealMaps is not running, PsiDat will attempt to obtain the NMEA data directly (at 4800 baud) instead of by polling the RM.SIG file. The <NOW> field modifier may also be used with lat/long fields to force a RealMaps or GPS read when creating a new record.
Fixed a minor display bug that could occur after a table merge.
Enabled user-defined helpfile: If a standard Data file 'MyFile.HLP' exists in the same folder as a PsiDat database called 'MyFile' then this can be accessed as well as the main helpfile.
Added the field number to the field properties dialog title.
Altered the binary file embedding dialog so that it initiates with the same directory as the database file and subsequently remembers the selected directory.

v1.17 - CHANGES SINCE 1.16
Fixed minor bug that prevented the tablelist and indexlist being updated after importing text and dbase files.
Tidied up the behaviour when a user attempts to open an EPOC data file that has no records.
Modified the embedded file launch routine so that it is easier for the system to recognise an HTML file and launch it in the default browser.
Some security has been added so that the user is warned and given the option not to run a custom OPL routine the first time it is run on their machine.
Fixed problem that prevented v1.16 running properly on machines with codepage 1250 localisation patch.
Altered the dialog boxes slightly when running on the Osaris so that they fit the screen better.
Made it possible to force a completely blank new record (ie. ignore all <CD> and autofield modifiers) by typing Shift-N.
The trailing space character on DateTime fields set to display 'Date only' has been removed.
The autofiltering routine has been improved so that it works properly with Date fields on systems that use the US or Japanese date ordering (provided that the PsiDat settings match those in the control panel).
The default forcing of zero times on Date fields has been restricted to those fields that use the <SQL> field modifier.
A new function ParseSQL$:(string$) has been added. This is used by the SQL search dialog and allows the user to enter the following:-
#TODAY#(to indicate today's date)
#TOMOR#(to indicate tomorrow's date)
These can be used together to filter records before, after, or on today's date, eg:-
WHERE Date>=#TODAY# AND Date<#TOMOR#returns only records with today's Date
WHERE Date<#TODAY#returns only records before today
WHERE Date<#TOMOR#returns only records up until midnight today
WHERE Date=#TODAY#returns only records with the DateTime value (Today at 00:00:00am)
NB: Once the filter has been searched or applied, the SQL dialog displays the parsed SQL so if you want to save it for the picklist, this should be done before using it.
Added option to use a Word file instead of a jotter file as the 'Rich Text' type for LongBinary fields (see 2nd page of the 'General Options' dialog).
Improved the printing setup so that PsiDat should be able to remember the selected media.
Added option to view/manually change the SQL statement for the current table OPEN  statement. This is shown on 2nd page of the 'General Options' dialog. This can be used by advanced users to perform special sort views (eg. FOLDED SELECT to sort without case-sensitivity). However, if you alter this manually and get it wrong, PsiDat will crash - though it's very unlikely any data loss will occur because the table is closed normally before attempting to open the view.
Enabled a basic print function that does not require a report layout to be prepared. This allows longtext8 (Memo) and MBM pictures embedded in longbinary fields to be printed as a continuous list.

v1.16 - CHANGES SINCE 1.15
Fixed bug in dBase IV export dialog that prevented escape if the filename was invalid (ie. too long for dBase IV).
HTML 'Back' default is now included in the resource file so that it can be localised.
The filepath used for HTML exports is now maintained during a session.
Buttons used in 'Create Table' dialog have been changed to make it quicker to use (ie. the tab and enter keys are used to move up and down the fields)
Improved the table and index selection by using an ordered lookup instead of a dialog box.
Added additional error-checking so that ordered lookups revert to unsorted ones when no suitable index exists on pre-ER5 machines.
Improved reporting of record numbers - PsiDat can now report record numbers in excess of 65535 provided that the table contains at least one text field.
Modified the table closure routine so that compaction of large (>500KB) databases is optional.
Speeded up multiple record deletion for large databases.
Made it possible to add text 'on-the-fly' to user-defined lookup and help tables in a similar fashion to the built-in zPsiDatLookups ones (limited to the lookup field only but can be useful)
Lookup fields can now be manually cleared even if an empty string is not included in the lookup table.
Improved the text cursor.
Added a text calculation function TNStr:() that can be used to insert tab and new paragraph characters into calculated text fields.
eg. TNStr:([1]^t[2]^p[3]^t[4]) would concatenate fields 1-4 so that '^t' would be replaced by a tab and '^p' would be replaced by a new paragraph character (ASCII 6).
Provided a simple way of copying any text or numeric field to the clipboard by tapping it with the Ctrl key held down.
Modified the new record routine so that the user is instantly in edit mode (unless the record is created from the toolbar since this may not always be desirable).
Removed limit on number of 'copy-down' (<CD>) field modifiers.
Added 'rollback' feature and improved use of transactions to improve memory usage on large databases. If rollback is available, it is indicated by a light (green) band behind the position indicator bar. Shift-Esc will rollback when available. NB: Use of First or Last record will commit the current transaction.
Additional comments :-
Rollback is not available if the table has an index.
Explanation: if records are added, deleted, or revised so that their position changes, then attempting to rollback corrupts the table. (Interestingly, deleting all the indexes for the table seems to recover the database but DbRecover: fails to do the same - this looked too dangerous to try and work around so Shift-Esc on indexed tables just commits the transaction).
On colour machines, incomplete transactions on indexed tables are indicated by a pale red band.

COMMENTS/SUGGESTIONS TO:-

psidat@millican.info