program qcom;
{ One way Keyboard Link: PC to Psion 3A  Requires KEYBOARD.MCO }
{ Copyright (c) 1995 PROTRONICS USA 404-351-1055 Public Use OK }
{ Compiled with Borland Turbo Pascal 5.0 }

uses crt,dos;

  Type
    __ParityType=(None,Even,Odd);

  var
    C : char;
    S : string;
    __Regs: Registers;

  procedure __Int14(PortNumber,Command,Parameter: Integer);
  { do a BIOS COM driver interrupt }

    begin
      with __Regs do
       begin
        DX:=PortNumber-1;
        AH:=Command;
        AL:=Parameter;
        Flags:=0;
        Intr ($14,__Regs);
       end;
    end;


  procedure SetSerial(PortNumber,BaudRate,StopBits,DataBits: Integer; Parity: __ParityType);
  { Set serial parameters on a COM port }

    var
      Parameter: Integer;

    begin
      case BaudRate of
        1200: BaudRate:=4;
        2400: BaudRate:=5;
        4800: BaudRate:=6;
        else BaudRate:=7; { Default to 9600 baud }
       end;
      if StopBits=2 then StopBits:=1
      else StopBits:=0; { Default to 1 stop bit }
      if DataBits=7 then DataBits:=2
      else DataBits:=3; { Default to 8 data bits }
      Parameter:=(BaudRate Shl 5)+(StopBits Shl 2)+DataBits;
      case Parity of
        Odd: Parameter:=Parameter+8;
        Even: Parameter:=Parameter+24;
        else; { Default to no parity }
       end;
      __Int14(PortNumber,0,Parameter);
    end;


  procedure __OutPort1(C: Byte);

    begin
      __Int14(1,1,C);
    end;

  function nval (Ln : string) : integer;

  var I, Code : integer;
  begin
    val (Ln, I, Code);
    nval := I;
  end;


  begin
    clrscr;
    S := 'PSION COM1:1200 by PROTRONICS USA 404-351-1055  Ctrl-Z shuts down link';
    writeln (S);
    writeln ('ctrl-a Psion key  ctrl-c toggles Ctrl  ctrl-s toggles Alt  ctrl-w Menu');
    writeln ('ctrl-o Psion off  ctrl-y loads System  ctrl-d Diamond btn  ctrl-l Help',#10);
    SetSerial(nval(S[10]),nval(copy(S,12,4)),0,8,__ParityType(0));
    repeat
      C := readkey;
      __OutPort1(ord(C));
      if (C > #31) or (C = #8) then
        write(C)
      else
        if C = #0 then
        begin
        C := readkey;
        __OutPort1(ord(C));
        end;
      if C = #13 then writeln;
      until C = #26;
  end.
