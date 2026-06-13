' Convert.bas
' Refer to PSICONV.H for more information
'
' (C) 1996 Psion PLC
' GER
'


Global Const TO_PSION = 0
Global Const FROM_PSION = 1
Global Const FROM_CLIPBOARD = 2
Global Const TO_CLIPBOARD = 3

' Transfer modes returned by XMGetTransfer mode
Global Const XM_OVERWRITE = 1
Global Const XM_APPEND = 2
Global Const XM_MERGEUPDATE = 4
Global Const XM_SYNCHRONIZE = 8


Type pfInfo
    AppName As String * 32
    AppExt As String * 4
    AppPath As String * 32
End Type

Type XMPath
    PathFlags As Integer
    nApp As Integer
    AppName As String * 64
    nFormat As Integer
    FormatName As String * 32
End Type


'HXM WINAPI  XMOpen(LPSTR psionName, LPSTR xlateFolder, HWND hParent);
Declare Function XMOpen Lib "psiwinxm.dll" (ByVal psionName As String, ByVal xlateFolder As String, ByVal Hwnd As Integer) As Integer
'BOOL WINAPI XMClose(HXM hXM);
Declare Function XMClose Lib "psiwinxm.dll" (ByVal hXM As Integer) As Integer
'BOOL WINAPI XMSetDirection(HXM hXM, int direction);
Declare Function XMSetDirection Lib "psiwinxm.dll" (ByVal hXM As Integer, ByVal direction As Integer) As Integer
'BOOL WINAPI XMSetFormat(HXM hXM, int nFormatID);
Declare Function XMSetFormat Lib "psiwinxm.dll" (ByVal hXM As Integer, ByVal nFormatID As Integer) As Integer
'BOOL WINAPI XMSetApp(HXM hXM, int nApp);
Declare Function XMSetApp Lib "psiwinxm.dll" (ByVal hXM As Integer, ByVal nApp As Integer) As Integer
'BOOL WINAPI XMSetPath(HXM hXM, int nPath);
Declare Function XMSetPath Lib "psiwinxm.dll" (ByVal hXM As Integer, ByVal nPath As Integer) As Integer
'int WINAPI  XMSelectApp(HXM hXM, LPSTR pszSourceFile);
Declare Function XMSelectApp Lib "psiwinxm.dll" (ByVal hXM As Integer, ByVal SourceFile As String) As Integer
'int WINAPI  XMSelectFormat(HXM hXM, LPSTR pszSourceFile);
Declare Function XMSelectFormat Lib "psiwinxm.dll" (ByVal hXM As Integer, ByVal SourceFile As String) As Integer
'int WINAPI  XMGetFilterLen(HXM hXM);
Declare Function XMGetFilterLen Lib "psiwinxm.dll" (ByVal hXM As Integer) As Integer
'int WINAPI  XMGetFilter(HXM hXM, LPSTR pszFilter, int nFilter);
Declare Function XMGetFilter Lib "psiwinxm.dll" (ByVal hXM As Integer, ByVal nFilter As Integer) As Integer
'int WINAPI  XMCountApps(HXM hXM);
Declare Function XMCountApps Lib "psiwinxm.dll" (ByVal hXM As Integer) As Integer
'int WINAPI  XMGetAppNameLen(HXM hXM, int nApp); ' defined but does not exist
Declare Function XMGetAppNameLen Lib "psiwinxm.dll" (ByVal hXM As Integer, ByVal nApp As Integer) As Integer
'int WINAPI  XMGetAppName(HXM hXM, int nApp, LPSTR pszAppName, int nAppName);
Declare Function XMGetAppName Lib "psiwinxm.dll" (ByVal hXM As Integer, ByVal nApp As Integer, ByVal AppName As String, ByVal nAppName As Integer) As Integer
'int WINAPI  XMCountFormats(HXM hXM);
Declare Function XMCountFormats Lib "psiwinxm.dll" (ByVal hXM As Integer) As Integer
'BOOL WINAPI XMGetFormatInfo(HXM hXM, int nFormat, LPPFINFO lpPFI);
Declare Function XMGetFormatInfo Lib "psiwinxm.dll" (ByVal hXM As Integer, ByVal nFormat As Integer, lpPfi As Any) As Integer
'int WINAPI  XMCountPaths(HXM hXM);
Declare Function XMCountPaths Lib "psiwinxm.dll" (ByVal hXM As Integer) As Integer
'BOOL WINAPI XMGetPathInfo(HXM hXM, int nPath, LPXMPATH lpPath);
Declare Function XMGetPathInfo Lib "psiwinxm.dll" (ByVal hXM As Integer, ByVal nPath As Integer, Path As Any) As Integer
'int WINAPI XMCountPrefs(HXM hXM);
Declare Function XMGetCurrentFormat Lib "psiwinxm.dll" (ByVal hXM As Integer) As Integer
'int WINAPI  XMGetCurrentApp(HXM hXM);
Declare Function XMGetCurrentApp Lib "psiwinxm.dll" (ByVal hXM As Integer) As Integer
'int WINAPI XMGetTransferMode(HXM hXM);
Declare Function XMGetTransferMode Lib "psiwinxm.dll" (ByVal hXM As Integer) As Integer
'BOOL WINAPI XMImport(HXM hXM, LPSTR pszSource,  LPSTR pszResult, int nMode, XMSTATPROC statProc);
Declare Function XMImport Lib "psiwinxm.dll" (ByVal hXM As Integer, ByVal Source As String, ByVal dest As String, ByVal mode As Integer, ByVal statProc As Long) As Integer
'BOOL WINAPI XMExport(HXM hXM, LPSTR pszSource,  LPSTR pszResult, int nMode, XMSTATPROC statProc);
Declare Function XMExport Lib "psiwinxm.dll" (ByVal hXM As Integer, ByVal Source As String, ByVal dest As String, ByVal mode As Integer, ByVal statProc As Long) As Integer
'int WINAPI XMGetLastErrorMsg(HXM hXM, LPSTR pszMsg, int nMsg);
Declare Function XMGetLastErrorMsg Lib "psiwinxm.dll" (ByVal hXM As Integer, ByVal errmsg As String, ByVal nMsg As Integer) As Integer

'
' Call Back functions
'
Declare Function InitHandSys Lib "vbcondll.dll" () As Integer
Declare Function InitHandler Lib "vbcondll.dll" (ByVal iHwnd As Integer, ByVal msgno As Integer) As Long
Declare Function DeleteHandler Lib "vbcondll.dll" (ByVal hand As Long) As Integer

Declare Function GetModuleHandle Lib "kernel" (ByVal modname As String) As Integer
Declare Function GetProcAddress Lib "kernel" (ByVal Module As Integer, ByVal ProcName As String) As Long

Function zst (in As String) As String
    pos = InStr(in, Chr$(0))
    zst = Left$(in, pos - 1)
End Function

