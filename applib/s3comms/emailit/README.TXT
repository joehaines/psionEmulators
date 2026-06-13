 ---- IMPORTANT ----
This release has changed the format of the emailit.cfg file and you will
get a "Media is corrupt" error when you try to run eMail-It.
Use the convert.opo program in this zip to convert this file to the new
format (version) if you have not done so previously, or delete emailit.cfg
and re-enter your mail setup data (NB: your POP3 information will *not*
be lost as this data is stored in tcpip.cfg).

In addition, if you define your SMTP server(s) in the ISP setup, you will
need to re-enter this data as it will be lost.  This is a result of adding
SMTP Authentication capabilities to eMail-It.


eMail-It v2.1 (c) 2003 by Mark Richardson
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
This program was initially started in 1998 with the idea that the PsiStack
project would come on stream. During that time I used eMail-It in conjunction
with Eudora v1.4.2b running on a PC to send/receive my mail.  Subsequently,
I wrote my own TcpIp stack when PsiStack got shelved.  This package has
been developed entirely on my Psion 3c.


Current Status
~~~~~~~~~~~~~~
The system can send (SMTP) and receive (POP3) email over a dial-up line to
an ISP that uses PPP for its link.  The TcpIp software supports the standard
"scripted dialog" where an ISP prompts you for your username/password, the
Password Authentication Protocol (PAP) and the Challenge Handshake
Authentication Protocol (CHAP) for connecting to an ISP.  It has been tested
on various SIBO machines (3a, 3c, 3mx, Siena, Workabout MX) using various
modems and ISPs around the world.

The biggest hurdle will be getting connected to your ISP by using the correct
script for the connection.  The following 2 scripts should work in most cases
[I think].  The 1st one is for ISPs using "Scripting", the 2nd for ISPs using
an Authentication Protocol such as PAP or CHAP.
    ATZE0L0 OK ph$ : id$ : pw$ ~  (note: ~ is used if you get a frame error)
    ATZE0L0 OK ph$ CONNECT
Refer to the "TcpIp setup" section for more details.


One attachment per message can be sent. eMail-It can remove MIME attachments
from incoming messages that are encoded as text, printed-quotable, Base64
or UUE.  I have sent and received messages as large as 150k; eMail-It is
capable of even larger msgs depending on the free space on your drive. I
usually have 100k free.

NB: Please read licence.txt since the TcpIp software which eMail-It uses is
SHAREWARE and there is a 21 day trial period (refer to the Registration
section for details on how to receive your personal registration code).

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
Normally your In mailbox will have certain symbols and your Out mailbox will
have others.  But since you can move messages around (such as an archive
mailbox), you can end up with any symbol in any mailbox.  Mail arriving from
the Internet will be placed in the In mailbox and any queued mail to be sent
to the Internet must reside in the Out mailbox.

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
is "System-Time: " and indicates when this message was initially processed
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
headers which take up lots of space.

You can move a selected message to another mailbox with <Psion-m>.

The "Send again" command <Psion-q> allows you to create a new message with
the body of the message being the same as the selected message.  If you use
this command on an outgoing or queued message, you will be asked if you
wish to keep the current message headers (ie. To: Cc: Bcc: Subject: etc.).
This will save you from re-entering this info if you just want to "edit"
the message body.

<Psion-e> will allow you to edit any mail message.  However, the length of
the headers and the message itself may not increased.  An error message will
be displayed if you do and the changes will be discarded.

Press <Psion-t> to send/receive your email via the Internet.  Here you will
be able to define which ISP to connect to via "TcpIp setup".  After a
connection has been made, you will see the "Transfer mail" menu which allows
you to send and receive your email.  You can also update your setups from
this menu. NB: not all TcpIp revisions take effect immediately but will on
your next attempt to connect.

The command <Psion-u> will "un-attach" a MIME attachment which is any of the
following formats: text, quoted-printable, Base64.  The UUE format is also
recognized. If one exists, it will be converted and written to the file you
entered or accepted in the directory \emailit\attach\ (will be created if it
does not currently exist).  The attachment will be replaced by a message
indicating where it was written to. To recover the used disk space, use
<Psion-k> to compress the mailbox.

When you are preparing your msgs, you can add an attachment to a msg by
setting "Attach file?" to "Yes" and following the filename dialog box to
the desired file.  If you wish a "return receipt", set the "Receipt type"
field to the type of receipt(s) you wish to get back.  eMail-It will add a
"X-Request-Receipt" header to the msg so you can tell which msgs generate
receipts.  Delivery Status Notifications (DSNs) are generated by the
Internet Mail System while Message Disposition Notifications (MDNs) *may*
be generated by the email software the recipient is using at the time.
NOTE: Within the framework of today's Internet mail, DSNs and MDNs cannot
be relied upon as a guarantee that a msg was or was not seen by the
recipient.  Even if they are not actively forged, they may be lost in
transit.  Please note that eMail-It does not generate "MDN receipts" for
incoming mail.


Nicknames
~~~~~~~~~
eMail-It allows a user to define an external phonebook using <Psion-b> to
allow lookups of email addresses, or the user can also use eMail-It's own
database to save email addresses and create mailing lists.  NB: eMail-It
will not update the external phonebook.

When a msg is being created (new, re-send, forward, etc), a "Nickname
Lookup" dialog box will appear.  By using this box, you can have eMail-It
access your external phonebook or the nickname database to retrieve stored
email addresses and place this in the To, Cc or Bcc msg header.  If you
wish to lookup another name next, set "Last lookup?" to No and eMail-It
will show this dialog box again.  If the lookup is via the nickname
database, you can set the "Address to" field to "nickname" and
eMail-It will place just the nickname in the To, Cc or Bcc msg header.

eMail-It's own nickname database is where you can maintain some/all of your
email addresses.  A skeleton database has been included with the zip file
and has the following titles:
  Nickname:  a short name you can easily remember ie. jb
  Address:   the actual email address ie. joeblow@isp.com
  Name:      the real name of the person ie. Joe Blow

You can also use the nickname file to create mailing lists such as:
  Nickname:  ml1
  Address:   joeblow@isp.com, myfriend@aol.com, suzy@virgin.uk.co
  Name:      Mailing List 1
To use this mailing list, you would address the message To: ml1 or Cc: ml1.
eMail-It will translate "ml1" into the 3 real email addresses but the To: or
Cc: header will remain unchanged.

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
1) Unzip the emailit.zip file.
    -> You can use the install.ctl file contained in the zip to install the
      10 files listed below.  Note: you also need an install utility program
      to use this file.
    -> OR you can move the files listed below in their appropriate
      directories on your Psion, Siena or Workabout.   These directories
      can exist on any drive EXCEPT \emailit\ should *not* be a flash disk.
      The files marked with an '*' must exist for the system to function,
      the others are optional.

     *  emailit.opa  --> \app\
        emailit.hlp  --> \app\emailit\
        emailit.pic  --> \app\emailit\
        irobex3.bin  --> \app\obex\
     *  tcpip.api    --> \net\
     *  tcpip.bin    --> \net\
     *  tcpip.ovl    --> \net\
        emailit.nic  --> \emailit\
        sample.toc   --> \emailit\
        sample.mbx   --> \emailit\

   eMail-It will create the nickname file (emailit.nic) when you press
   <Shift><Psion><A> if it does not already exist, but you will have to
   change the titles yourself.  eMail-It does not care what the titles are;
   they are there for your use only, but the first 3 fields are used based
   on assumed contents; (Nickname:, Address:, Name:).
   
   IrObex3.bin is a program that will send (and receive) data to other IR
   enabled devices that support the IrDA OBEX Put protocol.  eMail-It does
   not use the receive portion of this program.

2) Install emailit.opa to the system screen with <Psion-i>.
3) Run the application.  The system will create the following files:
     \emailit\emailit.cfg     eMail-It configuration file
     \emailit\emailit.log     optional log file of msgs sent and received
     \emailit\emailit.sig     optional signature file
     \emailit\xxxxxxx.toc     mailbox file (table of contents)
     \emailit\xxxxxxx.mbx     mailbox file (messages)
     \net\tcpip.cfg           TcpIp configuration file
     \net\tcpip.log           optional session log file

If you are currently using an older version of eMail-It, it is best if you
re-enter all your setup data again since much has changed in the config file
layouts and locations for eMail-It and TcpIp.  If this is not feasible,
contact the author for a conversion program.



Mail setup
~~~~~~~~~~
When you first run eMail-It, you will be asked to create a configuration
file and the system will then prompt you for some information.

We will use the following scenario to show what info goes where.  Let's say
your name is Davey Jones and your email address is: d.j.locker@yourisp.com
Your internet service provider (ISP) has told you that your email password
is "SECRET", your SMTP Server is called smtp.yourisp.com and your POP3
Server is called pop.yourisp.com
Note: You will need the Domane Name Server address for TcpIp and this will
be given to you by the ISP.

The first screen allows you to set some preferences:
 - "POP3 server(s)" should be set to "Only one" unless you plan to define
   more than one.
 - "Map accented chars" refers to how incoming msgs containing accented
   characters are to be mapped/translated for use on your SIBO machine.
   You can set this to "always" or "based on headers".  NB: Some email pgms
   do not produce the correct headers for msgs containing accented chars.
 - Where eMail-It is to look for your SMTP details (email address & server)
   If you are using only one ISP, place your SMTP details in "Mail setup".
   If you need SMTP Authentication to login to your SMTP server, you will
   need to place this info in the ISPs SMTP setup.
 - Which type of "editor" to use
 - Message width:  This value is used to limit the width of any msg being
   queued. eMail-It will add CrLf(s) where appropriate.
 - Whether you want to keep a log file to log your sent and recd messages

The next screen maintains your POP3 server details (POP3 configuration). You
can define up to 25 different POP3 servers to eMail-It.  This screen allows
you to add, delete, edit and use different POP3 server details.  eMail-It
requires at least one POP3 server to be defined and "locks" the last one
used so it cannot be deleted.

If on the previous screen you wanted to edit or add a POP3 server, the
"POP3 server details" screen will be displayed for entering your data.
  POP3 server       pop.yourisp.com
  User name         d.j.locker
  Password          SECRET
  Use APOP          If you know that your POP3 server supports this "secure
    login method", set this to Yes.
  Messages on server  If you use a PC to normally get your mail and the
    Psion while travelling, you might want to set this to Leave so that you
    can retrieve all your messages with your PC later. If you set this to
    Delete, the message will be deleted from the server after it has been
    downloaded to eMail-It.  Setting this to Prompt, you will be prompted
    for which messages on the server to examine, and for each message, what
    to do (Download Y/N, Delete Y/N, Receive in Full/Truncate).
  Large messages     Some messages can be quite large and memory on the
    Psion quite limited. If set to Truncate, large messages will be truncated
    after a set number of message lines are downloaded from the server.
    If "Messages on server" has been set to Delete or Prompt, details of the
    truncated message will be displayed on the screen and you will be
    asked if you wish to delete it from the server.
  Truncate limit (lines)  This is the number of message lines (excluding
    headers) to be downloaded of a BIG message.
  Per message limit (K)   You define what a BIG message is. The default 10K.

A note about your password: eMail-It stores them encrypted and are limited
to 16 chars.

When you have completed your "POP3 configuration", the next screen will be
"Send mail settings".
 SMTP server       smtp.yourisp.com       (or defined in ISP setup)
 Email address     d.j.locker@yourisp.com (or defined in ISP setup)
 Your name         Davey Jones
 Reply-To address  The field is left blank unless you want a "Reply-To:"
   message header to be included in your outgoing messages.  This is used
   when you want to have any replies to your messages sent to a specific
   email address which is different than your email address you defined
   on this screen or in the ISP setup.
 Use signature     If you want to append a standard set of text to all
   your outgoing messages, set this to Yes. To create/edit your
   signature file, set this to Edit and the next screen will be the
   word processor so you can edit your signature.
 Time zone         This field is appended to the "Date:" header of the
   outgoing message. It is your time zone in the world relative to GMT.
   (Note: -0500 can be used for EST)
 Attachments       You have only 2 choices: Base64 and UUE.
   Base64 will be sent as a MIME multipart/mixed message. UUcode will be
   appended to the original msg in Unix compatible format.  The UUcode format
   was a popular format before the introduction of MIME and Base64, but has
   since become less popular.

If on the Preferences screen you told eMail-It to look for your SMTP details
in the ISP setup, you will not be able to enter your SMTP server and email
address on the above screen.  You will enter this data via TcpIp setup (see
below).  The reason why you have a choice as to where you define your
SMTP details will be apparent if you use multiple ISPs, each with their own
SMTP server.  In days gone by, Internet mail required no password/security
to use SMTP servers.  But now we have Spam (junk email) and owners of these
SMTP servers are placing restrictions on their use; hence why the choice.
eMail-It supports SMTP Auth-Login, but this information can only be defined
in the ISP setup.



TcpIp setup
~~~~~~~~~~~
TcpIp setup is done when you initiate the "Transfer mail" facility <Psion-T>.
TcpIp will show a menu from which you can choose "TcpIp setup".  The TcpIp
setup dialog box allows you to change your ISP, Port or Cache settings,
and set the debug level.


You can define 25-50 different ISP setups depending on how long your ISP
names are.  The "ISP settings" dialog box works the same way as the
"POP3 configuration" dialog box described in Mail setup.  You can add,
delete, edit and use/Connect ISPs.  TcpIp requires at least one ISP to
be defined and "locks" the last one used from being deleted.  If you add
or edit an ISP, a screen will appear allowing you to enter data.  By setting
"Add SMTP details" to Yes, the next screen will allow you to enter SMTP
details, including SMTP Auth-Login information.

You can change the ISP name on the data entry screen but the name must be
more than 1 char in length and have no commas in it. The info about
passwords given in Mail setup applies here also.

The 4-digit Domain Name Server number, sometimes just referred to as DNS,
can be obtained from your ISP if you do not know it.  You will see an error
message if you enter this incorrectly and will be reset back to "0.0.0.0".
"Freeserve" in the UK tell you that "Domain name service is dynamic - you do
not need to set this", but you will have to search their website for these
DNS numbers; look in the Mac section.  You must enter a valid DNS in order
for TcpIp to obtain "Internet machine addresses".

The login script is very basic.  You send something, you wait for something,
you send something, you wait for something.  The somethings are CASE
sensitive and are separated by one or more spaces.  Everything sent is
terminated by a carrage return if one does not exist already.  When the
logon script has completed, the message "Logging onto the Internet..." will
be displayed.

There are 5 keywords:
  ph$  the phone number
  id$  the userid/name
  pw$  the password
  sp$  special char encoding
  wt$  wait
Use a keyword when you want to send that data.  The sp$ keyword is followed
by the 3 digit decimal code of the char you wish to send.  This keyword can
be used over and over to send a string of chars.  ie. sp$032Asp$013 will
send the space char, the letter "A" and then a carrage return.  The wt$
keyword is followed by the 3 digit decimal code indicating how many seconds
to wait before continuing with the script.  For example, wt$005 will cause
the script processor to wait 5 seconds before continuing.

The script is limited to 250 chars and initially set to:
   ATZL0E0 OK ph$ : id$ : pw$
ie. send a modem initialization string to the modem and wait for OK to be
received back.  Then send the phone number and wait for a colon to be
received. Then send the user name and wait for a colon, then send the
password.

The biggest hurdle to get eMail-It to work is getting connected to your ISP
using the correct script.

If your ISP is using PAP/CHAP, then the following script may work:
   ATZL0E0 OK ph$ CONNECT
   
It may help if you logon to your particular ISP using a "terminal" to see
what your ISP dialog is in order to create the correct script.

If you see a username prompt, use some part of this prompt to create that
part of your script. For example, the ISP may send a stream of info ending
with "Username".  So tell the TcpIp to wait for some characters such as
"name" and then send your userid.  The script to use would then be:
   ATZL0E0 OK ph$ name id$
but this will not be the final script since your ISP will next send a prompt
for your password. Your final script may end up being:
   ATZL0E0 OK ph$ name id$ word pw$
If you get a frame error while logging on, try placing the Tilde char ~
at the end of the script preceeded by a space.  One or two frame errors at
the beginning is alright, but lots of them means some form of setup error,
possibly with the port settings.

When you see "Logging onto the Internet...", your script has completed and
PPP Link is now going to try and "talk" to your ISP.  If you get
"PPP protocol bad", your ISP is using a protocol that the TcpIp does not
recognize and you MAY not be able to connect.  If for any reason the TcpIp
cannot "talk" to the ISP correctly, you will get the message "Connection to
ISP failed".

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
 Example 1:  atzl0e0 OK ph$ name: id$ word: pw$ ~
 Example 2:  ATZ OK ATL0E0 OK ph$ gin: id$ word: pw$ ocol: PPP
 Example 3:  AT&F OK ATZL0E0 OK ph$ CONNECT sp$013 : +CIS ID: id$
             NB: append the following to your ISP id in the User Name field
                  +/NOINT/PPP:CISPPP\pw$
             The pw$ will be replaced by your real password when id$ is sent.

   For Compuserve International Users, here is a script:
      ATZL0E0 OK ph$ CONNECT wt$003 sp$013 e: CIS D: id$ wor pw$
   and append the following to your ISP id in the User Name field
      /go:pppconnect
   so that your User Name looks something like   12345,6789/go:pppconnect
   This connection should work better if the Port is set to 7 bits,
   Even parity, 1 stop bit and use the following script:
      ATZL0E0 OK ph$ CONNECT wt$003 sp$013 Name: CIS ID: id$ word: pw$

Another sample script is the following which used an ELSA Microlink 28.8TQV
Modem to connect to the "Blue Window" ISP in Switzerland.  The ISP timeout
field was set to 40.  This ISP uses the CHAP/PAP authentication method for
userid/password validation.
  at&f\v8 OK ats7=60s30=0l0m1\n3%c3\q3x4 OK ph$ CONNECT

What commands you send to your modem depends what modem you have, but there
are some standard things such as ATZ (reset) and AT&F (load factory
settings).  Modem commands may work in lower case, but the responses are
normally upper case.


The port settings will be used with any of the ISP setups since these
settings pertain to your modem and communications port. IrDA has been added
to the Port list, but it is NOT functional yet.  "Ignore DSR" should be
left as "Y" for most modems, however the Psion Travel Modem requires this
set to "N".  Setting the handshake to RTS may result in faster comms, but
this will ultimately depend on the ISP; (this deals with ACCM).

  Std Modem Settings: Psion Travelmodem    Others     Compuserve Int'l
     Baud rate:          38.4k             depends       depends
     Parity:              none              none          even
     Data bits:            8                 8             7
     Stop bits:            1                 1             1
     Handshaking:         RTS               RTS           RTS
     Ignore DSR:           N                 Y           depends


TcpIp maintains a cache of DNS lookups in order to speed up processing.  You
can manually enter cache entries if you know the correct addresses.
Depending on your environment, this cache may result in not being able to
connect to a server if the server has "moved locations" on the Internet
since the last DNS lookup. TcpIp will display "TcpConnect: server RST"
in this case.  If you are experiencing problems, set "Use cache" to No.
TcpIp will delete this cache monthly.

The Debug level has 4 settings; if you select "file", the file
\net\tcpip.log will be created to record the modem traffic.  Useful for
finding out why you cannot connect to your ISP, but will slow your
transmission rate greatly.  Do not be alarmed by some of the diagnostic
messages that will be displayed;  "Bad Seq #" will occur and this is normal
Internet traffic.


Known Limitations/Bugs
~~~~~~~~~~~~~~~~~~~~~~
1) Most setup fields are limited to 40 chars, passwords 16 chars. The
   Reply-To: header can be 80 chars.
2) Only one attachment can be placed on a message.
3) Only one To: header is allowed and is limited to 250 US-ASCII characters.
4) Only one Cc: header is allowed and is limited to 64 US-ASCII characters.
5) Only one Bcc: header is allowed and is limited to 250 US-ASCII characters.
6) Multiple addresses are separated by a comma and one or more spaces.
   Use a comma ONLY to separate email addresses.  Do not use a comma in a
   way such as  To: "Smith, Joe" <js@isp.com>  it will not parse correctly.
7) You can place nicknames in the To:, Cc:, Bcc: headers, but be aware of how
   eMail-It translates nicknames to actual email addresses using eMail-It's
   nickname database.  See the Nicknames section for details.  The "nickname
   address" field MUST be less than 250 chars and this can be easily checked
   by seeing that the "Name" field is correct when you do a nickname lookup.
8) The max size of a queued msg (excluding an attachment) is approx 30k.



Special Thanks To
~~~~~~~~~~~~~~~~~
* Info on UUcode came from Jochen Siegenthaler (NZ).  Jochen's critique of
  the package's user interface has given v2.1 an impressive face-lift.
* Andrew Gregory (Australia) for info on the Workabout and many "OPL
  programming secrets".
* The initial design of the startup graphics screen was taken from the works
  of Adam Dawes.
* eMail-It's icon is from Matt Gumbley (UK).
* Alan Clifford (UK) and Mike McConnell (UK) for suggesting that the
  *original* eMail-It (pre v1.0) would be of value to Psion users!
* Gian Mario Weiss (Switzerland), Tapio V„is„nen (Finland), Tom Butz
  (NZ) and many others for Beta testing various versions of eMail-It
  and TcpIp.
* And all the users who keep sending in suggestions for improvements!



Legal Stuff
~~~~~~~~~~~
eMail-It v2.1 is offered as freeware, TcpIp v2.1 is SHAREWARE having a trial
period of 21 days, and all software included in the zip file is copyrighted
by Mark Richardson 1999 - 2003.  By using TcpIp v2.1, you agree to the terms
set out in the Licence Agreement (licence.txt) included in the zip file.

I cannot be held responsible for any loss of data or damage caused to you,
your machine or anything else in near proximity caused directly or
indirectly by eMail-It or its associated programs.

If you redistribute this package, it *MUST* be done by way of the
*UNALTERED* .ZIP file which you SHOULD have received in the first place!

Reverse engineering of eMail-It's component parts as well as TcpIp is
STRICTLY FORBIDDEN!

etc, etc, etc   ;-)


Registering TcpIp
~~~~~~~~~~~~~~~~~
TcpIp is SHAREWARE and is not free software.  By installing, copying, or
otherwise using TcpIp, you agree to be bound by the terms of the agreement
spelt out in licence.txt.  If you do not agree to the terms of the
agreement, you are not authorized to use TcpIp.

To obtain your personal registration code, please send an International
Money Order (or even cash via secured mail delivery) in the amount of 30 US
dollars or 20 Pounds Sterling (denominated in those currencies) to the
following address:
  Mark Richardson
  86 Gloucester St
  Suite 1405
  Toronto, Ontario
  CANADA  M4Y 2S2

Payment may also be made via PayPal to the following email address:
   emailit@webbox.com
Note:  Please contact the author first to check if the above addresses are
still valid!  Site licences are also granted.

You may obtain a temporary registration code to use while your payment is in
transit by contacting the author by email.  Your personal registration code
will be emailed to you when the payment has cleared the banking system.


Mark Richardson
April 29, 2003
email: mark@alumni.uwaterwloo.ca

