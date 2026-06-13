VERSION 2.00
Begin Form frmConPCPsion 
   Caption         =   "Convert from PC to Psion"
   ClientHeight    =   3495
   ClientLeft      =   2535
   ClientTop       =   2460
   ClientWidth     =   8955
   Height          =   3900
   Icon            =   CPCPSI.FRX:0000
   Left            =   2475
   LinkTopic       =   "Form4"
   ScaleHeight     =   3495
   ScaleWidth      =   8955
   Top             =   2115
   Width           =   9075
   Begin CommandButton cmdCancelConvert 
      Caption         =   "Cancel"
      FontBold        =   -1  'True
      FontItalic      =   0   'False
      FontName        =   "MS Sans Serif"
      FontSize        =   15
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      Height          =   1275
      Left            =   2520
      TabIndex        =   23
      Top             =   1890
      Visible         =   0   'False
      Width           =   3165
   End
   Begin Gauge Gauge1 
      Autosize        =   -1  'True
      Height          =   540
      InnerBottom     =   5
      InnerLeft       =   5
      InnerRight      =   5
      InnerTop        =   5
      Left            =   1470
      Max             =   100
      NeedleWidth     =   1
      TabIndex        =   22
      Top             =   1155
      Visible         =   0   'False
      Width           =   6000
   End
   Begin MsgBlaster MsgBlaster1 
      Prop8           =   "Click on ""..."" for the About Box ---->"
   End
   Begin ListBox List1 
      Height          =   810
      Left            =   2625
      TabIndex        =   17
      Top             =   840
      Width           =   2640
   End
   Begin CommonDialog CMDialog1 
      Left            =   0
      Top             =   105
   End
   Begin CommandButton cmdCancel 
      Caption         =   "Cancel"
      Height          =   390
      Left            =   6975
      TabIndex        =   6
      Top             =   2310
      Width           =   1125
   End
   Begin TextBox Text2 
      Height          =   285
      Left            =   5610
      TabIndex        =   5
      Text            =   "Text2"
      Top             =   735
      Width           =   2145
   End
   Begin CommandButton cmdBrowse 
      Caption         =   "..."
      Height          =   270
      Left            =   2205
      TabIndex        =   4
      Top             =   750
      Width           =   300
   End
   Begin ListBox List3 
      Height          =   1005
      Left            =   2625
      TabIndex        =   3
      Top             =   2100
      Width           =   2625
   End
   Begin CommandButton cmdBegin 
      Caption         =   "Begin Translate"
      Height          =   315
      Left            =   255
      TabIndex        =   2
      Top             =   105
      Width           =   1755
   End
   Begin TextBox Text1 
      Height          =   285
      Left            =   60
      TabIndex        =   1
      Text            =   "Text1"
      Top             =   735
      Width           =   2145
   End
   Begin CommandButton cmdSelect 
      Caption         =   "Select Format"
      Enabled         =   0   'False
      Height          =   315
      Left            =   2985
      TabIndex        =   0
      Top             =   105
      Width           =   1905
   End
   Begin Label Label14 
      Height          =   330
      Left            =   1470
      TabIndex        =   21
      Top             =   3150
      Width           =   7260
   End
   Begin Label Label13 
      Caption         =   "Path Info:"
      Height          =   225
      Left            =   210
      TabIndex        =   20
      Top             =   3150
      Width           =   1170
   End
   Begin Label Label12 
      Caption         =   "Translate to:"
      Height          =   225
      Left            =   2625
      TabIndex        =   19
      Top             =   1785
      Width           =   2535
   End
   Begin Label Label11 
      Caption         =   "Possible Formats:"
      Height          =   225
      Left            =   2625
      TabIndex        =   18
      Top             =   525
      Width           =   2430
   End
   Begin Label Label8 
      Caption         =   "Destination File:"
      Height          =   195
      Left            =   5610
      TabIndex        =   16
      Top             =   525
      Width           =   1725
   End
   Begin Label Label7 
      Caption         =   "Source file:"
      Height          =   165
      Left            =   75
      TabIndex        =   15
      Top             =   525
      Width           =   1290
   End
   Begin Label Label6 
      Height          =   165
      Left            =   105
      TabIndex        =   14
      Top             =   2730
      Width           =   2175
   End
   Begin Label Label5 
      Caption         =   "File Info:"
      Height          =   225
      Left            =   45
      TabIndex        =   13
      Top             =   2205
      Width           =   885
   End
   Begin Label Label4 
      Height          =   195
      Left            =   1710
      TabIndex        =   12
      Top             =   1890
      Width           =   585
   End
   Begin Label Label3 
      Caption         =   "Transfer modes:"
      Height          =   210
      Left            =   105
      TabIndex        =   11
      Top             =   1845
      Width           =   1590
   End
   Begin Label Label1 
      Caption         =   "Status:"
      Height          =   225
      Left            =   5610
      TabIndex        =   10
      Top             =   105
      Width           =   720
   End
   Begin Label Label2 
      Caption         =   "Select File"
      Height          =   180
      Left            =   6345
      TabIndex        =   9
      Top             =   105
      Width           =   1470
   End
   Begin Label Label9 
      Caption         =   "Last convert:"
      Height          =   240
      Left            =   5610
      TabIndex        =   8
      Top             =   1155
      Width           =   1275
   End
   Begin Label Label10 
      Caption         =   "None"
      Height          =   855
      Left            =   5610
      TabIndex        =   7
      Top             =   1365
      Width           =   2355
   End
End
' CPCPSI.FRM
'
' Demonstrats how to convert a PC file to a Psion File
'
' (C) 1996 Psion PLC
' GER
'


Dim hXm As Integer
Dim ProcAddress As Long
Dim VBHand As Long
Dim DoCancel As Integer

Sub cmdBegin_Click ()
Dim pfInfo  As String * 70
Dim PathInfo As String * 256
Dim AppName As String * 128
Dim AppPath As String
Dim AppExt As String

    If (hXm <> 0) Then
	Exit Sub
    End If
' Open converters
    hXm = XMOpen("Psion", "c:\apps\psiwin\convert\", hWnd)
' set direction
    res = XMSetDirection(hXm, TO_PSION)
    If (res = False) Then
	Beep
	Exit Sub
    End If
' Examine the file
    nApps = XMSelectApp(hXm, text1)
    If (nApps = -1) Then
	MsgBox "Password protected"
	Exit Sub
    End If
    If (nApps = 0) Then
	MsgBox "No Translator Available"
	Exit Sub
    End If
    Debug.Print nApps
    List1.Clear
' list available apps in a list box
    For i = 1 To nApps
	ret = XMGetAppName(hXm, i, AppName, 127)
	List1.AddItem (AppName)
    Next i
End Sub

Sub cmdBrowse_Click ()
On Error GoTo cmd2fail
    Cmdialog1.Filename = "*.*"
    Cmdialog1.Action = 1
    text1.Text = Cmdialog1.Filename
    Exit Sub
cmd2fail:
    Exit Sub
End Sub

Sub cmdCancel_Click ()
    If hXm <> 0 Then
	ret = XMClose(hXm)
	hXm = 0
	List3.Clear
	List1.Clear
	Endconvert
    End If
End Sub

Sub cmdCancelConvert_Click ()
    DoCancel = True
End Sub

Sub cmdSelect_Click ()
Dim ErrMsg As String * 128
Dim Info As XMPath

    If hXm = 0 Then
	Exit Sub
    End If
    If List3.ListIndex = -1 Then
	Exit Sub
    End If
' set the selected path
    res = XMSetPath(hXm, List3.ListIndex + 1)
' get the path info (for fun)
    res = XMGetPathInfo(hXm, List3.ListIndex + 1, Info)
    label14 = Str$(Info.pathFlags) + " " + Str$(Info.nApp) + " " + zst(Info.AppName) + " " + Str$(Info.nformat) + " " + Info.FormatName
' check the available transfer modes
' a real app should check the existance of the file your converting to
    mode = XMGetTransferMode(hXm)
    Label4 = mode
    If mode <> 2 Then ' choose mode if you want to
	mode = 1
    End If
    Gauge1.Value = 0
    Gauge1.Visible = True
    DoCancel = False
'    cmdCancelConvert.Visible = True
' do the conversion
    ret = XMImport(hXm, text1, Text2, mode, ProcAddress)
'    cmdCancelConvert.Visible = False
    Gauge1.Visible = False
' if theres an error display it
    If (ret <> 1) Then
	n = XMGetLastErrorMsg(hXm, ErrMsg, 127)
	Label10 = Left$(ErrMsg, n)
    Else
	Label10 = "Success"
    End If
    ret = XMClose(hXm)
    hXm = 0
    Endconvert
End Sub

Sub Endconvert ()
    Label2 = "Select File"
    cmdBegin.Enabled = True
    cmdSelect.Enabled = False
End Sub

Sub Form_Load ()
    hXm = 0
' initialise the MsgBlaster VBX needed for our C callback
' please write your own version of vbcondll.dll
    MsgBlaster1.hWndTarget = hWnd
    MsgBlaster1.MsgList(0) = 1024 + 69' WM_USER +69
    ret = InitHandSys()
    hand = GetModuleHandle("vbcondll")
    If (hand = 0) Then
	ProcAddress = 0
	Exit Sub
    End If
    ProcAddress = GetProcAddress(hand, "StatusProcedure")
    VBHand = InitHandler(hWnd, 69)
End Sub

Sub Form_Unload (Cancel As Integer)
    res = DeleteHandler(VBHand)
End Sub

Sub List1_Click ()
Dim Info  As pfInfo
    List3.Clear
    If List1.ListIndex = -1 Then
	Exit Sub
    End If
    ret = XMSetApp(hXm, List1.ListIndex + 1)
    nformats = XMCountFormats(hXm)
    If nformats <= 0 Then
	MsgBox "No conversion"
	Exit Sub
    End If
    For i = 1 To nformats
	res = XMGetFormatInfo(hXm, 1, Info)
	List3.AddItem (zst(Info.AppName) + " " + zst(Info.AppExt) + " " + zst(Info.AppPath))
    Next i
End Sub

Sub List3_Click ()
    If List3.ListIndex <> -1 Then
	List3.Enabled = True
	cmdSelect.Enabled = True
	Label2 = "Select Path"
	cmdBegin.Enabled = True
    End If
End Sub

Sub MsgBlaster1_Message (MsgVal As Integer, wParam As Integer, lParam As Long, ReturnVal As Long)
'    Beep
    DoEvents
'    Debug.Print MsgVal, wParam, lParam
    Gauge1.Value = wParam
    If DoCancel = True Then
	ReturnVal = 1
    Else
	ReturnVal = 0
    End If
End Sub

