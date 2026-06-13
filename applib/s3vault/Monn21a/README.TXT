                          Monnaie v.2.1a
                         ================
                     Shareware for Psion S3a
                   proposed by Guillaume Dupont
                     CompuServe ID 100740,1033
                 e-mail:100740.1033@compuserve.com


Introduction.
=============
Instant conversion of an amount expressed in any currency of a
list into all the other currencies. Handles up to 62 currencies
simultaneously. Easy update of exchange rates,...

Keywords: CURRENCY CONVERT CONVERSION FINANCE S3A


Content.
========
The monnaie.zip file contains the following:
- monnaie.wrd is this documentation under Psion word format
- readme.txt is the same under text format
- changes.txt is a summary of changes from previous version
- monnaie.opa is the application
- monnaie.upd is a parameter file
- monnaie.rat is a data file
- monnaie.rsc is a help file
- 3user.sco and monnaie.sco are scripts for the download of
  closing rates from PA News Financial (CompuServe, GO CIS:CITYNEWS),
  and for the download of some exchange rates from Global Report
  (CompuServe, GO CIS:GLOREP). Examples of the import text files
  which can be produced are attached.
- 3user.scr and monnaie.scr are the corresponding source scripts.
- closing.txt is a sample text file for import of exchange rates
  to Monnaie (from PA News Financial), produced with monnaie.sco.
- glo_rep.txt is a sample text file for import of exchange rates
  to Monnaie (from Global Report), produced with monnaie.sco.


Installation.
=============
To install "Monnaie":
- copy monnaie.opa to any \app\ directory
- copy monnaie.rat, monnaie.upd and monnaie.rsc to an \opd\
  directory on any disk (not a flash SSD). The 3 files must be
  in the same \opd\ directory.
- install monnaie like any other Psion application
  (press Psion-I,..).

To install version 2.1a if you had already installed a version of
Monnaie older than 2.0:
- delete existing monnaie.opo 
- copy monnaie.opa to any \app\ directory
- replace the existing monnaie.rat, monnaie.upd and monnaie.rsc
  with the newly supplied files with the same names

To install version 2.1a if you had already installed version 2.0
or later, simply replace monnaie.opa and monnaie.rsc with the newly
supplied files.

In all cases the three files monnaie.upd, monnaie.rat and
monnaie.rsc must be located in the same \opd\ directory.


Operation.
==========
Press "Enter" when the "Monnaie" icon is highlighted in the system
screen. Once Monnaie is started, the <Help> key gives access to a
fairly comprehensive instruction menu. It is strongly recommended,
that you read the different help screens, if you want to get the
full benefit of Monnaie. Monnaie.rsc is the help file. You can delete
it once you feel familiar enough with the programme, if you need to
save disk space. The <Help> key will then become ineffective. 

The menu options themselves, which are displayed when the <Menu>
key is pressed, are self explanatory. In addition to these commands,
and their corresponding Psion-x short cuts, the following commands
are also available:
- <Shift> + <Tab> to select all the countries of a region (opens
  a selection window)
- <Shift> + letter to select the country with name starting with
  letter
- <Shift> + <Esc> to reselect all countries
- <Tab> to show the current format for display and input of exchange
  rates, and <Tab> again to change it
- <Diamond> to show the dates of last update of the exchange rates,
  and <Diamon> again to show the currency strings.


Registration.
=============
Monnaie is Shareware. You can use it freely during 14 days. After
this period, if you find it useful, please register. Otherwise
remove it from your computer. Registration fee is US$20.00.

On CompuServe, you can register via SWREG (GO SWREG. Registration
ID: 8418).

Alternatively you can send US$20, £15, DM30, CHF25, or FF100 in
cash by mail. Send bank notes, no checks. Do not forget to send
your e-mail address with payment. Note that sending cash by mail
is at sender's risk. My address is:

Guillaume Dupont
25, rue Pauline Borghese
92200 Neuilly sur Seine
France

Upgrade from a previous version is free.

Registered users receive a registration code by e-mail, which removes
nagging screens and unlocks:
- the file import function
- alternative formats for the input and display of exchange rates
- the "preferences" menu (regions, decimals, line keys).


Scripts.
========
The following scripts are supplied:
- 3user.sco
- monnaie.sco

Copy them to an \sco\ directory. They are translated scripts,
which will run under the ReadCIS application, in conjunction with
ThreeCIS. Monnai.sco is "switch driven" in the same way as ThreeCIS.

The corresponding *.scr files are also supplied. You can copy them
to an \scr\ directory, read them and modify them, if you wish, with
the script editor (see your 3Fax or 3Link reference book).

3user.sco is called by ThreeCIS, when the "user.on" switch is
detected (see ReadCIS documentation on switches). The only function
of 3user is to call monnaie.sco. If you have already written your
own 3user script for another purpose, do not copy the supplied
3user.sco file, but modify your own to make sure that its final
module calls monnaie.sco.

Note on the scripts:
For the use of the script I recommend to set the session parameters
as suggested by Dennis Lefebvre in the ReadCIS documentation
(topic 3.1.1). Do not forget that these parameters must be setup
separately for each different network you are using for access to
CompuServe (I have been caught...).

To retrieve "CLOSING EXCHANGE RATES" from PA News Financial.
To automatically retrieve these exchange rates at your next
connection to CompuServe through ReadCIS, you must:
- start ReadCIS
- create a "user.on" switch
- create a "citynews.on" switch

After your next CompuServe connection, you will find a
"closing.txt" file in the \cis\library\ directory of your
default disk. Previous editions of this file are automatically
erased at each connection. If  you do not want to retrieve exchange
rates, toggle the "citynews" switch or the "user" switch off.

To import exchange rates from "closing.txt" into Monnaie.
Exchange rates in this file are in units per UK£. You must do
the following from within Monnaie:
- "manually" adjust the exchange rate of UK£ against US$
  (Psion-U)
- set the UK£ as "home currency" (i.e. bring it to line 1). You
  can use the "Rotate" option of the menu (Psion-R).
- Set "Format for exchange rates" (Psion-P) to "units per UK£".
  Current setting can be checked by pressing <Tab>.
- make sure that all the "currency strings" correspond to the
  strings in the "closing.txt" file (Psion-S to display all,
  Psion-Y to modify).
- press Psion-I
- select "closing" as the import file in the dialog box and
  press Enter.

To retrieve exchange rates from Global Report.
The monnaie.sco script can retrieve selected pages from the "Foreign
Exchange / Cross Rates / Menu" (page FXC/CRO/MEN) of Global Report
on CompuServe. Note that access to Global Report is not cheap (US$30
to 60 per hour). I strongly recommend the user to follow the script
as it runs, in order to crash-terminate it in case the dialog stops
for some reason, while the Global Report meter is still running (press
Esc and answer "yes" to terminate the script, then Psion-X or whatever
is required to logoff - check your "Comms" reference book in advance).
I will not accept any responsibility for a big CompuServe bill
resulting from an unended Global Report session...

To automatically retrieve exchange rates at your next connection
to CompuServe through ReadCIS, you must:
- start ReadCIS
- create a "user.on" switch
- create a "glo_rep.on" switch and toggle the "citynews" switch 
  off, if you had created one previously ("glo_rep" will be ignored
  if "citynews" is on)
- create one "glo_xxx.on" switch for each xxx/CRO/RAT page you wish
  to retrieve ("glo_mcu" for the MCU/CRO/RAT page, "glo_asi" for the
  ASI/CRO/RAT page,...), or a single "glo_all" switch to retrieve
  all pages listed in the script. 

After the next CompuServe connection, you will find a "glo_rep.txt"
file in the \cis\library\ directory of your default disk. The
glo_rep.txt file, which is included in the present package was
obtained with the glo_all switch on. Note that I have not included
a reference to all the pages of the FXC/CRO/MEN in my monnaie.scr
script. You can add more if you wish, either in the upper part of
the script (with corresponding tests on glo_xxx switches), or in the
lower part (controlled by the glo_all switch).

To import exchange rates from "glo_rep.txt" into Monnaie.
Exchange rates are in units per US$ (only the first column of Global
Report is used for import in Monnaie). You must do the following
from within Monnaie:
- set "Format for exchange rates" (Psion-P) to "units per US$"
- make sure that all the "currency strings" correspond to the
  strings in the "glo_rep.txt" file (Psion-S to display all,
  Psion-Y to modify).
- press Psion-I
- select "glo_rep" as the import file in the dialog box and press Enter.


Disclaimer of responsibility.
=============================
Users of Monnaie must accept this disclaimer of warranty:
"Monnaie is supplied as is. The author disclaims all warranties,
expressed or implied, including, without limitation, warranties of
fitness for any purpose. The author assumes no liability for damages,
direct or consequential, which may result from the use of Monnaie or
any of its accompanying files".


Acknowledgement.
================
The set of scripts supplied with Monnaie cannot be used alone.
They must be called by ThreeCIS or another connection software to
access CompuServe. ReadCIS and ThreeCIS are copyright material of
David Keppler and Dennis Lefebvre.


last update 19 October 1996
