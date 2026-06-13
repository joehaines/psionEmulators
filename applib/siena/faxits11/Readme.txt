
Release Notes for FaxIt 1.2 for Psion Siena
===========================================



1.      INSTALLATION
	1.1 File Contents
	1.2 Installing FaxIt
	1.3 File Structure
	1.4 Start FaxIt for the First Time

2.      CONFIGURATION
	2.1 Setting up the Printer Driver
	2.2 Cabling
	2.3 Fax Modems
	2.4 Troubleshooting Modems
	2.5 FaxIt Settings

3.      USING FAXIT
	3.1 How to Fax a Document
	3.2 The FaxIt Main Screen
	3.3 The Fax Details Window
	3.4 The Diamond Menu
	3.5 The Coversheet Editor
	
4.      SYSTEM CONSIDERATIONS
	4.1 Disk Space
	4.2 System Performance

5.      LEGAL DEPARTMENT
	5.1 Shareware
	5.2 Terms and Conditions
	5.3 Acknowledgements

6.      SUPPORT
	6.1 Support
	6.2 History


==================================================

1.      INSTALLATION

1.1 File Contents
-----------------

The file FAXITS12.ZIP should contain the following files:


 	FAXIT.OPA
	FAXIT.BIN
	FAXIT.RSC
	C2.DRV
	SHEET.OVL
	P_FAXIT.WDR
	COVER1.CVS      (Sample coversheet file)
	COVER2.CVS       .      .
	COVER3.CVS       .      .
	README.TXT      (this file)

Refer to one of the following sections depending on how you will be
copying the files to your Psion. Once the files have been copied across,
continue with section 1.5, Completing the Installation.


1.2 Installing FaxIt
--------------------

I did intend to use the same third-party Setup program as FaxIt 3a/3c.
Unfortunately, it has not been fully ported to the Siena yet. Until
such time as it is, you will need to install FaxIt-S manually. Basically,
using 3-Link, RCOM, PsiWin or any suitable link software, copy the files
to the locations described in the File Structure section below.


Installing Coversheets

FaxIt requires a data directory to keep day to day files in. This
is normally \FAXIT and may also be on any drive, even a different 
drive to the programs. It is not suitable for Flash memory however.
The sample coversheets supplied with FaxIt should be put into this
data directory. 


1.3 File Structure
------------------

You may wish to install the files manually. In this case, they should
finish up in the following structure:
Drive x: is the program drive (may be Flash RAM)
Drive y: is the data drive (preferably not Flash RAM)
x: and y: may be the same drive.

x:\ + APP
    .  |
    .  + FAXIT
    .  .   |
    .  .   + faxit.bin
    .  .   + sheet.ovl
    .  .   + c2.drv
    .  .   + faxit.rsc		(optional)
    .  .
    .  + faxit.opa
    .
    + WDR
    .  |
    .  + p_faxit.wdr


y:\ + FAXIT
	|
	+ cover1.cvs		(optional)
	+ cover2.cvs		.
	+ cover3.cvs		.


1.4 Starting FaxIt for the First Time
-------------------------------------

Install FAXIT.OPA with the standard Psion-I.

The first time you run FaxIt, if you did not install the sample
coversheets, you will be prompted for the drive
on which to install data files. A directory call \FAXIT will be
created on this drive. Due to the volatile nature of the fax
queue, it is preferable that this not be a Flash SSD.


2. CONFIGURATION

2.1 Setting up the Printer Driver
---------------------------------

FaxIt works by periodically looking for a print file called FAX.LIS
in the \FAXIT data directory. It then renames it out of the way while it
is being processed. The easiest way to make this happen is to configure
the printer destination. From the System screen, select Psion-Y. Change
the printer device to <File>, the File Name to \FAXIT\FAX.LIS. Change the
File Disk to the drive where you created the \FAXIT data directory. This
changes the default printer destination for all applications. Even if you
change it back to <Serial> or <Parallel>, it will still remember the Disk
and File Name should you change it back to <File> again later.

You will probably print more than you fax, so it makes sense
to leave the default printer as Serial or Parallel. You can then
change the printer destination temporarily from the Printer Setup
of each application (Psion-Y) each time you want to send a fax.
This sounds more complicated than it really is.


2.2 Cabling
-----------

To connect a fax modem to the Psion, you will need the Psion 3-Link cable
and a modem adaptor cable. The modem adaptor cable is normally available
from stockists of 3-Link. Alternatively, you can make a simple converter
to change the signals from the 3-Link serial cable to those required for
your modem. The following cables work in most cases, although you should
check your modem manual if you are in any doubt.

		3-Link (25pin)  Modem
		======          =====

		2  -------------  3
		3  -------------  2
		4  -------------  5
		5  -------------  4
		7  -------------  7
		6  -------------  20
		20 -------------  6


		3-Link (9-pin)  Modem
		======          =====

		2  -------------  3
		3  -------------  2
		7  -------------  8
		8  -------------  7
		5  -------------  5
		6  -------------  4
		4  -------------  6


2.3 Fax Modems
--------------

The following table is based on feedback from users. If a
modem is shown as "Not Class 2" then it will not work with FaxIt.
Please let me know if it works with your modem, and just as importantly,
let me know if it doesn't and I'll see what I can do (providing your
modem does claim to be Class 2 or 2.0 compatible that is).


AIWA WorldComm PV-PFV144 (portable) 
Andest Roadrunner 14.4 (portable) 
ASCOM AM2496F 
Boca Modem 14400
Cray Quantum 144 				(Not Class 2)
Dayna CommuniCard 14400
Dynalink 1414VQP (portable) 
Eigercom 33.6 PCMCIA
Ericsson 318/DC12 data card (cellular)
ELSA Microlink 28.8 TQV
ELSA Microlink 33.6 TQV
Express 14.4e
GVC 28800 External
GVC 24/96 Pocket
Hayes Accura 144 +Fax	
Hayes Optima VFC+FAX 28800  			(Not Class 2)
Infotel 1414VQP (portable)
Megahertz PCMCIA CC3288
Microcom Deskport 28800
Microcomputer Research Inc. "Mr Modem"  	(Not Class 2)
Microfax 14.4
Multitech MT1932ZDXK
Multitech MT2834BLKV
Motorola Lifestyle 28.8  			(Not Class 2)
Motorola Online 28.8     			(Not Class 2)
Nokia 2110/Data card (cellular)
Nokia 9000
Novafax 28800
Nuvotel Voyager 96424PFX  			(Not Class 2)
Olitec Fax/Modem 14400
Olitec 28800
Pace Linnet 34fx
Pace Microlin FX32+ (portable)
Pace Microlin fx34 (portable)
Pace Mobifax  					(Not Class 2)
Psion Dacom Gold Card Classic (C2 landline, only C1 GSM)
Psion Dacom Gold Card Global
Psion Dacom Surfer
Psion Travel Modem
Practical Peripherals 14.4 
Practical Peripherals PM288MT
Racal ALM3226 (portable)
Repko SL-144.1F
SupraFAXModem 14.4
Trust 28800/14400 Data/Fax
Twincom 14.4
UCOM Nordic 14400
US Robotics Courier V.32bis Terbo
US Robotics Courier Dual Standard V34 
US Robotics Sportster 14400 Vi  		(Not Class 2)
US Robotics Sportster 28800
US Robotics Sportster Vi 28800
US Robotics Worldport 14400 & below  		(Not Class 2)
US Robotics Worldport Dual Standard Cellular
Viva 2496P Pocket Fax
Zoltrix ZX-144  				(Not Class 2)
Zoom PKT 144 (portable)
Zoom V.34XE 28.8
Zyxel U-1496 E+
Zyxel 2864 Elite


An up to date list of working (and non-working) modems can be found 
at the FaxIt Web site. See "Support" below for details.


2.4 Troubleshooting Modems
--------------------------
	
There are two things you can do if you are having problems with your fax
modem. Firstly, run "Query Modem" (Psion-Y). This opens the serial port,
initialises the fax modem, then tests it for Class 2/2.0 compatibility.
If this works you should be able to fax successfully with this modem.
If not, try changing the "DSR handshaking" setting in the Modem
Settings screen (Psion-M). Modems normally work with this set to "No",
however the Psion Travel Modem needs this set to "Yes".

For more detailed diagnostics, select "Trace On" (Psion-T), then
attempt to send a fax. This creates a file "FAXDUMP" in the \FAXIT
data directory which will contain a trace of the conversation between
FaxIt and the modem. This file should be plain text, and can be
viewed or printed. If you cannot persuade your modem to work, e-mail
me this file and I will try to find out why FaxIt is having a problem.
The FAXDUMP file is cleared with each fax attempt so it will only
ever contain a trace of the last attempt (or the last Query
Modem attempt). Psion-T will toggle the Trace setting to "Off".
This setting is not saved when you exit FaxIt so Trace will always be
set to "Off" when you start up FaxIt.


2.5 FaxIt Settings
------------------

Preferences (Psion-Q):

	Automatic Foreground

		Set to <Yes> to jump to foreground when a
		fax print file is found

	Delete Completed Items

		Set to <Yes> to automatically delete Faxes
		after they have been sent successfully. Set to 
		<No> to leave them on the queue with a time of 
		"Defer".

	Maintain Log

		If set to <Yes> FaxIt will maintain a list of
		completed faxes (successes and failures)
		in the file "Faxlog" in the \FAXIT
		directory. This is a plain text file which you
		can view from FaxIt

	Phonecard Prefix / Suffix

		If you use a phone charge card, enter the
		number here. Some cards require both a prefix and a
		suffix, others use just a prefix. You can select
		whether to use the card on a fax by fax basis.

	Close Phonebook After Use

		FaxIt can use a phonebook to store fax numbers.
		It is just a Data application. If this field is set to
		<Yes>, the Data App is closed after each use to conserve
		memory. Set it to <No> to leave the Data App running. Note
		that this only applies if FaxIt launches the phonebook. If
		it was already running it will not be closed after use.

	Phonebook Name

		Use <tab> to select a Data file to use as
		a Phonebook. This must be an existing Database file.
		To create a blank one, use the Data Application then select
		it from here.



Port/Modem Settings (Psion-M):

	Port Settings
	-------------

	Modem Port

		Indicates the TTY: device. Set to "A" on a Psion Siena

	Port Open Delay

		Set to the number of seconds to wait after opening the
		serial port before sending any characters. This should
		be set to a higher value for GSM data card modems which
		often use the port open action to turn on the modem. This
		can result in several seconds delay before the modem is ready
		to receive commands. Some pocket modems will benefit from
		a higher value too.

	DSR handshaking
		Normally set to "No" works with most modems. Set to "Yes"
		for the Psion Travel Modem.


	Modem Settings
	--------------

	Modem is initially

		<Enabled> or <Disabled>. Set the default state
		of the modem when you start FaxIt.

	Modem Initialisation String

		The initialisation string to send to your fax modem
		before each fax call. FaxIt sets any modem settings
		which are important so normally you will not
		need to put anything special in here. It is
		best left as "ATZ" which just resets the modem.

	Dialout string

		This is the dial prefix to be used when you dial out of a
		PBX system for example. Many systems require a '9' followed
		by a pause, so you might set this field to '9,' in this case.
  
	Maximum Speed

		Defaults to 14400 but you can set this to a slower
		speed if you experience recurring communication errors.

	Dial Modifier

		The part of the dial string which must
		immediately follow the "ATD" command. Use
		"T" or "D" to force Tone or Pulse dialling.

	Maximum Line Errors

		This is the limit of *Error* attempts that
		FaxIt is allowed before giving up. When 
		exceeded, a fax stays on the queue with a
		send time of <Defer> until the problem is
		fixed or the fax is deleted. Note that an
		*Error* is defined as a communications
		error such as being cut off, the call
		is answered but not by a fax machine,
		or no answer at all. Line engaged is
		*Not* an Error.

	Retry Period

		The number of minutes that FaxIt should
		wait before retrying a fax transmission.


Coversheet Fields (Psion-S):

	
	Fax Number

		Your fax number. This is sent to the
		receiving fax machine for identification and also
		appears in the header line of outgoing faxes and on
		coversheets if required.

	Name / Organisation

		Enter your name and organisation as you wish them to
		appear on coversheets. Note that these fields can be
		over-ridden on a fax by fax basis.

	Default Coversheet Name

		Select a file to be the default coversheet for new
		faxes. This can be changed on a fax by fax basis.


3. USING FAXIT

3.1 How to Fax a Document
-------------------------

To fax a document from say, the Word application, it is best to select 
fax output before you start typing, as this sets up the fonts you can use
in the document. If you want to fax an existing file, setting the printer 
destination to FaxIt will convert your existing fonts to their nearest
equivalents. To select FaxIt as the destination printer:

	Select Printer Configuration (Psion-Y)

	Press <tab> on Printer model and set it to "FaxIt 1.2"

	Press <tab> on Printer device and change Printer device to "File"

	Set File: Name to "\FAXIT\FAX.LIS" and Disk to point to whatever
		you set your FaxIt data drive to be (Internal/A/B)
		(This is the bit you don't have to do every time if
		you have set it up in the System screen (above) ).

Once you have selected FaxIt as the destination printer, the
following fonts are available within your document:

Times (9 & 13 point), Helvetica (9 & 13 point) and Courier 7 point

The text can be made Bold, Underlined or Italic (Italic is faxed
as reversed text,ie white on black, not sloping text).
Because FaxIt uses a printer model, *any* Siena application should
be able to send faxes, as well as being able to use the standard
Print Preview facility.


FaxIt only moves the FAX.LIS file out of the way once you
have entered a fax number for it. It makes sense to do this
immediately after printing to avoid the print file being 
overwritten. You can set FaxIt to automatically jump to
the foreground and prompt for a number as soon as it finds a
print file. This is the default behaviour. Once you have entered
the fax number (and any other details) the file is saved and
queued for transmission. It is then safe to print anything else
to the fax print file.

FaxIt can be interrupted at most stages during call placement.
Press "Esc" and FaxIt will cancel its present activity and return
control to the user at the next convenient stage.



3.2 The FaxIt Main Screen
-------------------------

The FaxIt screen looks largely like a spreadsheet screen. Each
row is a queued fax which may or may not be eligible to be sent
now. Up, Down, PgUp and PgDn move the selection box a single
item or a page at a time. Home and End go to the top or bottom
of the queue respectively. When a fax is highlighted, the
following actions may be performed:

Del     -       Delete this item. You will be asked for confirmation.

Enter or
Psion-V -	View the fax fields in more detail.

Psion-N -       Send Now, regardless of the time it is queued for

Psion-D -       Defer. This fax will remain on the queue but
		will not be sent until specifically changed.
		Useful for queuing up faxes when you don't have
		access to the modem or a phone line for example.

Psion-C -       Change the settings using the Fax Details window
		described below.


The Columns of the screen should be largely self explanatory.
The Status column is the status of the *last* transmission
attempt. If blank, no attempt has been made yet.


3.3 The Fax Details Window
--------------------------

This screen will be presented whenever FaxIt has found a print
file to be faxed, or when you change the details of a fax
(see below).

Fax Number      -       The fax number. Note that you can use the
			<P> button to call up your phonebook if you
			have one set up. Highlight the fax number in
			the Data App then return to FaxIt (Shift-System
			will achieve this). Then usr the <B> button to
			bring the fax number into FaxIt. This will also work
			from any other application which supports Bring.

			Note also that you can specify fax numbers using
			the Psion Country Suffix, e.g 0123 456789[France].
			This is translated automatica;;y and the resulting
			number is shown on the next screen for further editing
			if required.

Use Phonecard	-	Set to <Yes> to prefix the number with the phonecard
			number set up in the Preferences Screen. Note that
			the phonecard number will not show in the FaxIt screen
			but will be used when the number is actually dialled.

Send Time       -       Set to <Now>, <Defer> (never, unless changed)
			or <Specify> to set an actual Date/Time.

With Coversheet -	Set to <Yes> to include a simple user-designable
			coverpage on the front of your fax.


Fax Details Second Screen:
Note that this screen differs depending whether or not a coversheet
has been specified.

Fax Number	-	The translated fax number (see previous screen)
	
Reference       -       A reference for you to recognise this
			fax on the Queue



The following fields can be entered when a coversheet has been
selected.

Attention Of	-	The intended fax recipient

Organisation	-	and their company name

From    	-       Your name.

My Organisation	-	Your company name

Coversheet Name	-	Filename of a coversheet file to use
			(*.cvs)



3.4 The Diamond Menu
--------------------

Psion-plus (+) enables the fax modem in Direct Dial mode.
Psion-star (*) enables the modem in Dial-Out mode. It will prefix
	fax numbers with the Dial-Out string set up in the Modem
	Settings dialogue (Psion-M).
Psion-minus (-) disables the modem.

Alternatively the Diamond Key toggles between the three states.



3.5 The Coversheet Editor
-------------------------

You can create new Coversheets (Psion-W) or edit existing ones (Psion-E).
Coversheets must reside in the \FAXIT data directory and have a file type
of .CVS

The editor is fairly simplistic and straightforward. All coversheets
use the Courier 7 font. You can also set a bold font by inserting ~B
at the beginning of a line. Note that the bold attribute will apply to
the entire paragraph, i.e. up to the next <return>. A second ~B will
revert to normal font. You can embed 'tags' which will be replaced by
the appropriate fields when the coversheet is faxed (see menu).

A coversheet can be up to 2048 characters long.



4. SYSTEM CONSIDERATIONS

4.1 Disk Space
--------------

The application itself is not very large. Queued faxes are small too,
so queuing up a batch of faxes does not take up an undue amount of space.
Conversion of the queued fax into a format for transmission can be quite
disk hungry however, and can be anything from 10k to 30k per page
depending on content. Be aware of this when faxing large documents.


4.2 System Performance
----------------------

Sending a fax to a Group 3 fax machine involves blasting the data down
the serial port to your fax modem at 19,200 baud. The fax protocol does
not allow for interrupts in transmission, meaning that if ever the modem
runs out of data (perhaps because the Psion is doing something else) the
fax call will drop and will have to be started over. Task switching
while a fax is being transmitted, lowers FaxIt's priority to below the
task you switched to. The call will *probably* drop although this does
depend on what you switched to, and how much buffer memory your
fax modem has. To be absolutely safe, do not task switch while
a fax is being sent.



5. LEGAL DEPARTMENT

5.1 Shareware
-------------

FaxIt is Shareware. An unregistered version is fully functional, except
that it is limited to sending one page. This restriction can be lifted
by sending 14UKP (US$25) to the address below, or on Compuserve, GO SWREG
to access the Shareware Registration service. FaxIt's Registration ID is
????. You will then be sent a registration code. You may use the software
for 30 days for evaluation purposes after which you must either register
it, or remove it from your machine.



5.2 Terms and Conditions
------------------------

FaxIt may be distributed freely providing that a) it is not modified in
any way, and b) that this notice forms part of the distribution.

Reverse engineering of any component of FaxIt is
expressly forbidden.

The author and distributors of this software can not be held liable for
any damage or loss of data either directly or indirectly, arising from
the use of FaxIt. No warranty is either expressed or implied.


5.3 Acknowledgements
--------------------
"Psion Setup  (c) is  written  by Jochen  Siegenthaler and  used  under
licence." You can find information about SETUP as well as other useful
Psion software at Jochen's Website:
http://ourworld.compuserve.com/homepages/JochenSiegenthaler

6. SUPPORT

6.1 Support
-----------

A Web page has been created to provide up to the minute information
about FaxIt. A list of compatible modems is maintained, as well as facts
and fixes for support issues. News about upgrades and up and coming
features and a few screen shots are also maintained here too.
Alternatively, I can be contacted by e-mail or letter post at the
addresses given below.

The Web page URL is given at the bottom of this document.

Please let me know if you find things in FaxIt that don't work
as advertised. That way the software gets fixed sooner. I am also
keen to receive feedback about modems which work, and those which
don't.


6.2 History
-----------

1.1
	First Release	Based on FaxIt 3a/3c 2.2

1.2
	Fixed problem with duplicate pages in registered version


Walter Wright
1 Deanery Cottage
High Street
Sonning
Reading
Berkshire
RG4 6UP UK

NB THIS POSTAL ADDRESS IS LIKELY TO CHANGE EARLY 1998

Compuserve: 101367,2040
E-mail:     WalterWright@compuserve.com
Web Page:   http://ourworld.compuserve.com/homepages/WalterWright



