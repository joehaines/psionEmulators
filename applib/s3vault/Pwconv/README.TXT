                              
                              
                   PsiWin translation API
                              
                              

Overview
  
  General
PsiWin includes a standalone file translation library. Using
the API to this library, a legitimate owner of PsiWin can
programmatically translate DOS and Windows application files
to appropriate Psion formats, or vice versa.
The file converters themselves are part of PsiWin and may
not be redistributed in any form.
This API information is supplied "as-is" and is unsupported.
Psion disclaim any liability for the use of this
information.
Sample 'C' code is provided in CONVERT.TXT, and BASIC in
CONVERT.BAS.
(The file format translation library can also be extended by
third-parties to support new DOS, Windows and Psion
application file formats. This is not described here.)
  
  Translation Paths
Each file format translator or translation path supports the
translation of files between one PC application file format
and one Psion native file format.  Applications that use
compound file formats, such as Lotus Organizer are supported
by more than one translator, each one handling one inherent
Lotus Organizer format, such as the Calendar and one Psion
native format, such as the Agenda.
  
  Terminology
The API uses the following conventions:
 App - PC Application
 Format - PSION Formats
 Path - Translation Paths
  
  Selecting a Translation Path
The selection of a translation path is accomplished by using
information about the files to be translated and the formats
they are to be translated into to select a unique
translator.  For example, to import a PC file, the
translation manager would:
 1.    Initialise the Translators - XMOpen
 2.    Set the translation direction - XMSetDirection
 3.    Get the translators to examine the file -
   XMSelectApp
 4.    The file may be recognised by more than one
   converter - e.g. there are two text formats (DOS and
   Windows have different character sets). If this is the
   case the application must select the appropriate format
   - use XMGetAppName and XMSetApp functions to do this.
 5.    At this stage the source has been uniquely
   determined. Now it's possible that the selected file
   could be converted to one or more formats. Use
   XMCountFormats and XMGetFormatInfo to find out what the
   choices are.
 6.    At last the translation Path can be uniquely
   determined. - XMSetPath
 7.    Check the transfer modes available to this
   translation path with XMGetTransferMode
 8.    Import the file and check the error return. -
   XMImport, XMGetLastErrorMsg
 9.    It's done - use XMClose to sign off.
Throughout the translator selection process, PsiWinXM
maintains three lists: one of possible PC applications, one
of possible PSION native formats and one of possible
translation paths.  If the source file and direction alone
are not sufficient to select a unique translation path, the
API can provide the names of remaining possible PC
applications and/or Psion native formats for the user to
make a selection.
Note: since at any point in the conversion process you can
obtain a list of the available translation paths, it is
possible to avoid steps 4 and 5 by listing the available
paths and picking one - using XMCountPaths, XMGetPathInfo
and XMSetPath. As, in step 4, the source app has been
chosen, there are the same number of Paths as Formats so the
Format number corresponds to the path wanted.
  
  Performing a translation
Once a translation path and result file name has been
selected, PsiWinXM is directed to run the translation,
either Importing to a Psion native format or exporting to a
DOS or Windows application format.  During the translation,
the translator will periodically callback to the Hosting
application to report translation progress and accept the
user's request to cancel the translation.  Should an error
occur during translation, the hosting application is
notified and can retrieve an error message from PsiWinXM.

API Calls
  
  Initialization
  HXM XMOpen(LPSTR psionName, LPSTR xlateFolder, HWND
  hParent)
XMOpen initialises a connection with the translation
manager.  PsiWinXM searches the indicated xlateFolder for
translator and translator preference modules.  The psionName
is used to track any preferences or past usage patterns by
user.  If psionName is NULL, a "Guest" user will be assumed.
hParent identifies a window to "own" any dialogs that may be
created by PsiWinXM's translators or preference components.
XMOpen returns a global handle to be used in subsequent
calls to the translation manager.  If the initialisation
fails for any reason, XMOpen returns a NULL handle.
  BOOL XMClose(HXM hXM)
XMClose terminates the connection with the translation
manager represented by the connection handle hXM and
releases any resources associated with the connection.
XMClose returns TRUE if the termination is successful and
FALSE otherwise.
  
  Queries
Often, the user must make a selection of Psion format or PC
application to work with.  The following query functions are
provided to support the generation of lists from which to
make a selection.
  int XMCountFormats(HXM hXM)
XMCountFormats returns the number of possible Psion formats
at this stage of the translator selection.
  BOOL XMGetFormatInfo(HXM hXM, int nFormat, PFINFO FAR
  *lpinfo)
XMGetFormatInfo fills in the PFINFO structure with details
about the nFormat'th possible Psion format.  This structure
includes the name of the Psion application as well as the
filename extension it uses and its default document
directory.
  int XMCountApps(HXM hXM)
XMCountApps returns the number of possible PC application
formats at this stage of the translator selection process.
  int XMGetAppNameLen(HXM hXM, int nApp)
XMGetAppNameLen returns the length of the bytes of the PC
application name corresponding to nApp.  nApp identifies one
of the pssible PC applications at this stage of the
translator selection process.
  int XMGetAppName(HXM hXM, int nApp, LPSTR appName, int
  nName)
XMGetAppName returns the PC application name corresponding
to nApp in the buffer appName up to a maximum of nName
characters.  XMGetAppName returns the length of the C string
copied to appName.
  int XMGetFilterLen(HXM hXM)
XMGetFilterLen returns the length of the COMMDLG filter
string representing the current set of possible PC
applications.
  int XMGetFilter(HXM hXM, LPSTR filterBuf, int nFilter)
XMGetFilter returns the current COMMDLG filter in filterBuf
up to a maximum of nFilter characters.  XMGetFilter returns
the number of characters actually returned in filterBuf.
  int XMCountPaths(HXM)
XMCountPaths returns the number of possible translators at
this stage of the translator selection process.
  BOOL XMGetPathInfo(HXM hXM, int nPath, XMPATH FAR
  *lpinfo)
XMGetPathInfo fills in the XMINFO structure with details
about the nPath'th possible translator at this stage of the
translator selection process.  The information provided
includes the names and indexes of the PC application and
Psion format handled by this translator as well as a set of
flags indicating translator attributes.
  
  Selecting a translator
The selection of a specific translator is accomplished by
reducuing the available choices until only one remains.  The
choices can be reduced by specifying the direction of
transfer and the file formats to be exchanged.  When the
file to be transfered is known, it also can be examined to
restrict the choice of translators.
The following calls support this narrowing process.
  BOOL XMSetDirection(HXM hXM, int direction)
XMSetDirection begins the process of selecting a translator
by restricting the choice of translators to those suitable
for import or for export.  Setting the direction clears any
selections of a Psion Format or PC application to exchange.
  BOOL XMSetFormat(HXM hXM, int nFormat)
XMSetFormat further restricts the choice of translation
paths by indicating the Psion format to be exchanged.  The
direction of transfer should already be set as resetting the
direction of transfer will clear the choice of Psion format
to be exchanged.
  BOOL XMSetApp(HXM hXM, int nApp)
XMSetApp restricts the choice of translators by selecting
the PC application format to be exchanged. The direction of
transfer should already be set as resetting the direction of
transfer will clear the choice of PC application format to
be exchanged.
  int XMSelectApp(HXM hXM, LPSTR fileName)
XMSelectApp asks each currently allowed translator in turn
to examine the indicated file and answer whether or not it
can translate the file.  The return value indicates either
the number of possible translation paths left to choose from
or and indication of a problem.  A return value of  0
indicates that NO translator can handle the file.  A return
value of -1 means that the file is password protected and
therefore cannot be translated.
  int XMSelectFormat(HXM hXM, LPSTR fileName)
XMSelectFormat uses the available translation paths to
determine the Psion format of the indicated file.  The
return value will be the one-based index of the associated
Psion format if the file is recognized and can be
translated.  A return value of  0 means that this file's
format is unknown.  A return value of -1 means that the file
is password protected and therefore cannot be translated.
  BOOL XMSetPath(HXM hXM, int nPath)
XMSetPath makes the final translator selection by
identifying one of one or more translation paths.
  
  Import/Export
Once a translator has been selected, the selection is
indicated via XMSetPath.  At this point,  a result file name
for the translation will be selected.  Depending on the
transfer modes supported by the translator selected, the
result file may be handled differently.  Translators can
support one or more of the following transfer modes.
(Translators do support more than Overwrite or Append
access.  Specifically, translations to Lotus Organizer and
Act! will support both Overwrite and Append access.)
     Overwrite The translator will replace an existing file
or create a new one
     Append         The translator will add its data to an
existing file
     Merge          The translator will merge the source
file into an existing file
     Synchronize    The translator will logically
synchronize the result file with the source file.
  int XMGetTransferMode(HXM hXM)
XMGetTransferMode returns the available transfer modes for
the selected translator.
  BOOL XMImport(HXM hXM, LPSTR sourceFile, LPSTR
  resultFile,
                      int xferMode, XMSTATPROC statusRtn)
XMImport uses the current translation path to translate the
indicated file using the indicated transfer mode, the
translation is from a PC application to a Psion format.  The
statusRtn points to a routine that will be called back to
report the status of the translation as a percentage
completed and to accept a request to cancel the translation.
  BOOL XMExport(HXM hXM, LPSTR sourceFile, LPSTR
  resultFile,
                      int xferMode, XMSTATPROC statusRtn)
XMExport uses the current translation path to translate the
indicated file using the indicated transfer mode, the
translation is from a Psion format to a PC application.  The
statusRtn points to a routine that will be called back to
report the status of the translation as a percentage
completed and to accept a request to cancel the translation.
  
  Error Reporting
Most PsiWinXM API calls return FALSE in case of an error.
An appropriate error message is also accessible via the API
as follows:
  int XMGetLastErrorMsg(HXM hXM, LPSTR szMsg, int nMsg)
XMGetLastErrorMsg places an error message in szMsg of up to
nMsg characters based on the last error which has occurred
and returns the actual length of the message copied to szMsg

