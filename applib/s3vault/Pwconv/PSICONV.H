/*	PSICONV.H
*/

#ifndef	_INC_PSIWINXM

#define	_INC_PSIWINXM

#ifdef __cplusplus
	extern "C" {
#endif

#include	<windows.h>

#define	PSIWINXM_VERSION	0x0100

typedef HGLOBAL	HXM;

#define	TO_PSION		0
#define	FROM_PSION		1
#define	FROM_CLIPBOARD	2
#define	TO_CLIPBOARD	3
	
#define	ALL_PSION_FORMATS	0
		

#define	PSXM_OVERWRITE		0x8000	/* Overwrite on export	*/
#define	PSXM_APPEND			0x4000	/* Append on export		*/


typedef	struct	tagPFINFO
{
	char	psionFormatName[32];
	char	psionFormatExt[4];
	char	psionFormatPath[32];

}	PFINFO, FAR *LPPFINFO;

typedef	struct	tagXMPATH
{
	WORD	pathFlags;
	int		nApp;
	char	appName[64];
	int     nFormat;
	char	formatName[32];

}	XMPATH,	FAR *LPXMPATH;

/*	Prototypes	*/

#define	XL_EXTENSION	".PXL"

typedef	BOOL	(WINAPI *XMSTATPROC)(int);

#define	XP_EXTENSION	".PXP"

#define	XM_MAX_PATH		144

#define	XM_MAX_PREF		512

HXM WINAPI  XMOpen(LPSTR psionName, LPSTR xlateFolder, HWND hParent);
/*
			psionName is a name for the connected Psion, to track user specific
            preferences and field mappings.  If none is available, set this to
            NULL and we will assume guest access

            xlateFolder is the DOS pathName to the directory where the
            translators will be stored.

            hParent is the HWND of a window that will be used as the parent
            window in bringing up dialogs, such as field naming and mapping.
*/
BOOL WINAPI XMClose(HXM hXM);
/*
            hXM is the HXM returned by XMOpen(), a translation context handle
*/
BOOL WINAPI XMSetDirection(HXM hXM, int direction);
/*
            hXM is a translation context handle

            direction is the direction of transfer

            Setting the direction causes the list of available translators
            and associated applications to be narrowed to reflect the indicated
            direction.

            Returns TRUE if the function was successful
*/
BOOL WINAPI XMSetFormat(HXM hXM, int nFormatID);
/*
			hXM is a translation context handle

            nFormatID is a one-based index into the list of known Psion
            formats, based on the installed translators.  An nFormatID of
            zero is the special case of ALL_AVAILABLE_FORMATS.

            Setting the Host Format causes the list of available translators
            and associated applications to be narrowed to reflect the indicated
            Host Format.

            Returns TRUE if the function was successful
*/
BOOL WINAPI XMSetApp(HXM hXM, int nApp);
/*
            hXM is a translation context handle

            nApp is the one-based index of the application to exchange. An nApp of
            zero is the special case of ALL_AVAILABLE_APPS

            Returns TRUE if the function was successful
            
*/
BOOL WINAPI XMSetPath(HXM hXM, int nPath);
/*
			hXM is a translation context handle

            nPath is a one-based index into the list of selected
            translation paths.

            Setting the translation path selects a specific translator for
            import or export.

            Returns TRUE if the function was successful
*/
int WINAPI  XMSelectApp(HXM hXM, LPSTR pszSourceFile);
/*
            hXM is a translation context handle

            pszSourceFile is the DOS path name of the file to be translated
            
            Returns a translator selection status as follows:

            0 - No translator is available for this file
            -1 - This file is password protected
            
            or
            
            The number of possible translation paths
*/
int WINAPI  XMSelectFormat(HXM hXM, LPSTR pszSourceFile);
/*
            hXM is a translation context handle

            pszSourceFile is the DOS path name of the file to be translated

            Returns a host format index or as follows:

            0  - Host Format is unknown
            -1 - This file is password protected
*/

//		Q U E R I E S

//			COMMDLG Filter

int WINAPI  XMGetFilterLen(HXM hXM);
/*
            hXM is a translation context handle

            Returns the length of the COMMDLG filter string
*/
int WINAPI  XMGetFilter(HXM hXM, LPSTR pszFilter, int nFilter);
/*
            hXM is a translation context handle

            pszFilter is a buffer that will receive the COMMDLG filter string

            nFilter is the size of the COMMDLG filter string buffer

            Returns the number of bytes copied to pszFilter
*/

//			Apps

int WINAPI  XMCountApps(HXM hXM);
/*
            hXM is a translation context handle

            Returns the number of known Application Formats,i.e., the numer of
            Application Formats for which there are translators, based on the 
            current Host Format and direction.
*/
int WINAPI  XMGetAppName(HXM hXM, int nApp, LPSTR pszAppName, int nAppName);
/*
            hXM is a translation context handle

            nApp is the one-based index of the application

            pszAppName is the buffer that will receive the application name

            nAppName is the size of the application name buffer

            Returns the number of bytes transfered to pszAppName, not counting
            the trailing '\0'
*/

//			Formats

int WINAPI  XMCountFormats(HXM hXM);
/*
            hXM is a translation context handle

            Returns the number of known Psion Formats,i.e., the numer of
            Psion Formats for which there are translators.
*/
BOOL WINAPI XMGetFormatInfo(HXM hXM, int nFormat, LPPFINFO lpPFI);
/*
            hXM is a translation context handle

            formatID is a one-based index into the list of known Psion
            formats, based on the installed translators.  A formatID of 0
            is a special case of ALL_AVAILABLE_FORMATS.

            Setting the Host Format causes the list of available translators
            and associated applications to be narrowed to reflect the indicated
            Host Format.

            Returns TRUE if the function was successful
*/

//			Paths

int WINAPI	XMCountPaths(HXM hXM);
/*
            hXM is a translation context handle

            Returns the number of available translation paths based on the current
            direction, format and application selected.
*/
BOOL WINAPI	XMGetPathInfo(HXM hXM, int nPath, LPXMPATH lpPath);
/*
			hXM is a translation context handle
			
			nPath is the one-based index of the translation path
			
			lpPath points to the XMPATH structure that will receive the
			information about the indicated translation path

            Returns TRUE if the function was successful
*/

int WINAPI	XMGetCurrentFormat(HXM hXM);
/*
			hXM is the translation context handle
			
			Returns 0 or the index of the currently selected hostFormat
*/
int WINAPI	XMGetCurrentApp(HXM hXM);
/*
			hXM is the translation context handle
			
			Returns 0 or the index of the currently selected application
*/

int WINAPI XMGetTransferMode(HXM hXM);
/*
            hXM is a translation context handle

            Returns the available transfer modes of the currently
            selected translator.  The available modes are as follows:

            1 - Overwrite
            2 - Append
            4 - Merge/Update
            8 - Synchronize

            A transfer mode should then be selected for the Import or Export
            operation.
*/
BOOL WINAPI XMImport(HXM hXM, LPSTR pszSource,  LPSTR pszResult, int nMode, XMSTATPROC statProc);
/*
            hXM is a translation context handle

            pszSource is the DOS path name of the source file

            pszResult is the DOS path name of the result file

            nMode is the transfer mode to be used by the translator
            as follows:

            1 - Overwrite
            2 - Append
            4 - Merge/Update
            8 - Synchronize
                        
            Returns TRUE if the function was successful
*/
BOOL WINAPI XMExport(HXM hXM, LPSTR pszSource,  LPSTR pszResult, int nMode, XMSTATPROC statProc);
/*
            hXM is a translation context handle

            pszSource is the DOS path name of the source file

            pszResult is the DOS path name of the result file

            nMode is the transfer mode to be used by the translator
            as follows:

            1 - Overwrite
            2 - Append
            4 - Merge/Update
            8 - Synchronize
                        
            Returns TRUE if the function was successful
*/
int WINAPI XMGetLastErrorMsg(HXM hXM, LPSTR pszMsg, int nMsg);
/*
            hXM is a translation context handle

            pszMsg is a buffer to receive the error message

            nMsg is the size of the pszMsg buffer

            Returns the number of characters actually copies to pszMsg,
            not including the trailing '\0'
*/

#ifdef __cplusplus
	}
#endif

#endif	//	_INC_PSIWINXM
