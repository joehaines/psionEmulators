MESSAGE.OPX
-----------
Copyright (c) Otfried Cheong and RMR Software 1998

If you going to distribute the OPX further then please read Part 5 first. This is VERY IMPORTANT.

README file
-----------
Contents:

1.	Introduction
2.	File Details
3.	Installation
4.	Using the OPX
5.	Distributing the OPX
6.	Registration
7.	Other Programs from RMR Software

INTRODUCTION
------------
This OPX gives you access to a number of functions in the Message Suite, to enable you to access
them from within a OPL program.

FILE DETAILS
------------
The archive consists of the following files:

README.TXT		This file
MESSAGEOPX.SIS	This is the main OPX file in SIS format
MESSAGE.OPX		This is the WINS version of the OPX
MESSAGE.OXH		This is the header file
MESSAGE.OPL		This is a demonstration program that shows you how the OPX can be used

Please note that the SIS file *MUST* be called 'MessageOPX.sis' to avoid clashes with Message Suite (which
happen if it is named Message.sis).

INSTALLATION
------------
1.	Install MESSAGEOPX.SIS

2.	Copy MESSAGE.OXH into the \System\Opl\ folder on either the C: or D: drive

3.	Copy MESSAGE.OPL any where you like

USING THE OPX
-------------
1.	First compile and run the MESSAGE.OPL file to make sure everything works and it communicates with
Message Suite correctly.

2.	To use the OPX in your program add the following line to the top of the code, immediately after
the APP...ENDA and before the first procedure

	INCLUDE "MESSAGE.OXH"

3.	You can now use the following additional procedures in your program.

MessageSend:(adr$, subject$, buffer&, length&)
==============================================
Send the message in the buffer at address buffer& (allocated with ALLOC) of length& directly to the
Message suite outbox. adr$ and subject$ are email address and subject line.


MessageEdit:(adr$, subject$, buffer&, length&)
==============================================
Like MessageSend, but instead of putting the message directly in the outbox the Message suite come
up with an edit buffer of the given message. The message can still be edited, and then saved to the
outbox, as a draft, or discarded.


MessageIndex&:(path$)
=====================
This function reads the index of the messages kept in the local folders on the machine. It returns the
number of messages available. The argument gives the path to the directory containing the local index
file. The default value is "C:\system\messages\local". If your messages are on "D:", it is sufficient
to pass "D:". 

This function HAS to be called before you can use any of the remaining functions.


MessageStatus&:(i&)
===================
Returns the status of message number i&. Numbering starts from one. The status is one of deleted, unread,
read, draft, ready-to-send, sent. Suitable constants are defined in Message.oxh.
 

MessageType&:(i&)
=================
Returns the type of message number i&. Currently, only Email and Fax are supported.
 

MessageToFrom$:(i&)
===================
Returns the To or From field of the message (depending on the type).


MessageSubject$:(i&)
====================
Returns the subject field of the message.


MessageSize&:(i&)
=================
Returns the size of the message in Bytes. 


MessageBody&:(i&, buffer&, length&)
===================================
This function can be used to read the content of the message into a buffer allocated with ALLOC.
Pass the ADDR of the buffer in buffer&, and the length of the available space in length&. The function
returns the number of bytes placed in the buffer.


DISTRIBUTING THE OPX
--------------------
If you wish to distribute the OPX as part of your program, then you need to include the unchanged
MESSAGE.SIS in the ZIP archive or SIS package for your program. Note that you may not, UNDER NO
CIRCUMSTANCES, distribute the unpacked MESSAGE.OPX file. 

If you do not follow this rule, you disable the EPOC version control over the OPX, and your
application may break when the user installs a different version of the OPX. Worse, installing
your application may break other applications, and you can imagine the reactions this may cause you.
Don't say you haven't been warned!

You may not rename the OPX that you distribute, and you may not redistribute MESSAGE.OXH or MESSAGE.OPL,
they are simply for use on the developers machine. (The first rule makes sure that multiple copies of
OPX with the same UID cannot happen, the second rule is to avoid a proliferation of outdated copies of
our OPXs. Please refer interested parties to the RMR website.)

Shareware using this OPX must include this information in the "About" screen. (A line like "Contains
MESSAGE.OPX © Otfried Cheong" or similar.)


REGISTRATION
------------
MESSAGE.OPX is free for personal use and for use in Freeware programs. If you wish to distribute it in a
Shareware Package, then we ask that you register it by E-Mailing us at opx@rmrsoft.com. We are asking
for a nominal fee of twice the registration fee of your program. This also includes full backup support,
such as a WINS copy, e-mailing of enhancments and influence over future development of the OPX. Hope you
think this is acceptable, we are not trying to make money on this, just cover out costs.


OTHER PROGRAMS FROM RMR
-----------------------
If you like the look of this OPX, why not have a look at our programs.

A full list is as follows:

S5BANK			: A Personal Accounts Suite
RMRTASK			: An Extended Task (ToDo) Manager.
RMRNOTES		: A Note Taker/Jotter program
S5HOME			: A Home Inventory program.
S5FUEL			: A Fuel Consumption Monitor.
RMRUTILS		: A Utility/Conversion program
RMRZIP			: A Compression/Archive Utility
VACTRAC5		: A Holiday/Leave Tracking program
RMRSOL			: The classic "Solitaire" patience game
RMRFILE			: The premier file manager for EPOC machines
CONTACT			: The only Contact Manager available for EPOC machines

Some of these are also available in other languages, such as French, German, Spanish etc.. See the
Home Page http://www.rmrsoft.com/ for details.