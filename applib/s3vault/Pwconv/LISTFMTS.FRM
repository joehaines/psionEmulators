VERSION 2.00
Begin Form frmListFormats 
   Caption         =   "List Formats"
   ClientHeight    =   3420
   ClientLeft      =   4530
   ClientTop       =   945
   ClientWidth     =   7365
   Height          =   3825
   Icon            =   LISTFMTS.FRX:0000
   Left            =   4470
   LinkTopic       =   "Form1"
   ScaleHeight     =   3420
   ScaleWidth      =   7365
   Top             =   600
   Width           =   7485
   Begin ListBox List2 
      Height          =   2370
      Left            =   3315
      TabIndex        =   2
      Top             =   630
      Width           =   3885
   End
   Begin ListBox List1 
      Height          =   1395
      Left            =   270
      TabIndex        =   0
      Top             =   630
      Width           =   2235
   End
   Begin Label Label1 
      Caption         =   "Windows Formats"
      Height          =   195
      Left            =   3360
      TabIndex        =   3
      Top             =   180
      Width           =   3015
   End
   Begin Label Label2 
      Caption         =   "Psion Formats:"
      Height          =   270
      Left            =   345
      TabIndex        =   1
      Top             =   195
      Width           =   1290
   End
End
' LISTFMTS.FRM
' (C) 1996 Psion PLC
' GER
'
' Demonstrates how to list the available formats
'

Dim hXm As Long

Sub Form_Load ()
Dim info As pfInfo
Dim Item As String

    hXm = XMOpen("Psion", "c:\apps\psiwin\convert\", hWnd)
    res = XMSetDirection(hXm, TO_PSION)
    If (res = False) Then
	Beep
	Exit Sub
    End If
    res = XMSetFormat(hXm, 0)
    If (res = False) Then
	Beep
	Exit Sub
    End If
    numPsionApps = XMCountFormats(hXm)

    Debug.Print numPsionApps
    Label1 = numPsionApps
    For i = 1 To numPsionApps
	res = XMGetFormatInfo(hXm, i, info)
	If (res = False) Then
	    Beep
	    Exit Sub
	End If
	Item = zst(info.AppName) + " " + zst(info.AppExt) + " " + zst(info.AppPath)
	List1.AddItem (Item)
    Next i

    res = XMClose(hXm)
    hXm = 0
End Sub

Sub List1_Click ()
Dim pfInfo  As String * 70
Dim AppName As String * 256
    If (hXm <> 0) Then
	res = XMClose(hXm)
	hXm = 0
    End If
    hXm = XMOpen("Psion", "c:\apps\psiwin\convert\", hWnd)
    res = XMSetDirection(hXm, TO_PSION)
    If (res = False) Then
	Beep
	Exit Sub
    End If
    res = XMSetFormat(hXm, List1.ListIndex + 1)
    If (res = False) Then
	Beep
	Exit Sub
    End If
    num_apps = XMCountApps(hXm)
    Debug.Print num_apps
    List2.Clear
    For j = 1 To num_apps
	res = XMGetAppName(hXm, j, AppName, 240)
	List2.AddItem (AppName)
	'Debug.Print AppName
    Next j
    res = XMClose(hXm)
    hXm = 0
End Sub

