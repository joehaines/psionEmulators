PROGRAM Explode;

CONST Size=6;

{ Program for the computer to play a player at the game of explode. }

TYPE BoardType=RECORD
                   Board:ARRAY[1..Size,1..Size] OF Integer1;
                   White,Black:BYTE;
               END;
     BoardCoord=1..Size;
     
VAR CriticalMass:ARRAY[1..Size,1..Size] OF Byte;
    TheBoard:BoardType;
    SkillLevel:Byte;

PROCEDURE Initialize;
    VAR i,j:BoardCoord;
        Mass:Byte;
    BEGIN
        FOR i:=1 TO Size DO
        FOR j:=1 TO Size DO
        BEGIN
            Mass:=4;
            IF (i=1) OR (i=Size) THEN Mass:=3;
            IF (j=1) OR (j=Size) THEN Mass:=Pred(Mass);
            CriticalMass[i,j]:=Mass;
            TheBoard.Board[i,j]:=0;
        END;
        TheBoard.Black:=0;
        TheBoard.White:=0;
    END;
               
PROCEDURE PrintBoard;
    VAR i,j:BoardCoord;
    BEGIN
        Writeln;
        Writeln('      1  2  3  4  5  6');
        Writeln('----------------------');
        FOR i:=1 TO Size DO
        BEGIN
            Write(Chr(i+64),' | ');
            FOR j:=1 TO Size DO
                Write(' ',TheBoard.Board[i,j]:2);
            Writeln;
        END;
        Writeln;
        Writeln('Black (+ve) has ',TheBoard.Black:1,' squares');
        Writeln('White (-ve) has ',TheBoard.White:1,' squares');
        Writeln;
    END;
    
PROCEDURE Explode(VAR ABoard:BoardType; x,y:BoardCoord;
                  Direction:INTEGER1);
    VAR ToExplode:ARRAY[1..Size,1..Size] OF Boolean;
        Finished:Boolean;
        i,j:BoardCoord;
        Mass:-4..4;
    PROCEDURE Add(VAR b:INTEGER1);
        BEGIN
            IF Direction=-1 THEN
            BEGIN
                IF b>0 THEN
                BEGIN
                    ABoard.White:=Succ(ABoard.White);
                    ABoard.Black:=Pred(ABoard.Black);
                    b:=-b-1;
                END
                ELSE IF b=0 THEN
                BEGIN
                    ABoard.White:=Succ(ABoard.White);
                    b:=-1;
                END
                ELSE b:=b-1;
            END
            ELSE BEGIN
                IF b<0 THEN
                BEGIN
                    ABoard.Black:=Succ(ABoard.Black);
                    ABoard.White:=Pred(ABoard.White);
                    b:=-b+1;
                END
                ELSE IF b=0 THEN
                BEGIN
                    ABoard.Black:=Succ(ABoard.Black);
                    b:=1;
                END
                ELSE b:=b+1;
            END;
        END;
    BEGIN
        Mass:=ABoard.Board[x,y]+Direction;
        IF Mass=Direction THEN
        BEGIN
            IF Direction=1 THEN ABoard.Black:=Succ(ABoard.Black)
                           ELSE ABoard.White:=Succ(ABoard.White);
        END;
        ABoard.Board[x,y]:=Mass;
        IF (Mass>=CriticalMass[x,y]) OR (-Mass>=CriticalMass[x,y]) THEN
        REPEAT
            Finished:=TRUE;
            FOR i:=1 TO Size DO
            FOR j:=1 TO Size DO
            BEGIN
                Mass:=ABoard.Board[i,j];
                IF (Mass>=CriticalMass[i,j])
                    OR (-Mass>=CriticalMass[i,j]) THEN
                BEGIN
                    ToExplode[i,j]:=TRUE;
                    Finished:=FALSE;
                END
                ELSE ToExplode[i,j]:=FALSE;
            END;
            IF Not(Finished) THEN
            FOR i:=1 TO Size DO
            FOR j:=1 TO Size DO
            IF ToExplode[i,j] THEN
            BEGIN
                IF i>1 THEN
                    Add(ABoard.Board[i-1,j]); 
                IF i<Size THEN
                    Add(ABoard.Board[i+1,j]); 
                IF j>1 THEN
                    Add(ABoard.Board[i,j-1]); 
                IF j<Size THEN
                    Add(ABoard.Board[i,j+1]);
                ABoard.Board[i,j]:=ABoard.Board[i,j]-
                                   Direction*CriticalMass[i,j];
                IF ABoard.Board[i,j]=0 THEN
                BEGIN
                    IF Direction=1 THEN
                        ABoard.Black:=Pred(ABoard.Black)
                    ELSE
                        Aboard.White:=Pred(ABoard.White);
                END;
            END;
            IF (ABoard.Black=0) OR (ABoard.White=0) THEN
                Finished:=TRUE;
        UNTIL Finished;
    END;
    
FUNCTION Score(VAR ABoard:BoardType):INTEGER2;
    VAR i,j:BoardCoord;
        WhiteScore,BlackScore:INTEGER2;
        A,C:INTEGER1;
    BEGIN
        WhiteScore:=0;
        BlackScore:=0;
        FOR i:=1 TO Size DO
        FOR j:=1 TO Size DO
        BEGIN
            C:=CriticalMass[i,j];
            A:=ABoard.Board[i,j];
            IF A>0 THEN
            BEGIN
                IF A>C THEN
                    BlackScore:=BlackScore+A+6
                ELSE
                    BlackScore:=BlackScore+A+4-C;
            END;
            IF A<0 THEN
            BEGIN
                IF -A>C THEN
                    WhiteScore:=WhiteScore-A+6
                ELSE
                    WhiteScore:=WhiteScore-A+4-C;
            END;
        END;
        IF BlackScore=0 THEN Score:=-32767
        ELSE IF WhiteScore=0 THEN Score:=32767
        ELSE Score:=(BlackScore-WhiteScore);
    END;
    
PROCEDURE DoPly(VAR ABoard:BoardType; Ply:Byte; Direction:INTEGER1;
                VAR x,y:BoardCoord; VAR Best:INTEGER2; Alpha:INTEGER2);
    VAR DummyX,DummyY,i,j:BoardCoord;
        Try:BoardType;
        Measure:INTEGER2;
    LABEL 99;
    BEGIN
        Best:=-32767;
        IF Ply=1 THEN
        BEGIN
            FOR i:=1 TO Size DO
            FOR j:=1 TO Size DO
            IF (Direction*ABoard.Board[i,j])>=0 THEN
            BEGIN
                Try:=ABoard;
                Explode(Try,i,j,Direction);
                Measure:=Score(Try);
                IF Measure*Direction>Best THEN
                BEGIN
                    Best:=Measure*Direction;
                    X:=i;
                    Y:=j;
                END;
            END;
        END
        ELSE BEGIN
            FOR i:=1 TO Size DO
            FOR j:=1 TO Size DO
            IF (Direction*ABoard.Board[i,j])>=0 THEN
            BEGIN
                Try:=ABoard;
                Explode(Try,i,j,Direction);
                DoPly(Try,Ply-1,-Direction,DummyX,DummyY,Measure,Best);
                { Experimental alpha-beta line }
                { IF Measure*Direction<Alpha THEN Goto 99; }
                IF (Measure*Direction)>Best THEN
                BEGIN
                    Best:=Measure*Direction;
                    x:=i;
                    y:=j;
                END;
            END;
        END;
        99:
        Best:=Best*Direction;
    END;
    
VAR i:BYTE;
    c:Char;
    Direction:INTEGER1;
    x,y:BoardCoord;
    Best:INTEGER2;

BEGIN
    Writeln('Enter skill level');
    Readln(SkillLevel);
    Initialize;
    TheBoard.Board[1,1]:=1;
    TheBoard.Black:=1;
    Direction:=-1;
    PrintBoard;
    REPEAT
        IF Direction=-1 THEN
        BEGIN
            REPEAT
                Readln(c,i);
            UNTIL (c>='A') AND (c<='H') AND (i>=1) AND (i<=Size);
            Explode(TheBoard,Ord(C)-64,i,Direction);
        END
        ELSE BEGIN
            DoPly(TheBoard,SkillLevel,Direction,x,y,Best,
                  -32767*Direction);
            Explode(TheBoard,x,y,Direction);
            Writeln('Quality of move is ',Best:1);
        END;
        PrintBoard;
        Direction:=-Direction;
    UNTIL (TheBoard.White=0) OR (TheBoard.Black=0);
END.
