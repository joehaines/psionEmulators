_______________
nConvert 1.1

EPOC file converter suite
_______________
Program Details

Name         	: nConvert
Version      	: 1.1
Release date	: 03/05/01
Copyright   	: NEUON
Email  	 	: nconvert@neuon.com
URL	       	: http://www.neuon.com/

Installation notices
--------------------

If you are upgrading from version 1.00 of nConvert, you will see an error displayed when
starting the installation of this new version. Please ignore it as it will cause no harm, it
is due to a problem with the current versions of the Symbian installer program.


Questions and Answers
---------------------

Q: What can I do if I find a file that won't convert? 
A: The most helpful thing you can do, is mail it to nconvert@neuon.com,
   and we will then analyse the problem, hopefully integrating a fix into
   the next nConvert release. 

Q: I'm interested in developing a converter plugin for nConvert. How do
   I go about doing that? 
A: At present we have distributed the preiminary nConvert SDK to a selected
   few individuals. In due course we will make this available to the development
   community at large. We have always been an active development stable
   (see our OPX's) and fully intend to continue that tradition with the release
   of nConvert. 

Q: Why can't I see my embedded images that nConvert now supports? 
A: Embedded object conversion is currently supported by the outgoing
   (i.e. from EPOC) Word/Jotter converters. These are capable of transforming
   any embedded objects (e.g. Sketch, Sheet, etc) into JPEG files which are either
   embedded inside your RTF file (in the case of the EPOC Word -> RTF converter)
   or added as local image links (in the case of HTML). If you converted your
   document to RTF, then you must ensure that you are using a version of MS-Word
   newer than 95. If you don't have this luxury, then we suggest that you download
   the MS-Word viewer from Microsoft's web site. nConvert also supports the conversion
   of the embedded images inside RTF files back to EPOC Sketch files, and again,
   you need to have the JPEG library installed.

Q: The nConvert documentation says it supports GIF files. Why can't I seem to 
   convert GIF files using my version of nConvert?
A: You need to download the free nConvert GIF plugin from our website. See the 
   nConvert web page (http://www.neuon.com/apps/nconvert/) for the download file.

Q: Am I paying for nConvert or the plugins?
A: You are paying for the plugins and the underlying conversion API. The nConvert
   UI can be replaced completely if needs be, but its debatable whether we would
   make all the necessary nCnvAPI header files publicly available to allow this.

Q: I have a suggestion for a plugin. What should I do?
A: Drop us a line with your suggestions (nconvert@neuon.com) and we'll see what 
   we can do.

Q: I would like to license nConvert for a large batch of machines. Is this possible?
A: Yes, but please send us an email with details of the number of machines (and hence
   number of licenses) you require.

Q: nConvert (versions before v1.00) didn't work in conjunction with the ER1-3 
   service pack (as made available by Psion). Has this problem been resolved in 
   version 1.01?
A: We hope so. We've done as much as we can to ensure that this won't be a problem
   anymore. If it still doesn't work, the only advice we give you is to suggest 
   you remove the service pack from your machine.

Q: What do I need to do when I am upgrading from an earlier version of nConvert?
A: Nothing - nConvert will ensure that all necessary files are updated. 

   NOTE: if you are a registered nConvert user, please take note of your username
   and password before installing v1.01. After nConvert v1.01 has been installed
   you will be required to re-enter your registration details.

Q: My previous demonstration version of nConvert has expired. If I install a newer
   version, do I get another chance to evaluate it?
A: Yes, you should find that you're once more allowed another 30 days to try the
   application. If you experience any problems with this feature, please contact
   us using nconvert@neuon.com.

Q: How long has it taken you to write nConvert?
A: A long time! We've been working on nConvert since about 9 months ago. It's
   a very big project, with around 1000 source files of various types. We hope
   to make a white paper available on the more interesting aspects of nConvert
   in due course.



3rd part developers - nCnvAPI Library 
--------------------------------------

If you are interested in providing an nConvert plugin for your application, then
please register your interest by emailing nconvert@neuon.com. We will eventually
make a public SDK available, but in the mean time, feel free to drop us a line.

_________________________
Installation instructions

1. Unpack the .zip file to your PC or your machine.

2. On the PC:
    Double click on the file nConvert.sis.  This will start the 
    EPOC Install installation of nConvert.

3. On the machine:
    Double tap on the file nConvert.sis This will start the PC
    installation of nConvert.

4. nConvert will now be accessible from the Extras bar. 


____________
Registration

To register:
1.  Sign on to the Neuon Web Site at

      http://www.neuon.com/
      and follow the instructions there,  or

2.  Follow the instructions in the help file

_________________
Conditions of use

The software is protected by copyright law and international treaty 
provisions. You acknowledge that no title to the intellectual property
in the software is transferred to you. You further acknowledge that 
title and full ownership rights to the software will remain the 
exclusive property of Neuon, and you will not acquire any rights to 
the software except as expressly set forth in this license. You agree 
that any copies of the software will contain the same proprietary 
notices which appear on and in the software.

We do not warrant that the software is error free. nConvert has been
tested extensively and is free of known problems. There is no liability
for consequential damages. In no event shall Neuon be liable to you for
any consequential, special, incidental or indirect damages of any kind 
arising out of the delivery, performance or use of the software, even 
if we have been advised of the possibility of such damages. 

________________________________________________________________________
nConvert is copyright (c) Neuon 1999-2001
