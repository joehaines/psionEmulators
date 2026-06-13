OPP SDK 1.0 for the Psion range of handhelds
Copyright 1997, Andy Clarkson, All Rights Reserved

Description:

	The OPP SDK includes the following two components:

		OPP 1.7F
		OPPDBG 1.1

	OPP is an OPL pre-processor for the Psion OPL language. If you are
	familiar with the operation of a C pre-processor then the function of
	OPP will be immediately clear. S3ATRAN provides similar facilities
	when translating OPL code on a PC. OPP provides the same facilities
	as S3ATRAN plus a large number of additional features. These
	additional features provide extensions to the OPL language, such as
	support for multi-dimensional arrays and C style structures. Also,
	whereas S3ATRAN only runs on a PC, there are both PC and Psion
	versions of OPP.	

	OPPDBG is a run-time OPL source level debugger for the Psion 3a/3c
	and Siena computers. It may be used when developing applications
	on the Psion in the OPL language.

Contents of OPP directory:

	opp_man1.txt	- user guide (part 1), DOS ascii text format
	opp_man2.txt	- user guide (part 2), DOS ascii text format

	opp.als		- Psion Word alias file for OPP files
	oph.als		- Psion Word alias file for OPH files
	sys$prgp.img	- Psion based OPL preprocessor, OPP

	opp.exe		- MSDOS based OPL preprocessor, OPP

	include/*.oph	- optional system include files

Contents of OPPDBG directory:

	dbg_man.txt	- user guide, DOS ascii text format

	oppdbg.opa 	- main application

	oppdbg.oph	- OPP include file

	oppdbg.hlp 	- on-line help file (optional)
	oppdbg.ttl	- title screen (optional)

Manual format:

	The OPP and OPPDBG manuals are contained within the files
	opp_man1.txt, opp_man2.txt and dbg_man.txt. The manuals have been
	split so that they can be read using the Psion Word application
	in "Text" mode. An on-line WWW version of the manual is also
	available from my home page at:

		http://ourworld.compuserve.com/homepages/andyc

OPP Quick installation:

	Psion:
	1) Copy opp.als and oph.als to \APP\ on any drive.
	2) Copy sys$prgp.img to \IMG\ on any drive.
	3) Install opp.als and oph.als using psion-i from the
	   Psion system screen.
	4) Optionally copy all the oph files from the include
	   directory to \OPP\INCLUDE on any drive.
	5) Refer to the user guide for further details.

	MSDOS:
	Refer to manual for full details.

OPPDBG Quick installation:

	1) Copy oppdbg.opa to \APP\ on any drive.
	2) If you want on-line help copy the file OPPDBG.HLP into
	   the \APP\OPPDBG\ directory.
	3) If you want a startup screen copy the file OPPDBG.TTL
	   into the \APP\OPPDBG\ directory.
	4) Install the OPPDBG.OPA using psion-i from system screen.
	8) Refer to manual.txt for further details.

I can be contacted via email at:

	andyc@compuserve.com

Note this software is shareware, refer to the manual for further
details.
