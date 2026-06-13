The vr3a application displays text files on the s3a in Vertical orientation.
 
It is a type $9003 application for the Series 3a only, it uses
files with a .TXT extension in VR directory, multiple files can be opened
but new files cannot be created. Text is displayed in one of eight fonts
with a choice of mono- or proportional spacing. Lines which are too wide
for the display can be truncated or wrapped according to preference.
The current position in a file, along with font and display mode
information, is automatically retained between invocations. The reader can
advance through the file a line at a time or a page at a time, jump to a
specified line or search for a line contining a specified text string.

To install place vr3a.opa in the /APP directory and vprint.opo in the 
/APP/LIB directory and use <psion-i> from the system screen.

The following commands are available

<Space> - scroll one page

<ctrl-menu> - toggle display of status window

<Help> - very brief help.

Via menu or hot keys the following commands are provided.

<psion-o> - Open another file. The current file position and preferences
            will be saved before the new file is opened. 

<psion-b> - Review Bookmarks

<psion-v> - About  Vr3a, version info

<psion-c> - Count lines in the current file

<psion-f> - Find text string in file. Search can start from the current
            position, ie the bottom of the current page or from the
            beginning of the file. Backwards search is not supported.
 
<psion-g> - Find again. Continue search for next occurrence of the 
            specified text string.

<psion-j> - Jump to specified line in file.

<psion-q> - Set preferences. These include :- Font(Swiss/Roman 8,11,13 or 15), 
            Character widths(proportional/monospaced), Display mode
            (Line truncate/Line wrap/Word wrap/Paragraph)

<psion-z> - Zoom in, increase font size.

<psion-shift-z> - Zoom out, decrease font size.

<psion-x> - Exit

Version 1.1 Enhancements.

1. Open file from within VR3A and multiple open files now work.
2. Kill application from system screen now works unless a Menu or
   Dialog is active.
3. Controls are now recognized while the screen is being updated.
4. The Next page is built while you read the one that is on the screen,
   scroll one line control has been removed.
5. The Set Preferences dialog now includes font size.
6. Review bookmarks dialog added for housekeeping of entries in vrinit.rc.

Ewan Paton - paton@vmark.co.uk - 21 Feb 94