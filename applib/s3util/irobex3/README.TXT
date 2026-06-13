IrObex3 v1.0F (Aug 15/04)
~~~~~~~~~~~~~~~~~~~~~~~~~
This software is designed as a generic communications facility by which
typed data objects can be sent and received.  It uses the OBEX IrDA
protocol.  The only level of OBEX supported currently is for the Put
operation.  It can act as both a client and a server.  In order to
implement the server, I had to write my own IrDA "Lite" Stack since the
Psion IR Stack was inadequate.  This software does not use the Psion IR
Stack, unlike my previous IrBeam3c program I released several years ago.

IrDA Object Exchange (IrOBEX) can be viewed as essentially "HTTP for
IrDA".  IrOBEX was designed to resemble HTTP, and it leverages what it
can from this internet protocol, adding capabilities that relate to the
unique environment or IrDA.  IrOBEX is best used where objects of some
kind need to be moved from one device to another.  For example, two
devices may exchange phone and address information, or calendar info
in vCard and vCal formats.  Or, a handheld scanner may capture a
graphics image and beam it to a laptop to manipulate.  The Palm OS uses
IrOBEX when "beaming" data.  Its built-in applications recognize file
extensions:
   txt      Memo
   vcf      AddressBook
   vcs      DateBook & ToDo
   prc,pdb  Launcher
Unless additional software on the receiving Palm device can recognize
other file extentions, the data being transferred will not be accepted
by the Palm OS as it does not know what to do with it.  My OBEX software
will accept anything sent to it and places the incoming file in \OPD\.

I have used this version for nearly 18 months for communications between
my Psion 3c and my Visor Handspring with no problems.  However, I *must*
set the Baudrate to "Slow" when I want to receive a file!  Never figured
out why there is a problem receiving on fast speed.

The following pertains mainly to transferring files to/from a Palm.
Files with extentions txt, vcf, vcs, prc or pdb can be sent as is.  If the
data being sent is *all* text data, set the "Type" to TEXT and the program
will do the required character mapping between the two platforms (Psion vs
other machine) to deal with accented characters.  Always send prc and pdb
files as BINARY.  If the file extension is not one of those listed above,
the Palm OS will most likely not accept the incoming file since it does not
know how to deal with it.  The file *will* be accepted if it is a pdb and
this can be done by setting the "Send format" to "Palm pdb" on the "Send
file" dialog box.  I do this to do quick backups of my Psion.
For those that use the DOC file format for large text files, I have
included a conversion utility to convert text files into the DOC file format
which you can then send *as is* since the resulting file is a pdb file.
Incoming pdb/prc files can be converted to files for the Psion using the
"Palm DB --> Psion" conversion. Please note that this utility basically
strips off the Palm header, but if it is a DOC file, the data will be
converted to a text file.


INSTALLATION
~~~~~~~~~~~~
(1) Move the following files to their correct directories.  These
    directories can exist on any drive.  Not sure about which drive
    the received files will go when the default \OPD\ directory is used.
      IrObex3.bin  -->  \APP\OBEX\
      IrObex3.opo  -->  \OPO\
    The opo program will become an opa later when I create an Icon.
(2) Run IrObex3.opo   There is a limited "help" facility if the <Help>
    key is pressed.


Technical Info on problems
~~~~~~~~~~~~~~~~~~~~~~~~~~
I have tested sending and receiving files up to 800K between my Psion3c
and my Handspring Visor Delux which runs Palm OS v3.1H.  I have found
a couple of irregularities with the OBEX Client on the Visor:
(1) When my Server tries to respond to the Client's CONNECT indicating
    the OBEX packet size my Server supports, which is 1K, the Visor
    hangs and a soft reset needs to be done to get it working again.
    I had to modify the "OBEX protocol" and respond with an OBEX packet
    size of 512 bytes in order that the Visor would not crash!
    Note, the actual byte figures above are 1021 and 509.
    It is interesting to note that the Visor OBEX Server responds with
    a 4072 byte OBEX packet size.  I really think this is a Palm bug.
(2) It appears the Client is unable to send a correct/complete IR frame
    unless the baud is 9600.  It sends about 350 bytes and how much it
    sends is not consistent.  When communicating with the OBEX Server,
    there is no problem as the bytes in its "response" frames are very
    few.  Perhaps the problem revolves around different IR data sizes;
    the Visor uses 512 bytes, I use 1K.  However, the IrLMP specs say
    each device negotiates its own data size.  I cannot remember now,
    but I believe this problem exists even when I used a 512 byte data
    size.
(3) Fluorescent lighting and strong direct sunlight appears to effect
    the IR beam and causes the IR link to "drop".  Actually, the LMP
    protocol drops the link.



Mark Richardson
August 15th, 2004
email: mark@alumni.uwaterloo.ca

