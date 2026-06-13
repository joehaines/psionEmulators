VERSION 2.00
Begin Form frmConPsionPC 
   Caption         =   "Convert to PC Format"
   ClientHeight    =   3105
   ClientLeft      =   3765
   ClientTop       =   1620
   ClientWidth     =   8490
   Height          =   3510
   Icon            =   CPSIPC.FRX:0000
   Left            =   3705
   LinkTopic       =   "Form3"
   ScaleHeight     =   3105
   ScaleWidth      =   8490
   Top             =   1275
   Width           =   8610
   Begin CommandButton cmdSelect 
      Caption         =   "Select Format"
      Enabled         =   0   'False
      Height          =   315
      Left            =   3255
      TabIndex        =   12
      Top             =   105
      Width           =   1905
   End
   Begin CommonDialog CMDialog1 
      Left            =   -195
      Top             =   1245
   End
   Begin TextBox Text1 
      Height          =   285
      Left            =   330
      TabIndex        =   5
      Text            =   "Text1"
      Top             =   735
      Width           =   2205
   End
   Begin CommandButton cmdBegin 
      Caption         =   "Begin Translate"
      Height          =   315
      Left            =   525
      TabIndex        =   4
      Top             =   105
      Width           =   1755
   End
   Begin ListBox List3 
      Height          =   1785
      Left            =   2955
      TabIndex        =   3
      Top             =   495
      Width           =   2625
   End
   Begin CommandButton cmdBrowse 
      Caption         =   "..."
      Height          =   270
      Left            =   2520
      TabIndex        =   2
      Top             =   735
      Width           =   300
   End
   Begin TextBox Text2 
      Height          =   285
      Left            =   5880
      TabIndex        =   1
      Text            =   "Text2"
      Top             =   735
      Width           =   2145
   End
   Begin CommandButton cmdCancel 
      Caption         =   "Cancel"
      Height          =   390
      Left            =   7245
      TabIndex        =   0
      Top             =   2310
      Width           =   1125
   End
   Begin Label Label12 
      Height          =   225
      Left            =   1365
      TabIndex        =   18
      Top             =   2625
      Width           =   4530
   End
   Begin Label Label11 
      Caption         =   "Path info:"
      Height          =   225
      Left            =   315
      TabIndex        =   17
      Top             =   2625
      Width           =   960
   End
   Begin Label Label10 
      Caption         =   "None"
      Height          =   855
      Left            =   5880
      TabIndex        =   16
      Top             =   1365
      Width           =   2355
   End
   Begin Label Label9 
      Caption         =   "Last convert:"
      Height          =   240
      Left            =   5880
      TabIndex        =   15
      Top             =   1155
      Width           =   1275
   End
   Begin Label Label2 
      Caption         =   "Select File"
      Height          =   180
      Left            =   6615
      TabIndex        =   14
      Top             =   105
      Width           =   1470
   End
   Begin Label Label1 
      Caption         =   "Status:"
      Height          =   225
      Left            =   5880
      TabIndex        =   13
      Top             =   105
      Width           =   720
   End
   Begin Label Label3 
      Caption         =   "Transfer modes"
      Height          =   210
      Left            =   270
      TabIndex        =   11
      Top             =   1845
      Width           =   1590
   End
   Begin Label Label4 
      Height          =   195
      Left            =   1980
      TabIndex        =   10
      Top             =   1890
      Width           =   585
   End
   Begin Label Label5 
      Caption         =   "File Info:"
      Height          =   225
      Left            =   315
      TabIndex        =   9
      Top             =   2355
      Width           =   885
   End
   Begin Label Label6 
      Height          =   165
      Left            =   1440
      TabIndex        =   8
      Top             =   2400
      Width           =   2175
   End
   Begin Label Label7 
      Caption         =   "Source file:"
      Height          =   165
      Left            =   345
      TabIndex        =   7
      Top             =   525
      Width           =   1290
   End
   Begin Label Label8 
      Caption         =   "Destination File:"
      Height          =   195
      Left            =   5880
      TabIndex        =   6
      Top             =   525
      Width           =   1725
   End
End
' CPCPSI.FRM
'
' Demonstrats how to convert a Psiom file to a PC File
'
' (C) 1996 Psion PLC
' GER
'


Dim hXm As Integer

Sub cmdBegin_Click ()
Dim info  As pfInfo
Dim Xi As XMPath

    If (hXm <> 0) Then
        Exit Sub
    End If
    hXm = XMOpen("Psion", "c:\apps\psiwin\convert\", hWnd)
    res = XMSetDirection(hXm, FROM_PSION)
    If (res = False) Then
        Beep
        Exit Sub
    End If
    nFormatPaths = XMSelectFormat(hXm, Text1)
    If (nFormatPaths = -1) Then
        MsgBox "Password protected"
        Exit Sub
    End If
    If (nFormatPaths = 0) Then
        MsgBox "No Translator Available"
        Exit Sub
    End If
    Debug.Print nFormatPaths
    ' there should only be one format
    res = XMGetFormatInfo(hXm, 1, info)
    Label6 = zst(info.AppName) + " " + zst(info.AppExt) + " " + zst(info.AppPath)
    nPaths = XMCountPaths(hXm)
    Debug.Print nPaths
    List3.Clear
    For i = 1 To nPaths
        res = XMGetPathInfo(hXm, i, Xi)
        List3.AddItem (Xi.AppName)
    Next i
    List3.Enabled = True
    cmdSelect.Enabled = True
    Label2 = "Select Path"
    cmdBegin.Enabled = True
End Sub

Sub cmdBrowse_Click ()
On Error GoTo cmd2fail
    Cmdialog1.Filename = "*.*"
    Cmdialog1.Action = 1
    Text1.Text = Cmdialog1.Filename
    Exit Sub
cmd2fail:
    Exit Sub
End Sub

Sub cmdCancel_Click ()
    If hXm <> 0 Then
        ret = XMClose(hXm)
        hXm = 0
        List3.Clear
        Endconvert
    End If
End Sub

Sub cmdSelect_Click ()
Dim ErrMsg As String * 128
Dim info As XMPath
Dim AppName As String
Dim FormatName As String
Dim nformat As Integer
Dim nApp As Integer
Dim pathFlags As Integer
    If hXm = 0 Then
        Exit Sub
    End If
    If List3.ListIndex = -1 Then
        MsgBox "No Selection"
        Exit Sub
    End If
    res = XMGetPathInfo(hXm, List3.ListIndex + 1, info)
    label12 = Str$(info.pathFlags) + " " + Str$(info.nApp) + " " + zst(info.AppName) + " " + Str$(info.nformat) + " " + zst(info.FormatName)
    res = XMSetPath(hXm, List3.ListIndex + 1)
    mode = XMGetTransferMode(hXm)
    Label4 = mode
    If mode <> 2 Then ' choose mode if you want to
        mode = 1
    End If
    ret = XMExport(hXm, Text1, Text2, mode, 0)
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
End Sub

