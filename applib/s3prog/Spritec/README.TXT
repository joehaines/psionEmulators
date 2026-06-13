The file spritec.opl and the (rough and ready) test-code tspritec.opl
give an example of accessing sprites in Opl via functions normally
only available from C.

These procedures are much faster than using the OPL keywords because 
they access the file as little as possible.

The user needs to refer to the C SDK WSRV manual for full details of
the functions and the parameters they require.  However, spritec.opl
contains many comments to help the reader understand things.

spritec.opl provides procedures that call the Window Server directly
to:
- make multi-bitmap files in OPL. All the bitmaps for a sprite can be
saved in a single file.
- load them from the multi-bitmap file using the C routines gInitBit()
and gGetBit() 
- creating the sprite and changing it.

These are all the functions that the Window Server provides for
sprite handling.

Warning: the procedures can be abused by leaving lots of bitmap files
and bitmap drawables open.

Note: this Opl does not work on the S3 classic.
