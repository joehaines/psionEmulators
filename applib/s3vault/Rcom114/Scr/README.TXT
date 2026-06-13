    			ADDITIONAL SCRIPTS
    			==================

The \SCR directory contains sample scripts that can be used with the
Comms application that is supplied on the 3Link lead. For more
information about the Comms application, see the 3Link manual.

The sample scripts consist of:

CIXNCS.SCR   -      CIX logon script for non-constant speed modems
_CIX.SCR     -      Improved CIX logon script
_MCI.SCR     -      Improved MCI logon script
_CIS.SCR     -      

CIXNCS.SCR
==========

CIXNCS.SCR is a sample script for logging onto CIX for those who do
not have a constant speed DTE modem. If you have a constant speed DTE
modem, CIXNCS.SCR is of no use to you. (Most modern modems support
constant speed DTE.)

"DTE" stands for "Data Terminal Equipment" - when the modem is
connected to the Series 3, the DTE is the Series 3. "Constant speed"
means that the Baud rate used by the DTE when sending commands to the
modem remains in force regardless of the speed at which the modem
connects to the remote system.

If you do not have a constant speed DTE modem, you have the
following problem when connecting to a remote system:

After the connection to the remote system has been established, the
modem sends a string to the Series 3 in the form "CONNECT 2400",
"CONNECT 1200" or just "CONNECT" (according to the speed of
connection - 2400, 1200 or 300 Baud). The modem then changes the Baud
rate at which it sends characters to the Series 3 to 2400, 1200 or
300, as appropriate. You then have to manually set the Series 3 to
the new Baud rate, before sending or receiving any more data.

This problem is overcome with the CIXNCS.SCR by fully automating the
changes in Baud rate on the Series 3 with the script language.

_CIX.SCR & _MCI.SCR
===================

The _CIX.SCR and _MCI.SCR script are improved scripts for logging on
to CIX and MCI. They set the modem to use RtsCts (hardware)
handshaking and not XonXoff handshaking when communicating between
the Series 3 and a remote system. This prevents data loss between the
Series 3 and modem.

If you use XonXoff handshaking data can be lost between the Series 3
and modem for the following reason:

Most modems (constant speed DTE modems with full buffering) are set
up by default to pass Xon/Xoff handshaking characters through to the
HOST computer, and to ignore them for flow control between the
Series 3 and modem. A problem can occur when the modem has buffered a
large amount of incoming data and then the Series 3 sends an Xoff.
The HOST receives the Xoff and stops sending data, however the modem
continues to send its buffered characters to the Series 3,
potentially causing an OVERRUN error. Using RtsCts (hardware)
handshaking between the Series 3 and the modem prevents this problem
arising by stopping the modem sending any buffered data.

If you look at the scripts you will see that they set XonXoff and
RtsCts handshaking, and then send the AT command AT&J2 to the modem to
tell the modem to ignore XonXoff handshaking, but to obey RtsCts
handshaking.

Although the comments in the scripts suggest that they are designed
for Psion Dacom modems, they can in fact be used with most modern
modems. (These scripts do not, in fact, use the Psion Dacom modems to
the best advantage.) If your modem is not a Psion Dacom modem, check your
modem's documentation for the relevant AT command to turn on RtsCts
(hardware) handshaking, and use it in place of the AT&J2 command.

MNP error correction and compression
------------------------------------

The Psion Dacom modems are capable of using MNP error correction and
MNP 5 compression, allowing effective transmission rates of 4800
Baud. (Of course, the modem at the other end must also support these
facilities.)

If you wish to enable these facilities on a Psion Dacom modem, edit
the script and replace the AT\N0 command with AT\N3, and add a new
line immediately afterwards to send the command AT%C1. Again, if your
modem is not a Psion Dacom modem you will have to consult the modem's
documentation for the relevant commands.

