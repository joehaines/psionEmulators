Zterm V1.00
------------
	Date: 10 Oct 1996


Description
------------
Zterm is simple, dumb, terminal emulator with Zmodem file transfer
protocol. It works on the Series 3a only.

Zterm features:

- full serial port and handshake configuration, including:
	baud rate
	data bits
	stop bits
	parity
	handshaking including:
		Xon/Xoff
		RTS/CTS
		DTR/DSR

- full modem configuration, including:
	modem initialising string
	modem hang up additional command
	dial prefix
	dial suffix

- translate settings for LF/CR/ENTER character

- dialling number editor

- full screen 24x80 dumb terminal emulation

- save current settings and last dialling number on exit

- ZModem file transfer protocol, including
	standard dialog for transmit file selection
	automatic crash recover
	partially transferred file resuming
	ZModem protocol is based on:
	The Working Programmer's guide to Serial Protocols,
	by Tim Kiemtzle, Coriolis Group Inc.
	

***********************************************************************
*                     WARNING AND DISCLAIMER                          *
***********************************************************************
*                                                                     *
* Use this software at your own risk. I will not be held responsible  * 
* for any damage to you, your machine and/or your information as a    *
* result of using Zterm.  			                      *
*                                                                     *
***********************************************************************


Installation
------------
Zterm is distributed as a single zip archive. You will need Unzip (under
Unix, MSDOS, or VMS), PKUNZIP (under MSDOS), or WinZip (under Windows)
to unpack the distribution file, Ztermxxx.zip (xxx = version number). 

 1. Unpack Ztermxxx.zip to an empty directory and check the contents...

	Zterm.app	Zterm application
	Readme.txt	This readme file

 2. Copy Zterm.app into the \APP\ directory on any of your Psion drives.

 3. Install Zterm from the System screen with Psion-I or select Apps |
Install from the menu (for English Psions).


Menu description
----------------

File
	Transmit (Psion + T)
		Send a file using Zmodem protocol.

Connect
	Dial (Psion + D)
		Dial

	HangUp (Psion + H)
		Hang Up modem using Settings/Modem/HangUp string.

	Initialize (Psion + I)
		Initialise modem using Settings/Modem/Initialize string.		

Settings
	Port (Psion + P)
		Sets the serial port options:
			Baud rate	50 ... 19200
			Data bits	5 ... 8
			Stop bits	1,2
			Parity		None,Event,Odd
			Ignore parity	no / yes

	Modem (Psion + M)
		Sets the modem commands string:
			Init string
				determine string which will be send
				to modem for modem initialising.
			Hang Up
				determine string which will be send
				to modem for modem Hang Up.
			Dial prefix
				determine string which will be send
				to modem before dialling phone number.
			Dial suffix
				determine string which will be send
				to modem after dialling phone number.

	Handshake (Psion + K)
		Sets the serial port handshakes:
			Xon/Xoff
			RTS/CTS
			DTR/DSR

	Translate (Psion + A)
		Sets the translating for transmitted character:
			Enter Key 	<CR> / <CR+LF> / <LF>
				determine which code is transmitted
				when the Enter key is pressed.
			Incoming <CR>	<CR> / <CR+LF> / <LF>
			Incoming <LF>	<CR> / <CR+LF> / <LF>
	
Extras
	About (Psion + Shift + A)
		Display the About screen with version,
		copyright and contact details.
	Exit (Psion + X)
		Leave Zterm. Current settings will be save in 
		current .INI file in \OPD\ directory.

Bug Reports / Comments
----------------------
Send all bug reports and comments to the author (Serge Shestopalov) 
at the address shown in the Support section below.


Registration
------------
Zterm is Shareware, which means you can try the program before deciding
to purchase it, and you may give copies of the trial version to other
Psion Series 3a/3c users to try. However, you cannot legally continue to
use the program if you decide not to register it, nor may you give
copies of the registered version or the registration code to others.  
You may use the trial version for 30 days, and I will provide assistance
to get the program to work with your Psion 3a during that period, but
further support is available only to registered users. 

The registration fee is UKP L15 or USD $25.

Payment can be made by...

	* Cash only (no cheques please) to
		Serge Shestopalov
		Balvu 15-56
		Riga, LV-1003
		LATVIA
		EUROPE

	* GO SWREG on CompuServe and use ID number 13299

	* Using the RegNet service on my World Wide Web page at
	  http://ourworld.compuserve.com/homepages/Serge_S

*Important note*:  Please do not send cheques. The local banks all 
charge an excessive amount to cash them. The easiest way to register 
is by sending cash in a good strong envelope (between two pieces
of paper so you cannot see through the envelope).

Once registered, you will receive a personal code to free Zterm from 
the nag screen and limitations. There is no need to re-install.

Please add your full name for the registration message.

Support
-------
This product is supported via e-mail only.
Please send all messages to:

        Internet: dustman@stoat.riga.lv
        CompuServe: 100526,1317

The latest version will always be available from my Web page:

	http://ourworld.compuserve.com/homepages/Serge_S


History
-------
	v0.1 - first alpha release.
	v0.2 - remote "rz" command bug fixed
	       clear terminal screen while
	       transmit/receive bug fixed
	v0.3 - add automatic file receiving start
	v1.0 - first release
	       shareware registration engine added
						
-----------------------
Serge Shestopalov 1996.
