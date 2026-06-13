IrTranP OPM - Infra-red Tranfer of Photos
-----------------------------------------
Copyright (c) 2001-2003 by Andrew Gregory

Date              Version Notes
 7 April  2003    1.01    Added "ARB" image size support.
26 August 2001    1.00    First 'official' release.

Introduction
------------
OPMs are pre-written and compiled OPL modules which can be shared
between one or more applications.

The advantage of OPMs are numerous; they have already been written and
tested, thus saving the developer time; they can be used by more than
one application, so common code shared between applications is not
replicated, thus saving memory. 

There can be only 8 modules loaded at a time.  However, modules may be
loaded and unloaded from memory at will. 

Purpose of this OPM
-------------------
The purpose of this module is to allow developers to write programs
that can receive photos from digital cameras via infra-red using the
standard IrTran-P protocol.

Eventually, this OPM will be extended to allow sending of photos.

Dependencies
------------
IrTranP requires the SCOMMS.OPX available from Symbian. SCOMMS.OPX is
included inside the SIS file.

Module ID
---------
Each OPM has a unique name and ID - the name of the .opm file within the
\System\Opm\ directory of either the C: or D: drive. 

The IrTranP OPM is called: IrTranP
The IrTranP OPM UID is: &100099D5

Before Use
----------
If you are using the DECLARE EXTERNAL command to check your routines,
then you will need to copy the IrTranP.omh file to the \System\Opl\
directory on either C: or D:.  You also need to put the following line
at the top of your code: 

  INCLUDE "IrTranP.omh"


Coding with the IrTranP OPM
---------------------------

REM Assumes module has been loaded.
REM Receives a photo to "C:\Test.jpg", asking for VGA (640x480) resolution
REM and a maximum size of 32768 bytes.
PROC doTran:
  LOCAL hdl&, ret&, stat&, pid$( 255 )
  hdl& = IrTranp_Receive&:( "C:\Test.jpg", "VGA", 32768, ADDR( stat& ) )
  BUSY "Start transmitting the picture..."
  WHILE 1
    IOWAIT
    IF stat& <> KStatusPending32&
      ret& = IrTranp_Handle&:( hdl& )
      IF ret& = 0
        IF pid$ = ""
          pid$ = IrTranp_GetProductId$:( hdl& )
          IF pid$ <> ""
            BUSY "Receiving from " + pid$ + "..."
          ENDIF
        ENDIF
        GIPRINT GEN$( INT( IrTranp_GetComplete:( hdl& ) * 100 ), 9 ) + "%"
      ELSEIF ret& = KE32ErrEof&
        BUSY OFF
        PRINT "Transfer complete"
        BREAK
      ELSE
        BUSY OFF
        PRINT "Transfer failed, Error #" + GEN$( ret&, 9 )
        BREAK
      ENDIF
    ENDIF
  ENDWH
  PRINT "Press a key..."
  GET
ENDP

Available procedures
--------------------
IrTranp_Receive&:( filename$, size$, maxfilesize&, pstat& )
  Initiate receive of a picture. Note that the photo size (resolution)
  and maximum file size are merely hints to the camera - the camera is
  allowed to ignore them and do its own thing! This is a 'feature' of
  the IrTran-P protocol, and not a limitation of the IrTranP OPM.

  Returns a handle to be passed to other IrTranp OPM procedures.

  filename$ is the filename to store the picture in
  size$ specifies the size of the picture to receive:
        "QVGA" =  320 x 240
        "VGA"  =  640 x 480
        "SVGA" =  800 x 600
        "XGA"  = 1024 x 768
        "SXGA" = 1280 x 960
        "ARB"  = arbitrary (unrestricted)
        "m,n"  =    m x   n (eg. "512,384")
  maxfilesize& is the maximum allowed file size in bytes. A good argument
     here would be the free disk space :-) Also, treats zero as 'no limit'.
  pstat& is a pointer to an I/O status word


IrTranp_Handle&:( hdl& )
  Handle ongoing picture receive events.

  hdl& is the value returned by IrTranp_Receive&:
  Returns:
     KE32ErrNone&     (  0) while receive progresses
     KE32ErrEof&      (-25) when the receive is complete
     KE32ErrNoMemory& (- 4) if the receiver ran out of memory
     KE32ErrInUse&    (-14) if the IR port was in use (turn off your remote link!)
     KE32ErrTimedOut& (-33) if a comms error occurred

IrTranp_Cancel:( hdl& )
  Cancel a receive.
  hdl& is the value returned by IrTranp_Receive&:


IrTranp_GetProductId$:( hdl& )
  Get the product ID of the other device. Only becomes available as the
  transfer proceeds.
  hdl& is the value returned by IrTranp_Receive&:


IrTranp_GetComplete:( hdl& )
  Gets the proportion of the transfer that has completed. Returns a
  floating point value that ranges from 0.0 to 1.0. It is not guaranteed
  to return 1.0 when the transfer is complete.
  hdl& is the value returned by IrTranp_Receive&:


IrTranp_GetFilename$:( hdl& )
  Gets the photo filename sent by the camera. Check this repeatedly
  during the transfer until it gets set.
  hdl& is the value returned by IrTranp_Receive&:
