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
         |               WARNING              |
         | The shareware version has been     |
         | "crippled". As long a unregistered,|
         | "nag" screens will appear now  and |
         | then as a reminder to register.    |
         --------------------------------------


Registration of Shareware
=========================


Lite version (S3 and S3a)
------------

The registration fee is A$39.95 + A$6.00 for postage and handling.
We can send you a registration number by email, and send you the printed manual
and a floppy with all of our shareware by mail.

Please use the form  ORDER.TXT. Post it via mail, email or FAX to


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


Alternatively, you can register via CompuServe (GO SWREG):

		Author: Berthold Daum
		Program Title: RepPoc V2.3 lite S3/S3A
		Program name: RepPoc.zip
		Registration ID: 3826
		Fee: US$ 35.00 plus postage and handling

Professional version (S3a only)
--------------------

The fee for the professional version is consists of

	1. Fee for lite version A$ 39.95 + A$6.00 postage and handling
	2. Upgrade fee: A$ 19.95
	
Alternatively, you can register via CompuServe (GO SWREG):


		Author: Berthold Daum
		Program title:	RepPoc V2.3 prof S3a - Upgrade
		Program name: RepPoP.zip
		Registration ID: 6984
		Fee: US$ 15.00 plus postage and handling
		(You must be registered to the LITE version)


Entering the registration number
--------------------------------

   Press MENU, / , ENTER, ENTER,
   then enter first name and the registration number.

   Note: The entered name must be part of the machines owner name.
         The registration number must be issued on the entered name.




Professional version for S3a
----------------------------


The professional version has additional functions:

  - Creating spreadsheets from DATA files
    and updating existing spreadsheets with DATA file contents.

	
NEW!!! Recommendation program
-----------------------------

We distribute our programs directly to offer you the lowest prices as
possible. Direct distribution depends very much on mouth-to-mouth recommendation.
We would like to acknowledge this fact by honouring the recommenation of our
products. If you are a registered user of any of our products, and another
user quotes your name when he registers, we will send you a floppy disc with
the newest versions of the products you had registered absolutely free !



				RepPoc Version 2.3 - Release Notes
				===================================

Platforms
---------

The program runs on PSION S3 and S3a and Acorn PocketBook I and II
in native mode.
On Acorn PocketBook I an extra OPL translator is required to create
custom text functions.

RepPoc can be run on the S3a-emulator and supports screen sizes up to 640x400.
( Set SERVER_PARAMS -X640 -Y400 -D in HCSERVER.PAR for the big screen)


Requirements
------------

Please see the installation routine for details.
For a full installation about 50k of disk space are required.

We recommend to install the system on flash SSD, the REPORT folder
on the internal disk.

New in version 2.311
--------------------
Explicite helptext added in LITE version.

New in version 2.310
--------------------
    - Support for F11-Data added (Preferences).

New in version 2.38
-------------------
    -  PROF version
       The spreadsheet generator can now handle type declarations:
       Prefix field names within the chevrons with
         Fn::   fix point with n decimal place (0...9)
         Sn::   Scientific with n decimal places (0...9)
         Cn::	currency with n decimal places (0...9)
         Pn::   percent with n decimal places (0...9)
         Nn::   thousandscomma with n decimal places (0...9)
         A::   Default
         G::   Floating point
         H::   Hidden
         B::   Bargraph
         D::   Date (number interpreted as days starting at 31.12.1899)
               accepts also date strings in system date format
         T::   Time (0.0...0.5...1.0 starting at midnight as in spreadsheet)
               accepts also time strings in system time format

       All of the above declarations will cause the generation of
       cells with numeric content. Columns without type declarations
       or cells with nonnumeric content are generated as strings.
       If the number n of decimal places is omitted, the system
       setting for the currency will be used.

       Example:
          <<C2::price:>>  <<D::birthdate>>  (<<>> meaning chevrons)

    -  PROF and LITE version
       The following text additional text functions are now available
       (do not appear in RPFUNC)
       
         Fn::   fix point with n decimal place (0...9)
         Sn::   Scientific with n decimal places (0...9)
         Cn::	currency with n decimal places (0...9)
         Pn::   percent with n decimal places (0...9)
         Nn::   thousandscomma with n decimal places (0...9)
         G::   Floating point
         D::   Date (number interpreted as days starting at 31.12.1899)
         T::   Time (0.0...0.5...1.0 starting at midnight as in spreadsheet)

       Fields with nonnumeric content are generated as strings.
       If the number n of decimal places is omitted, the system
       setting for the currency will be used.

       Example: as above
         

New in version 2.36
-------------------

    -  Dial setup has been removed. Instead the system setup is used.

New in version 2.35
------------------

    -  German language version available in lite version, too.

    -  Dial procedure has been simplified.

    -  Automatic shutdown of DATA application if data file is found locked

    -  Spreadsheet functions now under an own title.

    -  New spreadsheet function includes now the update function.

    -  Revised setup procedure saves some flash memory when multiple applications
       are installed on one disk.


New functions in Version 2.3
----------------------------

	-  Break processing
	
	   A parameter in the SORT function allows to show only the records
	   where the SORT criterion has changed.
	   
	   A flag field informs text functions if sort criterium 1 or 2
	   have changed or will change (see RPFUNC.DOC).
	   Break processing relies on sorted selection sets.
	   
	   The following standard text functions are provided:

       SORTED::       Checks for sorted selection set.
                      If not sorted, report generation is aborted,
                      and sort function is started.
                      The same check is done by the following functions:

       NEW1::field    Print field only if sort criterium 1 has changed.
       BRK1::         Counter for changes of sort criterium 1.

       POS1::
       SUM1::field
       MAX1::field
       MIN1::field
       MEAN1::field
                      Similar to POS,SUM,MAX,MIN,MEAN, but the values
                      are reset when sort criterium 1 changes.
	   	
       NEW2::field    Print field only if sort criteria 1 or 2 have changed.
       BRK2::         Counter for changes of sort criterium 2.
                      Reset when sort criterium 1 changes.
	                  
       POS2::
       SUM2::field
       MAX2::field
       MIN2::field
       MEAN2::field
                      Similar to POS,SUM,MAX,MIN,MEAN, but the values
                      are reset when sort criteria 1 or 2 change.


	-  Conditional paragraphs.

       When the return string of a text function contains chr$(0)
       the text following that character until the end of the
       paragraph is skipped.
       When the return string of a text function contains chr$(12)
       a page break will be inserted after the current paragraph.
	   
       The following standard text functions are provided:

       IF::field       Print rest of paragraph if field content is >= "0"
       SKIPIF::field   Skip rest of paragraph if field content is >= "0"

       HDR0::          Print rest of paragraph if the first character of
	                   sort criterium 1 has changed.
       HDR1::          Print rest of paragraph if sort criterium 1 has changed.
       HDR2::          Print rest of paragraph if sort criteria 1 or 2 has changed.
       TRL0::          Print rest of paragraph if the first character of
	                   sort criterium 1 will change.
       TRL1::          Print rest of paragraph if sort criterium 1 will change.
       TRL2::          Print rest of paragraph if sort criteria 1 or 2 will change.

       NPAG0::         Insert page break after current paragraph if
	                   the first character of sort criterium 1 will change.
       NPAG1::         Insert page break after current paragraph if
	                   sort criterium 1 will change.
       NPAG2::         Insert page break after current paragraph if
	                   sort criterium 1 or 2 will change.

       CHAR1::         Returns the first character of sort criterion 1.
	   

	-  As set of global variables has been provided to store values
	   between the invocation of text functions. (see RPFUNC.DOC).

	-  Sort prompt can be forced.
	   This option can be set in Preferences or in the
	   Sort function itself. If set, the Sort function will
	   be invoked whenever a selection set becomes unsorted.

	-  Prompts for data file, search and sort criterium when a
	   file is opened can now be suppressed.
	   This option can be set in Preferences. 

	-  Easier modification of text functions.
	   RepPoc no longer locks RPFUNC.OPA, so this module
	   can be retranslated while RepPoc is active for easy testing.
	   Before loading the default text functions .:\app\reppoc\rpfunc.opa,
	   RepPoc will look into the \APP\ folders on all three drives.
	   If the module is found there, it will be taken from there
	   So, transfer of RPFUNC.OPA to .:\app\reppoc\rpfunc.opa
	   is no longer required during the testing stage.

	-  It is now possible to edit the data file with the DATA application
	   while RepPoc is active. If the data file has been changed, RepPoc
	   will restart with the new version of the data file when control
	   is passed back to RepPoc. In this case all records become unmarked.

	-  A special function (PSION-J) allows to invoke the DATA application
	   directly out of RepPoc.
	   The DATA application will be positioned to the current record. (S3a only)
	   
	-  Instead of using WORD for editing of forms and reports you can
	   use F11 if installed. You can select the word processor of your choice
	   via Preferences (PSION-Q). F11 makes typing easier and assist during
	   insertion of text functions into forms.

	-  The selection set size has been increased to 1024 records on S3a.

	-  The operators ==,<<>>,<<,>>,<==,>==  have been added for comparing
	   numeric values.

New function in Version 2.2
---------------------------

    -  It is now possible to process OPL-data files (ODB) in the same
       way as Database files. The ODB files can only consist of string fields.
       RepPoc will generate dummy field labels for each field.

    -  Dummy field labels are now displayed in grey colour (S3a only).
    
	-  New text function UC1:: converts the first character of a string
	   into upper case, the rest into lower case.

	-  New predefined form : ROLODEX to print addresses in the ROLODEX format.
	   
	-  New output modes for letter documents
	   - into one document: the traditional method as in Version 2.1
	   - manual: a separate document is created for each record.
	             The WORD application is invoked and you can modify
	             and print each letter separately. Leave the WORD application
	             with PSION-X, and WORD will be invoked again with the
	             next letter.
	   - automated: same as manual, except that printing and exitting
	             the WORD processor is done in an automated way.
	             Make sure, the correct printer was set up for the form sheet.
	             This mode is only available on S3a.
	             
	   Manual and automated mode are more flexible for creating letters. You
	   can use form sheets that contain WORD-header lines and WORD-foot lines.
	   Also the page numbering is done for each letter separately.
	   In addition these method do not build up huge output files.
	   
	   Do not connect the WORD printer driver to a printfile when you use these modi as
	   with each record a new printfile would overwrite the old printfile.
	   Instead, print to a real printer.

	-  Printing interruptable with ESC-key.

New functions in Version 2.1
----------------------------

	-  Group processing. One formsheet can now refer to multiple records from the data
	   file, e.g. «1.Name» «2.Name» «3.Name» ... «9.Name».
	   The maximum record number in a formsheet defines the group size.
	   The record number always comes first, before the name and before an optional
	   text function. RepPoc will read as many records are required to process
	   the form sheet, and will then continue with the next group of records.
	   This feature is especially helpful for processing multi-column layouts, e.g.
	   for labels. (See LABEL.WRD),
	-  Numeric functions can also be prefixed with a record number and will then
	   apply only the record with the specified number within the group.
	   Example: «1.SUM::Amount» «2.SUM::Amount» ... 
	   This feature can be used for column sums.
	   If not prefixed with a record number, the function will apply to every
	   record of a group.

New function in version 2.0
---------------------------
	
	-  New TextStyle TR for trailer paragraphs. Trailer paragraphs
	   are printed only once at the end of a list.

	-  Some text functions (like date::) can be used not only in Body text
	   but also in header paragraphs (HA,HL) and trailer paragraphs (TR).

	-  New numeric functions:
	   pos::              current position in list (no argument)
	   sum::              sum of the numeric value of that and the above fields.
	   max::              maximum of the numeric value of that and the above fields.
	   min::              minimum of the numeric value of that and the above fields.
	   mean::             mean of the numeric value of that and the above fields.

	   All these functions can be used in both body and trailer.
	   0 is assumed for nonnumeric fields.

	-  New form type:
	   PSION-C creates now optionally a form of type "report".
	   This form is similar to a list, but fields are separated by TABS
	   instead of commata, and a trailer is generated with sum:: functions
	   below each column.

	-  The name functions in RPFUNC have now a logic for titles:
	   given::  and intial::  
		remove now titles like Prof and Dr from the first name
	
	-  A new name function title:: returns any title strings, like "Prof.Dr."
	   in a name. If no title is found it return "Mrs./Mr.".
	   If the name string was empty it returns "Madam/Sir"

	-  Text function country:: can now also return the country name
	   on phone numbers with country code instead country name (+61-x-xxxxxx)

	-  New menu function PSION-E and PSION-D to edit or delete a form sheet.

	-  Assistant, helping to construct complex search and sort criteria.

	-  Field labels that contain blanks can now be used. They must be placed
		inside chevrons (« »). (The assistant will place any label inside chevrons.)

	-  professional version: Creation of PSION-S3/S3a spreadsheet files
		from DATA files, and updating of spreadsheet files with DATA contents.

	-  Native version for S3a supporting the big screen with ZOOM function.


Program Description
-------------------

RepPoc is a report writing and mail merging program based on
the PSION WORD and DATA formats.
It can be used to create lists, labels, letters, envelops from
selected database records. The print layout is controlled by forms
created with the WORD application.

To create a report you need:
	a database file                 (DAT\ folder)
	a form (WORD document)          (REPORT\FORMS\ folder)
	an output file (WORD document)  (WRD\ folder)
	a search criterion

The above file specifications and the search criterion are
stored in a report file (REPORT\ folder). This saves you
typing if you want to run the same report again and again.

When you start the report application, you are first prompted
with the name of the DATA file and the search criterion.
At this stage you may want to modify this information. All changes
will be kept in the report file selected.

You can also re-specify the search criterion using the PSION-F
function (or the TAB key). This changes will go into the selected
report file, too.

Limitations
-----------

	-  Only a selection set of 256 records (1024 on S3a) can be viewed or sorted,
	   but you can print more.
	-  Records with a total length of more than 1022 bytes are not processed.
	-  Fields with more than 255 characters are not processed.
	-  only the first 32 fields of a record can be processed.
	-  OPL data files (ODB) can be processed when they contain only string fields.


Searching
---------

The search criterion can be complex. It can consist of several
conditions.

'condition' has the following construction:

	fieldspec operator value

or

	fieldspec value

'fieldspec' has the following construction:


	label

or

	label[n]                n=1...9
				n identifies line number
				  for multiple line fields.
or

	txtfunc::label

or

	txtfunc::label[n]       n=1...9


'label' denotes the field of the DATA record, e.g. Name: or Work:
(Phone symbols are not written).

'txtfunc' denotes a valid text function (see below).

If a field has no label, it is addressed via a dummy label:

	field#nn

with "nn" being the field number (starting from 1 for the first field).


Valid operators are:
	=  <>   <   >   <=   >=		for alphanumeric comparisons (not case sensitive)
	== <<>> <<  >>  <==  >==    for numeric comparisons


If no operator is used, the condition is that the field denoted
by 'label' has to contain the value (not case sensitive).
	

Values are specified without quotes and can contain blanks. However,
they must not contain logical operators and paranthesis as separate
tokens.

Conditions can be combined with logical operators such as
	NOT   AND    OR    (       )

Example:

	work: germany and not ( notes: = no mail or field#5 berlin )

selects german business contacts but excludes the ones with the
remark "no mail" or where the second line of the address (field#5)
contains "berlin".

Search criterions treat multiple line fields (created with Shift-Enter)
as a whole.

Alternatively to a complex search criteria, you can enter the usual
search string, too. The string will be sought in any field of each record.
This is the only form of search criterion that can contain wild cards.


Only the records matching the search criterion are displayed. You
can scroll the records and from record to record in the same way
as in the DATA application. You cannot, however, update records.
You can use the dialling function, by pressing the dialling key.


Printing
--------

Despite of the name of this function, RepPoc does not output to a printer.
Instead it prints into a WORD document. This document can then be
opened with the word processor, modified and printed to any appropriate
printer.

However, this method can create a problem, as the output file can
become to large for the disk. If this happens, simply divide your
report into parts by modifiying the search criterium. Add, for 
instance,  Name: < m  for the first part and   Name: >= m for
the second part.

When the print function is started, you are prompted for the
form sheet and the output file. Changes made here will be stored
in the current report file.

After the output file was created, you have the possibility to invoke
the word processor immediately and print you file from there.
However, you may want to leave RepPoc first and initiate the printing
from a separate task if you run into memory problems.

For letters, two more print modes exist. Instead of creating one big
document, RepPoc can create a single document for each letter. It will
invoke the WordProcessor for each letter to print the document. On an
S3a this can be done in an automated manner.
Generating letters as individual documents has the advantage that headers
and footers appear with each letter and that the page number is correct.


Form sheet
----------

You need a form sheet for printing. RepPoc gives some help
to create new form sheets. Use PSION-C to create a new form sheet.
The new sheet will contain a title paragraph, a header paragraph
(in the case of lists), and a body paragraph.

The body paragraph will contain field specification of the current 
data file enclosed in ® ¯ (left chevron, right chevron).
The chevrons can be entered via CNTRL-174 and CNTRL-175
(or CNTRL-1 then "l", and CNTRL-1 then "r" )but it is
much more comfortable to generate a form sheet with the PSION-C function
and to delete the fields not needed.

Field specifications are given in the same way as for search criteria 
and can contain text functions and line numbers. In addition field speci-
fications can be prefixed with a record number for group processing.
The record number denotes the position of the record within the
record group processed with one formsheet.


The body paragraphs have slighty different layouts depending
on the form type (list, label, letter). For letters, e.g. the body
paragraph will start a new page.

After you have created a new form, you can edit immediately using
the PSION word processor. The word processor will be invoked automatically.
When you have done you changes, press PSION-X to return to RepPoc.
If you decide to modify after you have left RepPoc, move the highlight 
under the WORD icon, press TAB, navigate to the REPORT\FORMS folder, 
move the highlight to the new form and press ENTER.

Now you can rearrange the keywords, include some more text or tabs, etc.

Apart from the standard style sheets: HA,HB,BT,BL we have included
some new style sheets:

	HL      for list and report header
	LE      for list and report entry
	LB      for label
	LT      for letter
	PB      for page break (internal)
	TR      for report trailer

The style sheets HA, HL, and TR play an important role during report
generation: 
	Only the bodytext is repeated.
	The bodytext is defined as the part between the last HA or HL style 
	paragraph and the first TR style paragraph.
Empty paragraphs of style PB are inserted automatically at the beginning
of each repetition if a form contains at least one LT paragraph is found in
the bodytext.
So, letters start always with a new page.


Multiple line fields
--------------------

These fields were created in DATA using Shift/Enter. When displayed
all lines are displayed.
However, during searching or when printed, only the first line is 
used, unless specified otherwise. You may do so, by specifying

	Name:[n]                n = 1..9

for the n-th line of the Name: field.


Text functions
--------------

RepPoc provides some useful text functions. Functions
are specified in the following way:

	function::label

or

	function::label[n]

or if they have no argument

	function::


A full description of all text functions is contained in RPFUNC.DBF 
(PSION database file) as an online reference.

However, you may want to write some custom text function.
You do this by adding them to RPFUNC.OPL.
The programming guidelines are described in RPFUNC.DOC.

It is a good idea to document all changes in RPFUNC.DBF.


Standard forms
--------------

We have already included some standard forms:

PHONES.WRD              landscape format phone list

LETTER.WRD              standard letterhead

FAX.WRD                 FAX cover sheet

ENVELOP.WRD             DL1 envelop

LABEL.WRD               address labels

ROLODEX.WRD             ROLODEX format address cards

You may want to modify these forms according to your requirements
and the printer you use.


Sorting
-------

After pressing PSION-S (sort function) you can specify up to 2 sort fields
(full fieldspec possible including text functions) and the sort direction 
(ascending/descending).

Note, that not the whole file but only the selected records are sorted.
Although we have implemented QuickSort, it can take a while to sort depending
on the size of the selection set and how the records are preordered.

You may then view or print the records in sorted order.
You can interrupt the sort process by pressing any key.

You can reduce the selection set to only the records where the sort
criterion has changed.

A variety of text functions can react on changes in the sort criterion
and allows you to create sophisticated reports with breaks, subtotals
and totals.


Marking
-------

During viewing, you may mark the selected records be pressing PSION-M or Shift/Enter.
When you print, you have the option to print only the records marked.
The mark symbol is shown in the top right corner of the screen.


MS Word compatability
---------------------

When you create a new form sheet (PSION-C) you may select WordDataFile.
This will generate a list format form sheet, not containing any blanks
and replacing all special characters in field names in the header line with
underscore. Field merge entries are set between double quotes; so each later data
entry will be surrounded by double quotes to avoid problems with comma. 
The title entry is supressed.

You may then modify this form sheet to your reqirements using the WORD processor.
Then you use REPPOC to generate a list document from this form sheet.
Read the resulting document with the WORD processor and store it as ASCII text
file to your PC (Save as, text). You may do this directly when MCLINK is active
or first store it locally and transfer it later with MCLINK.

On the PC you use the resulting file as DATA file in the usual way:
With Word for Windows for instance, create a new document, then choose Print merge,
attach the file as DATA file. Then use the insert_merge_field button to
create field entries in the master document.

The advantage of this approach compared to direct mail generation from the PSION computer
is that you can include graphics like logos into your letterheads and envelopes.


Preferences
-----------

The preferences allow you to specify
- if the sort function shall be invoked every time
  the selection set becomes unsorted (especially when the file is opened).
  
- if you want to be prompted for data file, search criterion and sort criterion
  when the file is opened.

- if you want to use WORD as word processor or if you prefer to work
  using F11. F11 is a WORD enhancer distributed by us.


Spreadsheets
------------

Only in professional version.

First use the function "New spreadsheet" and define the maximum number of lines.

RepPoc generates a title line and a control record. You may want to alter the
title line or the control record. The control record is used to generate the values
later on.

You can also copy this new spreadsheet into other existing spreadsheets.

Then use the "Update spreadsheet" function to insert the values.

When the contents of the DATA file have changed, use the update function
again, to replace the old values with the new ones.

For more details about this function see the manual.


Troubleshooting
---------------

1. Line feed between entries is missing.

   Examine the form sheet. The last paragraph is probably not closed,
   i.e. when the cursor is moved to the end of the document, it will be
   at the end of the last line and not below the last line.
   Press ENTER to bring the cursor below the last line.

2. List headers or other header paragraphs repeat with the body text.

   You probably changed the format of these paragraphs. The WORD application
   has created a new style sheet (e.g. ZA) and assigned it to the paragraph.
   RepPoc cannot identify the paragraph as header paragraph because they have
   different names now.
	
   You can:
	
   a) reapply a Header Style sheet to the header paragraph (HA,HB,HL)
      and accomplish the necessary changes by modifying the settings
      of the header style sheet.
   b) create a short paragraph below the header paragraph and apply a header
      style sheet (HA,HB,HL) to it.

3. Conditional paragraphs don't appear at all.

   The paragraph was probable defined above a header paragraph. Move
   the paragraph to a position after the last header paragraph.







