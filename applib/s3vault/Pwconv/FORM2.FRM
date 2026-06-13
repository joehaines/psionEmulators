VERSION 2.00
Begin Form Form2 
   Caption         =   "Convert Tester"
   ClientHeight    =   4200
   ClientLeft      =   3690
   ClientTop       =   2475
   ClientWidth     =   7455
   Height          =   4605
   Icon            =   FORM2.FRX:0000
   Left            =   3630
   LinkTopic       =   "Form2"
   ScaleHeight     =   4200
   ScaleWidth      =   7455
   Top             =   2130
   Width           =   7575
   Begin CommandButton Command3 
      Caption         =   "List Formats"
      FontBold        =   -1  'True
      FontItalic      =   0   'False
      FontName        =   "MS Sans Serif"
      FontSize        =   15
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      Height          =   1185
      Left            =   2205
      TabIndex        =   2
      Top             =   2835
      Width           =   2940
   End
   Begin CommandButton Command2 
      Caption         =   "PC to Psion"
      FontBold        =   -1  'True
      FontItalic      =   0   'False
      FontName        =   "MS Sans Serif"
      FontSize        =   15
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      Height          =   1185
      Left            =   2205
      TabIndex        =   1
      Top             =   1470
      Width           =   2940
   End
   Begin CommandButton Command1 
      Caption         =   "Psion to PC"
      FontBold        =   -1  'True
      FontItalic      =   0   'False
      FontName        =   "MS Sans Serif"
      FontSize        =   15
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      Height          =   1185
      Left            =   2205
      TabIndex        =   0
      Top             =   135
      Width           =   2940
   End
End
'
' Converter Tester
'
'  (C) 1996 Psion PLC
'
' Author Gareth Richards
'
' Small visual basic program that demonstrates how to use
' the PsiWin converters.
' Refer to convert.txt and psiconv.h for more detailed
' discriptions of how to use these functions.
'
'

Sub Command1_Click ()
     Load frmConPsionPC
     frmConPsionPC.Visible = True
End Sub

Sub Command2_Click ()
     Load frmConPCPsion
     frmConPCPsion.Visible = True
End Sub

Sub Command3_Click ()
    Load frmListFormats
    frmListFormats.Visible = True
End Sub

