eMail-It v1.4 (c)1999,2000 by Mark Richardson
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
This program was initially started in 1998 with the idea that the PsiStack
project would come on stream. During that time I used eMail-It in conjunction
with Eudora v1.4.2b running on a PC to send/receive my mail.  Subsequently,
I wrote my own Tcp/Ip stack when PsiStack got shelved.


IMPORTANT INFO FOR CURRENT USERS OF eMail-It
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
If you are currently using a version before v1.4, then your current
configuration/setup files will NOT work with this version and subsequent
releases.  There have been significant changes to v1.4 and as a result,
emailit.cfg and emailit.tcp contents have been altered. You have two choices
to upgrade to v1.4 and beyond:
 (1) use the program v1xv14.opo that is in this zip file and it will
     convert your old files to the new format.  This utility will rename
     your current emailit.cfg to emailit.cfb so you can go back to an
     earlier version of emailit if you want.
 (2) Delete emailit.cfg and emailit.tcp and re-enter all the setup data
     again.
If you are happy with this version, delete emailit.cfb and v1xv14.opo from
your Psion or Siena.


Current Status
~~~~~~~~~~~~~~
The system can send (SMTP) and receive (POP3) email over a dial-up line to
an ISP that uses PPP for its link.  The TcpIp software supports the standard
verbose username/password dialog or the Password Authentication Protocol
(PAP) for connecting to an ISP.  It has been tested on a 3a, 3c, 3mx and
even a Siena using various modems and ISPs around the world.  It works but
is not fast.  The biggest hurdle will be getting connected to your ISP by
using the correct script for the connection (refer to the System Setup
section).  One attachment per message can be sent and I have only tested the
Base64 format.  eMail-It can remove attachments from incoming messages that
are in MIME Base64 format.  I have not tested how eMail-It handles "large"
messages or when you have limited free space on your drive; I usually have
100k free. It should recover if it detects a problem.

NB: The TcpIp software expires on Dec 31/2000.  Please contact me for status
of revision.

You will need a "null modem cable" to connect your Psion to a modem. If you
need to make one, refer to Walter Wright's Faxit 2.4 software documentation.
  http://ourworld.compuserve.com/homepages/WalterWright
The adapter I made which connects the Psion serial cable (DB9 connector) to
my modem (DB25 connector) was wired as shown on the left, and a DB9 to DB9
adapter (provided by Martin Haenseler) is shown on the right:
   Psion DB9     Modem DB25    Psion DB9   Modem DB9
    TD  3   ---> 3  RD              3   ---> 2
    RD  2   <--- 2  TD              2   <--- 3
   RTS  7   ---> 5  CTS             7   ---> 8
   CTS  8   <--- 4  RTS             8   <--- 7
   DSR  6   <--- 20 DTR             6   <--- 4
   DTR  4   ---> 6  DSR             4   ---> 6
    SG  5   <--> 7  SG              5   <--> 5
    RI  9   <--- 8  DCD
   DCD  1  no connection


General Information
~~~~~~~~~~~~~~~~~~~
The system generally cancels your requested task (ie. New message, Reply
message, Compress mailbox) if you press <Esc> at the FIRST dialog box the
system displays.  If you press <Enter>, the task will usually continue to 
completion. However, you are given several other chances to cancel new and
forwarded messages by pressing <Esc> (ie. during the nickname lookup and
again when you are asked to fill in Subject and Attachments).

The numbers in the top left corner of the screen show the number of messages
in the mailbox and the size of the mailbox.  The mailbox which is currently
open is shown in the top centre.  The current date & time in the top right.

The grey bar highlights which message will be selected for a task. The
<Up Arrow>,<Down Arrow><PgUp>,<PgDn>,<Home>,<End> keys will move this grey
bar.

The symbols in the left column of the screen can be best described by saying
what they mean.
  Deleted  Msg is "deleted"
  >>       Msg came from the Internet (Incoming)
  R        Msg has been replied to
  F        Msg has been forwarded
  >        Msg is unread (symbol is a black half diamond)
  <<       Msg was sent to the Internet (Outgoing)
  Q        Msg is queued to be sent later
Normally your Inbox will have certain symbols and your Outbox will have
others.  But since you can move messages around (such as an archive mailbox),
you can end up with any symbol in any mailbox.  Mail arriving from the
Internet will be placed in the IN mailbox and any queued mail to be sent to
the Internet must reside in the OUT mailbox.

eMail-It uses the built-in ROM applications (Word and Data) for many of
its functions.  When these applications are invoked, eMail-It clears the
screen and displays a flashing message showing what sub-activity eMail-It
is "waiting" on.  If you see the blank eMail-It screen, press <WORD> or the
<DATA> button to get back to what you were working on.  Another method is
to press <Shift><System> together to "scroll" through all the active
processes running on your Psion until you get to the required one. To return
to eMail-It, you must exit these applications using <Psion-x> as usual.


Some General Commands
~~~~~~~~~~~~~~~~~~~~~
Pressing <Enter> will display the selected message. The 1st message header
is "System-Time: " and indicates when this message was initally processed
by eMail-It.  Since Word is used for displaying/viewing messages, you can
scroll, zoom, print, cut/paste, etc.  Any changes you make to this "copy"
of the message will be discarded.  To "cut/paste", use the "Bring text"
facility of Word to move portions of text from this message to another Word
document, or you can save your "cuts" to a file using the "Save as" feature
of Word and "paste" these into a new message using the "Merge in" feature of
Word.  Note: It is not possible to "Bring text" from a message to a new
message directly since eMail-It only allows one message to be open at a time.

Pressing <Delete> or <Psion-g> will delete/undelete the selected message.
To remove deleted messages from a mailbox, use <Psion-k> to compress the
mailbox. An option in the compress task is to remove 'verbose' message
headers which take up alot of space.

You can move a selected message to another mailbox with <Psion-m>.

The "Send again" command <Psion-q> allows you to create a new message with
the body of the message being the same as the selected message.  If you use
this command on an outgoing or queued message, you will be asked if you
wish to keep the current message headers (ie. To: Cc: Subject: etc.).
This will save you from re-entering this info if you just want to "edit"
the message body.

Press <Psion-t> to send/receive your email via the Internet.  Here you will
be able to define which ISP to connect to.  After a connection has been made,
you will see the "Transfer Mail" menu which allows you to send and receive
your email.  You can also update your "Mail Settings" and "TcpIp Settings"
from this menu. NB: not all TcpIp revisions take effect immediately but will
on your next attempt to connect.

The command <Psion-u> will "un-attach" an attachment on a message that has
been sent in MIME Base64 format.  This function will try and find an
attachment on the selected message.  If one is found, it will be converted
and written to the file you entered or accepted.  The directory
\emailit\attach\ must exist for this to work.  The attachment will be
replaced by a message indicating where it was written to.  To recover the
used disk space, use <Psion-k> to compress the mailbox.


Nicknames
~~~~~~~~~
This is a database where you can maintain all your email addresses.  A
skeleton database has been included with the zip file and has the following
titles:
  Nickname:  a short name you can easily remember ie. jb
  Address:   the actual email address ie. joeblow@isp.com
  Name:      the real name of the person ie. Joe Blow

You can also use the nickname file to create mailing lists such as:
  Nickname:  ml1
  Address:   joeblow@isp.com, myfriend@aol.com, suzy@virgin.uk.co
  Name:      Mailing List 1
To use this mailing list, you would address the message To: ml1 or Cc: ml1.
eMail-It will translate "ml1" into the 3 real email addresses but the To: or
Cc: header will remain unchanged.  You can create a pseudo Bcc: this way.

A note about how eMail-It translates nicknames is in order.  If a nickname
is NOT the last entry in To: or Cc:, only the FIRST email address in the
address field will be used, otherwise ALL email addresses are used.
For example, if you sent a message with the following "To" header:
  To: jp, suzy@virgin.uk.co, ml1
eMail-It would lookup jp in the nickname file and use the FIRST email address
listed, use suzy@virgin.co.uk as written, lookup ml1 in the nickname file and
use all email addresses listed. As a result, the message would be sent to
jp, suzy, joeblow, myfriend, and suzy again (suzy gets TWO copies).
NB: eMail-It does *not* detect infinite loops!!

If you address the message "To: ml1, suzy, jp" then joeblow, suzy, and jp
would get the message.


Installation
~~~~~~~~~~~~
1) Unzip the emailit.zip file. The files included in the zip file are:
     Emailit.opa  *  email client application
     Emailit.pic     startup graphics screen
     Emailit.hlp     help file
     Emaitit.nic     nickname file with correct titles
     readme.txt      this file
     updates.txt     a summary of revisions to eMail-It so far
     v1xv14.opo      conversion utility to upgrade setup files to v1.4 fmt
     sample.toc      a sample mailbox
     sample.mbx
   The only required (*) file to be installed is Emailit.opa (all others
   are optional).
   eMail-It will create the nickname file if you want to use the
   nickname facility, but you will have to change the titles yourself.
   eMail-It does not care what the titles are; they are there for your use
   only, but the first 3 fields are used based on assumed contents;
   (Nickname:, Address:, Name:).
2) Create two new directories (\emailit\ and \emailit\attach\) on one of
   your Psion drives: M (internal), A, B or C.  Flash disks should not be
   used due to eMail-It's I/O requirements.
3) Copy the files you want to use to the new \emailit\ directory.  Yes,
   emailit.opa should be placed in here to keep everything together.
4) Install Emailit.opa to the system screen.
5) Run the application.  The system will create the following files in the
   \emailit\ directory:
     Emailit.cfg     eMail-It configuration file
     Emailit.log     optional log file to record msgs sent and received
     Emailit.sig     optional signature file
     Emailit.tcp     TcpIp configuration file
     xxxxxxx.toc     mailbox file (table of contents)
     xxxxxxx.mbx     mailbox file (messages)


System Setup
~~~~~~~~~~~~
When you first run eMail-It, you will be asked to create a configuration
file and the system will then prompt you for some information.

We will use the following scenario to show what info goes where.  Let's say
your name is Davey Jones and your email address is: d.j.locker@yourisp.com
Your internet service provider (ISP) has told you that your email password
is "SECRET", your SMTP Server is called smtp.yourisp.com and your POP3
Server is called pop.yourisp.com

The first screen allows you to set some preferences (ie. which POP3 server
to access to retrieve your mail, which type of "editor" to use, the width
of a reply message, whether you want to keep a log file to log your sent
and recd messages).  You can define to eMail-It, three different POP3
servers and one SMTP server.  You must run "Mail Setup" several times to
define all your POP3 servers if you have more than one.  Once all your POP3
servers have been defined, you can select just one POP3 server to download
your messags from or the "ALL defined servers" option.

The second screen is called "Network Configuration"
  POP3 Server       pop.yourisp.com
  User Name         d.j.locker
  Password          SECRET
  SMTP Server       smtp.yourisp.com
  Real Name         Davey Jones
  Return Address    d.j.locker@yourisp.com

Your password: eMail-It stores it encrypted. If your password is more than 8
chars, type the first 8 on this screen and you will be prompted for 8 more
on the next screen. NB: The password is limited to 16 chars and you only have
to enter your password once; ie you do not need to re-enter it again if you
decide to change any of your settings later.  You only need to re-enter it
when you actually want to change it.

The third screen is for the "Receive Mail" settings.
 Leave mail on server?  If you use a PC to normally get your mail and the
    Psion while travelling, you might want to set this to Yes so that you can
    retrieve all your messages with your PC later. If you set this to No,
    the message will be deleted from the server after it has been downloaded
    to eMail-It.
 Truncate big messages? Some messages can be quite large and memory on the
    Psion quite limited. If set to Yes, large messages will be truncated
    after a set number of message lines are downloaded from the server.
    Truncated messages will be displayed on the screen and you will be
    asked if you wish to delete a truncated BIG message if "Leave Mail on
    Server" has been set to No.
 Number of message lines? This is the number of message lines (excluding
    headers) to be downloaded of a big message.
 How BIG is big?        You define what a large message is. The default 10K.

The fourth screen is for the "Send Mail" settings. NOTE: These settings
apply when ALL your queued messages are finally sent to the SMTP server that
is defined on the "Network Configuration" screen and NOT for any one
particular message.
 Use Signature?         If you want to append a standard set of text to all
   your outgoing messages, set this to Yes. The next screen will be the word
   processor so you can create/edit your signature file.
 Time Zone:             This field is appended to the "Date:" header of the
   outgoing msg. It is your time zone in the world relative to GMT.
   (Note: -0500 can be used for EST)
 Format for Attachments: You have only 2 choices & Mac users use BinHex :-(.
   Base64 will be sent as a MIME multipart/mixed message. UUcode will be
   appended to the original msg in Unix compatible format.  The UUcode format
   was a popular format before the introduction of MIME and Base64 but has
   since become less popular.
 Reply-To Address:      The field is left blank unless you want a "Reply-To:"
   message header to be included in your outgoing messages.  This is used
   when you want to have any replies to your messages sent to a specific
   email address which is different than your Return Address you defined
   on the "Network Configuration" screen.


Tcp/Ip Setup
~~~~~~~~~~~~
The TcpIp setup is done when you initiate the mail transfer facility.
The first screen asks you which settings to access (Port or ISP settings).

The port settings will be used with any of the ISP setups since these
settings pertain to your modem and communications port. IrDA has been added
but it has NOT been tested yet.  "Ignore DSR" should be left as "Y" for most
modems, however the Psion Travel Modem requires this set to "N".  The Debug
level has 4 settings; if you select "file", the file \emailit\dump will be
created to record the modem traffic.  Useful for finding out why you cannot
connect to your ISP. Setting the level to "screen" may give you some comfort
in that something is going on when you see little progress being made, but
do not be alarmed by some of the diagnostic messages that will be displayed.

  Std Modem Settings: Psion Travelmodem    Others
     Baud rate:          38.4k             depends
     Parity:              none              none
     Data bits:            8                 8
     Stop bits:            1                 1
     Handshaking:         RTS               RTS
     Ignore DSR:           N                 Y

You are allowed 3 different ISP setups.  To configure an ISP setup, set the
name in the "ISP:" field to the one you want and set the "Action:" field
to "Update".  You can change the ISP name on the next screen but the name
must be more than 1 char in length and have no commas in it. The info about
passwords given in the previous section applies here also.

The 4-digit Domain Name Server numbers, sometimes just referred to as DNS,
come from your ISP.  You will see an error message if you enter these in an
incorrect manner and the faulty one(s) will be reset back to "0.0.0.0".
"Freeserve" in the UK tell you that "Domain name service is dynamic - you do
not need to set this."  This may be true if you use some specialty software
from "freeserve", but you will have to search their website for these DNS
numbers; look in the Mac section.  At least DNS1 must be valid in order for
TcpIp to work.

The login script is very basic.  You send something, you wait for something,
you send something, you wait for something.  The somethings are CASE
sensitive and are separated by one or more spaces.  Everything sent is
terminated by a carrage return if one does not exist already.  When the logon
script has completed, the message "Logging on to Network" will be displayed.

There are 4 keywords:
  ph$  the phone number
  id$  the userid/name
  pw$  the password
  sp$  special char encoding
Use a keyword when you want to send that data.  The sp$ keyword is followed
by the 3 digit decimal code of the char you wish to send.  This keyword can
be used over and over to send a string of chars.  ie. sp$032Asp$013 will
send the space char, the letter "A" and then a carrage return.

The script is limited to 250 chars and initially set to:
   ATZL0E0 OK ph$ : id$ : pw$
ie. send a modem initialization string to the modem and wait for OK to be
received back.  Then send the phone number and wait for a colon to be
received. Then send the user name and wait for a colon, then send the
password.

The biggest hurdle to get eMail-It to work is getting connected to your ISP
using the correct script.  It helps if you logon to your particular ISP using
a "terminal" to see what your ISP dialog is in order to create the correct
script.  But you can also do it by "trial and error".  To do this, start with
a script that just sends the phone number and wait for something, say "::".
   ATZL0E0 OK ph$ ::
When you "timeout", TcpIp will display on the screen what was sent back by
the ISP.  If the ISP does not prompt you for your username, it may want you
to use PAP/CHAP or it may be waiting for you to send just a carrage return.
If your ISP is using PAP/CHAP, then the following script may work:
   ATZL0E0 OK ph$
If you see a username prompt, use some part of this prompt to create the next
part of your script. For example, the ISP may send a stream of info ending
with "Username".  So tell the TcpIp to wait for some characters such as
"name" and then send your userid.  The next script to try would then be:
   ATZL0E0 OK ph$ name id$ ::
and see what comes back.  It will usually be a prompt for your password. Your
final script may end up being:
   ATZL0E0 OK ph$ name id$ word pw$

When you see "Logging on to network", your script has completed and PPP Link
is now going to try and "talk" to your ISP.  If you get "PPP protocol bad"
or "ISP wishes to CHAP", your ISP is using a protocol that the TcpIp does
not recognize and you MAY not be able to connect.  If for any reason the
TcpIp cannot "talk" to the ISP correctly, you will get the message
"Unable to negotiate PPP (login failed)".

Three examples of what dialog your ISP may go thru are:
   Username:       []          Login:           []   CONNECT
   Password:       []          Password:        []   :
                   []          Protocol:        []   ID:
The questions usually end in a ":" and you are requested to send some info.
In the second example, the ISP wishes to know what protocol to use; SLIP,
CSLIP or PPP.  The TcpIp only supports PPP so you would respond with PPP.
The 3rd example is for Compuserve and you are to send just a carrage return
after the CONNECT msg to start thing rolling and a very long string of data
for the id which also incorporates your password.  Similar to Compuserve,
your ISP may just wait for you to send a carrage return after the modem
connects.  You may have to play around to get the correct script for each
ISP you use.

Sample scripts for the above three examples could be:
 Example 1:  atzl0e0 OK ph$ name: id$ word: pw$
 Example 2:  ATZ OK ATL0E0 OK ph$ gin: id$ word: pw$ ocol: PPP
 Example 3:  AT&F OK ATZL0E0 OK ph$ CONNECT sp$013 : +CIS ID: id$
             NB: append the following to your ISP id in the User Name field
                  +/NOINT/PPP:CISPPP\pw$
             The pw$ will be replaced by your real password when id$ is sent.
Another sample script is the following which used an ELSA Microlink 28.8TQV
Modem to connect to the "Blue Window" ISP in Switzerland.  The ISP timeout
field was set to 40.  This ISP uses the CHAP/PAP authentication method for
userid/password validation.
  at&f\v8 OK ats7=60s30=0l0m1\n3%c3\q3x4 OK ph$

What commands you send to your modem depends what modem you have, but there
are some standard things such as ATZ (reset) and AT&F (load factory
settings).  Modem commands may work in lower case, but the responses are
normally upper case.

One final word on the TcpIp.  It maintains a monthly cache of DNS lookups in
order to speed up processing.  This should not present a problem, but if you
experience problems, delete the cache manually using "TcpIp Setup".


Known Limitations/Bugs
~~~~~~~~~~~~~~~~~~~~~~
1) Most setup fields are limited to 40 chars, passwords 16 chars.
2) Only one attachment can be placed on a message.
3) Only one To: header is allowed and is limited to 250 characters.
4) Only one Cc: header is allowed and is limited to 64 characters.
5) Multiple addresses are separated by a comma and one or more spaces.
   Use a comma ONLY to separate email addresses.  Do not use a comma in a
   way such as  To: "Smith, Joe" <js@isp.com>  it will not parse correctly.
6) You can place nicknames in the To: and Cc: headers, but be aware of how
   eMail-It translates nicknames to actual email addresses.  See the
   Nicknames section for details.  The "nickname address" field MUST be
   less than 250 chars and this can be easily checked by seeing that the
   "Name" field is correct when you do a nickname lookup.
7) You cannot change the From: address except with Mail Setup.  The info
   stored in Mail Setup is used when communicating with the SMTP server
   and not what is shown in the message header.  I may change this as some
   servers check who are authorized to send messages based on the From:
   header.
8) Only attachments in MIME base64 format can be removed from a message.
9) Standard mail messages use the US-ASCII character set (ie. 7-bit) and
   eMail-It does NOT check if you use only these characters.  eMail-It
   creates 8BITMIME message headers for all messages but does NOT check
   if the server will process them.  Not all Mail Transport Agents (MTA)
   will.  You run the risk of the message being rejected if you do not
   use US-ASCII.  With ESMTP becoming the standard, this may not pose a
   problem, but ....


Special Thanks To
~~~~~~~~~~~~~~~~~
Info on UUcode came from Jochen Siegenthaler.  The initial design of the
startup graphics screen was taken from the works of Adam Dawes.  The icon
was slightly modified from the works of Matt Gumbley.  Alan Clifford (UK),
Gian Mario Weiss (Switzerland), and others, helped with Beta testing.


To Do List
~~~~~~~~~~
1) Rewrite TcpIp (a separate application) in assembler in hopes that it will
   speed up the Internet communications.
2) Check for 8-bit chars in outgoing mail.


Legal Stuff
~~~~~~~~~~~
This application (eMail-It v1.4 and TcpIp v1.2) is offered as freeware and
is copyrighted by Mark Richardson 1999,2000

I cannot be held responsible for any loss of data or damage caused to you,
your machine or anything else in near proximity caused directly or
indirectly by eMail-It or its associated programs.

If you redistribute this package, it *MUST* be done by way of the
*UNALTERED* .ZIP file which you SHOULD have received in the first place!

Reverse engineering of this application's component parts as well as TcpIp
is STRICTLY FORBIDDEN!

etc, etc, etc   ;-)


Mark Richardson
February 2000
email: markr@s4.interpasscard.net

