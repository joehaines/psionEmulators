Notes on C programming examples of Link Paste
=============================================

Files in this zip (lpaste3.zip)
-------------------------------
readme.txt      This file
lp.h            Header file
lpclnt.c        Source code for example link paste "client"
lpserv.c        Source code for example link paste "server"
lpclnt.img      Built example link paste client
lpserv.img      Built example link paste server

Background
----------
See Chapter 14 in the "Object Oriented Programming Guide" in volume 3
of the C SDK for a general discussion about link paste clients and
servers.  Note: this discussion is in part in terms of object
oriented programming, but you may find the definitions of the various
link paste formats helpful, if nothing else.

(The "Object Oriented Programming Guide" can be downloaded, in W4W2
format, eg as the file OODOC201.ZIP in the FLIST of the psion/sibosdk
conference on CiX.)

What the files all do
---------------------
Copy the .IMG files to an \IMG\ directory on an S3 or S3a.
Find their names under the RunImg icon and press Enter on them to
start them running.

LpServ.IMG apparently does nothing.  But if you make it foreground,
and then task to another application that supports "Bring" and choose
the "Bring" menu command, the current time and date, as text, will
appear in that application.  LpServ.IMG provides this data to any
application that asks for it.

LpClnt.IMG asks you to press 'F' to, in effect, Bring data from
whichever application (if any) currently has any available.  It then
shows the name of this application, together with a selection of the
available "formats" that you can choose.  Make your choice and see a
hex dump of the data actually brought.

The two source files are reasonably well commented.

Differences from previous releases of these files
-------------------------------------------------
(Skip this section if you never had a previous version)

1. There is an unstated limit, in system code, as to how much data
can be "brought" in any one stage of the transaction.  This limit is
256 bytes.  The original LpClnt asked for 512 bytes at a time, which
could result in random damage.

(Note: this limit only applies when the data is fetched from an
editor, using one of the system defined formats.  If you are doing
some "native" link pasting with your own format, you can choose to
transmit data in larger chunks than this.)

2. In the "middle" stages of a link paste transaction, the link
client has to tell the server, each time, the maximum number of
characters it can handle at that time.  This value has to be passed
as the second word of the "arguments" structure, with the address of
the buffer to receive the character being the first word.

In the original LpClnt code, this second parameter was not being set
up correctly, so that a random amount of data was actually being
requested.

3. If the client wishes to terminate the transaction before the data
available from the server is exhausted (as can happen when the user
presses ESC midway through the hex dump in LpClnt), it is the FIRST
word of the argument structure that needs to be set to zero. 
However, the earlier code in LpClnt and LpServ treated the SECOND
word as the relevant one in this regard.  Result: system apps could
be left indefinitely hung up, in some cases.

4. There was some confusion between the TYPE values for the different
formats, as raw integers (1, 2, 4) and the MASK values formed by
shifting 1 left these number of times (ie 2, 4, 0x10).

In fact, the MASK values should only be used in communications with
the Window Server - when you specify which types you can supply, if
requested, whereas in the communication between the client and
server, the TYPE value should always be used (since you can only ask
for one type at a time).

5. LpClnt now handles the case of there not being any link paste data
available.

6. All explicit reference to the process control block (E_PROC data
structure) of the link client has been removed, and the explicit call
to p_iosignaln, passing the I/O semaphore for the application, has
been replaced by simply calling p_iosignal the requisite number of
times.

