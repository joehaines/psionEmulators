              DBS Database server installation guide
              ======================================

If you unzipped DBS.ZIP using the -d flag, the following directories
will have been created:

    SERVER - DBS files common to both OPL and C
    OPLAPI - DBS API for using DBS from the OPL programming language
    CAPI   - DBS API for using DBS from the C programming language

The SERVER directory contains the DBS files which need to be copied
to a \DBS\ directory on any drive of the Psion machine. These are the
files which will need to be distributed with any application you produce
which uses DBS.

You may use and distribute DBS on a royalty free basis provided you
meet the conditions set out in the LICENCE.TXT file.



OPL Development
===============

The OPLAPI directory contains the files relevant to developing DBS
applications using OPL. These are:

    OPLAPI.TXT   - Reference manual for OPL API functions
    DATABASE.OPO - OPL API library module for the Series 3a
    DATAWA.OPO   - OPL API library module for the Workabout
    DATAS3.OPO   - OPL API library module for the Series 3
    DATAHC.OPO   - OPL API library module for the HC
    DBS.OPH      - OPL header file
    DATABASE.LEX - Language extension file for the OPL API functions
    DB.OPA       - Sample OPL application
    DB.OPL       - The source for the sample application
    DB.MAK       - An ODE .MAK project for the sample application
    DB.PIC       - The sample application icon
    CHECKVER.OPL - See below
    
The API library module, DATABASE.OPO, needs to be copied onto the Psion
machine, into the \OPO\ directory. If you are using ODE, then it can be
simply added to the project file and will be automatically copied across.

If you wish to run the server on a Series 3, HC or Workabout rather
than a Series 3a machine, then use the appropriate .OPO instead of
the DATABASE.OPO module.

CHECKVER.OPL contains a function ChckVer%: which may be useful to
application distributors to determine whether or not they need to
install DBS on any machine their application is installed onto.


C Development
=============

The CAPI directory contains the files relevant to developing DBS
applications using C. These are:

    CAPI.TXT    - Reference manual for C API functions
    DBS.LIB     - The C API library
    DBS.H       - The C API include file
    DB.IMG      - Simple C application
    DB.C        - C application source
    CHECKVER.C  - See below

The DBS.LIB should be placed in your C library directory, and DBS.H
in your include directory.

CHECKVER.C contains a function CheckVer which may be useful to
application distributors to determine whether or not they need to
install DBS on any machine their application is installed onto.
