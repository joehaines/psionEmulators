14. Readme.txt
Company Accounting 2.8.2 for the Psion Series 5, 5mx, Revo etc (C) Pythia
Information & PocketIQ 1997-2000
------------------------------------------------------------------------------
------

Released as shareware.

Organizes your company's financial elements and generates language
independent invoices, rough cash-books and annual accounts.


Installation:
------------

OBS! If you are upgrading from a previous version please follow the upgrade
section.


Installation from SIS file:

1) Execute the Company.sis file on your pc or Psion.

2) Choose C or D drive for the installation (make sure that there is enought
disk space).

3) The program will install itself.


Installation from ZIP file:

1) Make sure the \System folder is shown on your Series 5 system screen. If
it is not, enable it in the 'Preferences' dialogue

2) Using PsiWin copy the files PYTHIA.OPO, PIQInfo.OPO, PIQInfo.RSC and
PIQInfo.MBM to the folder \system\opl (you might have to create the folder)

3) Copy sysram1.opx, systinfo.opx to the folder \system\opx (you might have
to create the folder)

4) Go into \System\Apps on C: or D: and make a new folder called "Company"

5) Using PsiWin, copy all the remaining files to the new \System\Apps\Company
folder

6) A new icon will appear on your Extras bar


Problems
--------

1. Read only files

Some times files become read only while being installed. None of the files
should be read only. If you get any errors like the program is unable to
write information to your Psion go through the program files to see if some
of them are read only.

In the system-view you can open the file property window by selecting
(highlighting) a file and pressing Ctrl-P. In the property window you can see
and set/clear the read only checkbox. 

2. Copying between C and D

Be very careful when you move or copy applications between the C and D
drives. It seems that the Psion is not correctly updated when applicaions are
copied from C to D or vice versa.

Always delete the application directory from one drive before reinstalling on
the other drive.


Upgrade:
--------

When upgrading from a previous version of Invoice Manager do not overwrite
the 
file "Invoice.dbf". This file contains all your registered invoice
information.

When upgrading from a version 1.4 or lower the "Invoice.dbf" file will be
upgraded to version 2.0 making it unusable with previous versions of the
program.

To upgrade the Invoice.dbf file from version 2.0 to version 2.2 please
execute the IMUPG22.OPO file. You can place it anywhere on your Psion and
delete it after use.

To upgrade the Invoice.dbf file from version 2.2 to version 2.3 please
execute the IMUPG23.OPO file. You can place it anywhere on your Psion and
delete it after use.

To upgrade from Invoice Manager 2.4 to Company Accounting 2.5 just install
Company Accounting. The first time your run the program it will copy the
necessary information from your Invoice Manager installation. After that you
can delete the \system\apps\invoice folder.


Usage:
------

1) Press Ctrl+Shift+H to see help file.

2) Download the detailed "Company Accounting On The Run" document from the
PocketIQ home page (http://www.PocketIQ.com).


Known Bugs:
-----------



Made by:
--------

Marc de Oliveira 
for Pythia Information & PocketIQ

Web: http://www.PocketIQ.com
E-mail: Pythia@PocketIQ.com


Registering:
------------

Registering the program will give you the following benefits:

 1) The register window will not pop up on startup
 2) You get free support via e-mail (Pythia@PocketIQ.com)
 3) Your suggestions to new functionalities will be implemented (as far as
resources permits it)
 4) The screen and generated files will not contain the "unregistered"
message.

You can register using cheque or credit card. See details on registering on
the PocketIQ home page at:

  http://www.PocketIQ.com


After receiving your registering fee I will mail you a Key that you can use
to register the program.


History:
--------

1997.11.30: Version 1.0

1997.12.03: Version 1.1
  - Memory leak removed.

1997.12.09: Version 1.2
 - "Invalid Argument" error when editing customer list fixed.

1997.12.10: Version 1.2.1
 - Error while registering fixed.

1997.12.11: Version 1.2.1 (release 2)
 - Problem with install on D: drive was only in my imagination...

1997.12.11: Version 1.3
 - pop-lists for customers and invoice lines are reprogrammed to handle more
items.
 - bugs on delete line and delete invoices fixed

1997.12.27: Version 1.4
 - Language independent invoice generation. The text of the generated invoice
is based on parameters controlled by you.
 - Parameter sets for English, American, German, French and Danish are
included and you can define your own sets as well.
 - No-VAT option. If you set the VAT to "0" you will not get two price
columns on the invoice (excl. VAT and incl. VAT) but just one (Price).

1997.12.28: Version 1.4.1
 - Currency is updated on the screen when changed on the invoice.
 - Last invoice is made current on start-up.

1997.12.30: Version 1.4.2
 - Selecting of non existing invoice numbers now prevented.
 - Bug: Too many invoice lines deleted, fixed.

1998.02.12: Version 1.4.3
 - Bug: Too few invoice lines deleted, fixed.

1998.03.30: Version 2.0
 - Language independent rough cash-books and annual accounts can be generated
in HTML
 - The scope of the program is extended to implement financial elements of
five different types instead of just invoices.
 - Account groups can be maintained
 - Financial elements can be related to account groups

1998.04.15: Version 2.0.1
 - Released as a Palmscape product.

1998.05.02: Version 2.0.2
 - Bugfix: Some fields could not handle being filled completely.

1998.05.14: Version 2.0.3
 - Palmscape splash screen shown during startup.

1998.06.07: Version 2.1
 - Browse through financial elements using arrow keys (up and down keys
toggles element type).
 - Create and Delete a Financial Element through the menu (also deleting
using the "Del" key).
 - Rearrangement of menu.
 - Financial Overview showing ballances.
 - When creating a new line for a financial element the default date is set
to the date of the financial element (instead of current date).
 - To avoid Psion bugs the sysram1.opx is now used instead of built in
database functions.

1998.06.16: Version 2.2
 - The filter window now lets you filter companies.
 - The company choice list is now sorted alphabetically.

1998.07.12: Version 2.2.1
 - Better handling when wrong version of ps.opo is installed.

1998.07.26: Version 2.2.2
 - Small bugs fixed.

1998.09.09: Version 2.2.3
 - New financial elements (Shift-Ctrl-N) are based on values from the current
financial element.
 - Bug when deleting financial elements fixed.

1998.09.20: Version 2.3
 - Fields E-mail, Fax, Alternative Phone and Note has been added to Companies.
 - Bug fixed concerning calculation of next invoice number.

1998.10.03: Version 2.3.1
 - The financial overview calculation (Ctrl-O) has been extended with three
new VAT related sums.
 - The company information on the top right corner of the screen is no longer
truncated by 5 pixels.
 - Fixed problem with saving e-mail and fax number of companies.
 - Other bugs fixed.

1998.10.18: Version 2.4
 - A new "Find" utilitiy added to search for strings in either financial
elements (Ctrl-F) or financial element lines (Shift-Ctrl-F).
 - Using pythia.opo version 1.2 makes it possible to include ',' in company
names, account groups and other fields that are selectable through pop-lists.

1998.12.03: Version 2.4.1
 - Default currency can now be setup.
 - Alignment of generated invoice has been improved to handle long customer
addresses (5 lines) correctly.
 - Fixed subscribt error when creating a new financial element from an empty
file.

1999.04.01: Version 2.5.1
 - Invoice Manager has been renamed to Company Accounting.
 - A new Graphical Overview shows the surplus generated in any given periode.
 - The current system date format is used instead of yyyy.mm.dd.
 - The Fields of the financial overview have been restructured (again).
 - Bug fix: Alternative Phone number of companies is now shown.
 - The Palmscape logo is no longer shown during startup.
 - Geofox enabled: Screen no longer reacts when the mouse passes over a field.
 - To conform with English law invoices now contain the amount of VAT on a
seperate line.
 - The Graphic field (for specifying the company logo) can now be used to
specify a HTML-string to be added to the top of the generated invoice.
 - An option to hide the Element Type from the main screen is added (Ctrl-D).
This is of interest to people who want to print invoices via screen dumps
instead of the generated HTML invoices.

1999.04.11: Version 2.5.2
 - The HTML generators now launches the generated invoices, cash-books and
annual account in the Message Suite Web Browser for viewing.
 - The generated HTML for cash-books and annual accounts now comes out in a
smaller font. This gives room for more lines per page.
 - The Graphical Overview now shows the end ballance of the chosen periode as
a number on the graph.
 - "Update" buttons have been changed to "Save" buttons to conform with the
Psion standard.
 - Menu, buttons etc have been changed to conform with the Symbian Style
Guide.

1999.06.29: Version 2.6
 - The application has been updated to work on both the Psion 5 and 5mx.

1999.08.05: Version 2.6.1
 - New list views for uncompleted elements
 - Jump to first and last using "Home" and "End"
 - "Closing..." message is now displayed while the program is ending.
 - Fixed bug: On 5mx the HTML file was not loaded the first time.
 - Fixed bug: Sometimes an error occurs when deleting financial element.

1999.08.15: Version 2.6.2
 - Annual account: The ballance at the start of the year is now automatically
added as an entry in the Liabilities section.
 - Annual account: More advanced handling of asset write off.

1999.09.21: Version 2.7
 - It is now possible to print Invoices directly from the Psion.
 - Bug fix: Installation bug is fixed.

1999.10.12: Version 2.7.1
 - Bug Fix: Generation of next invoice number has been improved to handle
un-cronological invoice numbers.

2000.03.25: Version 2.8
 - Graphics now adapt to the current screen size. This should work on all
EPOC platforms.
 - Released as shareware under PocketIQ.

2000.04.09: Version 2.8.1
 - Toolbar setting (show/hide) is stored when leaving the program.
 - It is now possible to search for a specific invoice number (Ctrl-F).
 - It is now possible to search for invoice line prices (Shift-Ctrl-F).
 - Maximum amount of each invoice line is changed to 999.999 instead of
100.000.
 - Error when printing without an MBM logo removed.
 - Error when trying to view the menu after deleting all financial elements
removed.
 - Spelling of "Balance" and "Period" is fixed.