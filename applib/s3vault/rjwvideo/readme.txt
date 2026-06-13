RJWVIDEO - VIDEO TAPE DATABASE MANAGEMENT SYSTEM
================================================

1. INTRODUCTION
   ------------

This program for the Psion 3a/3c/3mx implements a database of video tapes.

Each tape is identified by a one to three digit number and may contain up 
to eight television programmes.  The name of each programme is recorded as 
are the start and end positions on the tape (in hours and minutes) and the 
duration of the programme.

In addition to adding and deleting videos and programmes, facilities are 
provided for browsing videos and for searching for programmes and free 
space.

2. INSTALLATION INSTRUCTIONS
   -------------------------

Copy the executable file rjwvideo.opo to the \OPO\ directory on the default 
disk (usually the internal disk).  Rjwvideo will then appear under the OPL 
icon on the System screen.

Copy the initial database file rjwvideo.odb to the \OPD\ directory.  When 
the program is run the database will contain a single video (number 1).  
Other videos may be added as required and video 1 erased if necessary.

3. SUPPORT
   -------

Informal support is provided by Roger Ward by email at R.J.Ward@leeds.ac.uk

Bug reports and suggestions for enhancements will be gratefully received.

This program is public domain provided its authorship is acknowledged.

4. MENU COMMANDS
   -------------

Video menu
==========

This menu contains commands which act on a whole video.

Psion-N  New video
------------------
Function:
Creates a new video in the database
Parameters:
Number  - the identification number (in the range 1-999) of the new video.
Minutes - the length of the video in minutes (in the range 30-480).

Psion-O  Open video
-------------------
Function:
Displays the names of the programmes on the video together with their start 
and end positions on the tape and durations.
Parameter:
Number - the identification number of the video to be opened.

Psion-B  Browse video
---------------------
Function:
Uses the arrow keys to browse the video database, starting with the most 
recently opened video, then the next most recently opened and so on.

Psion-E  Erase video
--------------------
Function:
Erases the currently open video from the database.  A confirmation message 
is displayed.

Prog menu
=========

This menu contains commands which act on individual programmes on a video.

Psion-A  Add prog
-----------------
Function:
Adds a new programme to the currently open video.
Parameters:
Number - the number of the free slot (in the range 1-8) on the currently 
open video into which the programme is to be added.  The default is the 
first free slot.
Name - the name or description (maximum 29 characters) of the programme.
Duration - the length of the programme in hours and minutes.
Start time/End time - as an alternative to specifying the Duration the 
start and end times of the recording may be given.

Psion-U  Update prog
--------------------
Function:
Updates details of a programme on the currently open video.
Parameters:
Number - the slot number of the programme to be updated.
Name - the programme name may be edited.
Start time - the programme start time on the video tape may be increased 
(but not decreased), eg to remove any excess time before the start of a 
programme.
End time - the programme end time on the video tape may be decreased (but 
not increased), eg to remove any excess time after the end of the 
programme.

Psion-D  Delete prog
--------------------
Function:
Deletes the specified programme from the currently open video.
Parameter:
Number - the slot number of the programme to be deleted.  A confirmation 
message is displayed.

Search menu
===========

This menu contains commands for searching for programmes and free space.

Psion-F  Find prog
------------------
Function:
Opens the most recently opened video with a programme containing the 
specified name.
Parameter:
Name - The name of the programme to find.  The name may be the full 
programme name or a substring.  It is not case-sensitive.

Psion-G  Find next
------------------
Function: 
Opens the most recently opened video with a programme containing the name 
previously specified by Psion-F.

Psion-S  Find space
-------------------
Function:
Opens the most recently opened video with a free slot of the specified 
length.
Parameters:
Duration - the length of the free slot in hours and minutes.
Start time/End time - as an alternative to specifying the Duration the 
start and end times of the recording may be given.

Exit menu
=========

This menu just contains the Exit command.

Psion-X  Exit
-------------
Function:
Exits from the program.


Roger Ward
