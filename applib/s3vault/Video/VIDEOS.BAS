DEFLNG A-Z
DECLARE FUNCTION cmpstr$ (a$)
DECLARE FUNCTION decmpstr$ (a$)
DECLARE FUNCTION title$ (a$, b$)
DECLARE FUNCTION truncate$ (a$)
DECLARE FUNCTION remove$ (in$, del$)
'Psion pointer files
'Word                Pointer to title string 0=Null string, Last entry
'Word                Pointer to description
'Word
'        bit  15     Display this one in compressed mode
'        bit  14-13  Series Format
'                    0=None, 1=n), 2=() n), 3=[] n)
'        bits 12-0   Tape number 0-8191 0=Not got
'Byte    bit  7      Black & White
'        bit  6      Programme is long play
'        bit  5      Programme is stereo
'        bit  4      Programme is missing a bit
'        bit  3      Programme is subtitled
'        bit  2      Programme is non english
'        bit  1      Widescreen
'        bit  0      Programme is animated
'Psion description file
'Byte                Series number
'Byte                First episode number, If >127 then next episode
'Byte                End episode number
'Only the bytes needed are used
'String              Psion string format, padded to make desc multiple of 2
'Tape file
'1 Byte Per Tape
'        bit  7-4    Time remaining in 15 min intervals
'        bit  3      Tape is S-VHS
'        bit  2      Tape is not missing
'        bit  1      Tape has been lent out
'        bit  0      Flag for searches, etc.

CONST svhsbit = 8, tapegot = 4

DIM tapes%(8191)
DIM SHARED cmp(256), codes$(3)
codes$(1) = " ETAONSRIHLMDC"
codes$(2) = "GUPFWBYKV'.,"
codes$(3) = "J?+-()0123456789"
codes$(0) = codes$(1) + codes$(2) + codes$(3)

maxtape = -1

OPEN "video.txt" FOR INPUT AS #1
FOR a = 1 TO 4
   LINE INPUT #1, a$
   PRINT a$
NEXT

zero$ = CHR$(0)
zero2$ = zero$ + zero$
zero3$ = zero2$ + zero$
tit$ = CHR$(0)

KILL "titles.vid"
KILL "desc.vid"
KILL "ptr.vid"
KILL "binsort.vid"
KILL "tapes.vid"

OPEN "titles.vid" FOR BINARY AS #2
PUT #2, , tit$
tit = 1

OPEN "desc.vid" FOR BINARY AS #3

OPEN "ptr.vid" FOR BINARY AS #4

OPEN "binsort.vid" FOR BINARY AS #5

OPEN "tapes.vid" FOR BINARY AS #6

OPEN "error.txt" FOR OUTPUT AS #7

OPEN "desc.txt" FOR OUTPUT AS #8

PUT #5, , zero2$
PUT #5, , zero2$

f$ = "": last$ = "": t$ = ""

a = 0: b = 0: c = 0: desc = 0: descptr = 0

nuldesc$ = CHR$(0) + CHR$(0)
descptr = LEN(nuldesc$)
desc = LEN(nuldesc$)

PUT #3, , nuldesc$
lst$ = ""

WHILE NOT EOF(1)
   format = 0
   LINE INPUT #1, a$
   IF a$ <> "" THEN
'      b$ = LEFT$(a$, 2)
      c$ = MID$(a$, 3, 60)
'      IF LEFT$(c$, 4) = "THE " THEN c$ = MID$(c$, 5)
'      IF LEFT$(c$, 2) = "A " THEN c$ = MID$(c$, 3)
'      d$ = MID$(a$, 64, 5)
      num = VAL(MID$(a$, 64, 5))
      IF LEFT$(a$, 1) = "*" THEN num = 0
      e$ = MID$(a$, 71, 7)
      IF INSTR(c$, "B&W") THEN
         format = format OR 128
         c$ = remove$(c$, "B&W")
      END IF
      IF INSTR(c$, "ANIME") THEN
         format = format OR 1
         c$ = remove$(c$, "ANIME")
      END IF
      IF INSTR(c$, "SUBTITLED") THEN
         format = format OR 8
         c$ = remove$(c$, "SUBTITLED")
      END IF
      IF INSTR(c$, "WIDESCREEN") THEN
         format = format OR 2
         c$ = remove$(c$, "WIDESCREEN")
      END IF
      IF INSTR(c$, "*") THEN
         format = format OR 16
         c$ = remove$(c$, "*")
      END IF
      IF num = 0 THEN format = format OR 16
      IF INSTR(e$, "S") THEN
         IF num > maxtape THEN maxtape = num
         tapes%(num) = svhsbit + tapegot
      ELSE
         IF num > maxtape THEN maxtape = num
         tapes%(num) = tapegot
      END IF
      IF INSTR(e$, "M") THEN format = format OR 128
      IF INSTR(e$, "L") THEN format = format OR 64
      IF INSTR(e$, "N") THEN format = format OR 32
      IF INSTR(e$, "*") THEN format = format OR 16
      IF INSTR(e$, "T") THEN format = format OR 8
      IF INSTR(e$, "F") THEN format = format OR 4
      IF INSTR(e$, "W") THEN format = format OR 2
      IF INSTR(e$, "A") THEN format = format OR 1
      series = 0
      desc$ = ""
      seriesno = 0
      nxtepisode = 255
      episodeno = 255
      c$ = truncate$(c$)
      g$ = title$(c$, f$)
      IF g$ <> "" THEN
         IF LEFT$(g$, 2) = "A " THEN
            tmp$ = MID$(g$, 3)
         ELSEIF LEFT$(g$, 4) = "THE " THEN
            tmp$ = MID$(g$, 5)
         ELSE
            tmp$ = g$
         END IF
         IF tmp$ < lst$ THEN
            PRINT #7, tmp$
         END IF
         lst$ = tmp$
         f$ = g$
         PRINT g$: a = a + 1: b = b + LEN(g$)
         t$ = CHR$(LEN(g$))
         tl = tit AND 255
         th = tit \ 256
         titptr$ = CHR$(tl) + CHR$(th)
         PUT #5, , titptr$
         PUT #2, , t$
         PUT #2, , g$
         currtitle = tit
         tit = tit + LEN(g$) + 1
         num = num OR 32768   'Display this
      END IF
      strt = LEN(f$)
      IF LEFT$(c$, strt) = f$ AND strt > 0 THEN
         PRINT "Series ";
         h$ = MID$(c$, strt + 2, 1)
         seriesno = VAL(MID$(c$, strt + 3))
         IF h$ = "(" OR h$ = "[" THEN
            episodeno = VAL(MID$(c$, strt + 6))
            strtdesc = INSTR(strt + 7, c$, ")") + 1
            dash = INSTR(strt + 7, c$, "-")
            IF dash THEN
               IF dash < strtdesc THEN
                  episodeno = episodeno + 128
                  nxtepisode = VAL(MID$(c$, dash + 1))
               END IF
            END IF
            desc$ = MID$(c$, strtdesc)
         END IF
         IF h$ = "(" THEN
            series = 2
            PRINT f$; "("; seriesno; ")"; episodeno AND 127;
            IF nxtepisode > 0 AND nxtepisode < 255 THEN PRINT "-"; nxtepisode;
            PRINT ")";
         ELSEIF h$ = "[" THEN
            series = 3
            PRINT f$; "["; seriesno; "]"; episodeno AND 127;
            IF nxtepisode > 0 AND nxtepisode < 255 THEN PRINT "-"; nxtepisode;
            PRINT ")";
         ELSEIF h$ = "-" THEN
            series = 0
            PRINT f$; " - ";
            strtdesc = strt + 4
            desc$ = MID$(c$, strtdesc)
         ELSE
            series = 1
            episodeno = VAL(MID$(c$, strt + 2))
            PRINT f$; episodeno;
            strtdesc = INSTR(strt + 3, c$, ")") + 1
            dash = INSTR(strt + 3, c$, "-")
            IF dash THEN
               IF dash < strtdesc THEN
                  episodeno = episodeno + 128
                  nxtepisode = VAL(MID$(c$, dash + 1))
               END IF
            END IF
            IF nxtepisode > 0 AND nxtepisode < 255 THEN PRINT "-"; nxtepisode;
            PRINT ")";
            desc$ = MID$(c$, strtdesc)
         END IF
         WHILE LEFT$(desc$, 1) = " "
            desc$ = MID$(desc$, 2)
         WEND
         PRINT desc$
         PRINT #8, desc$
         desc$ = cmpstr$(desc$)
         c = c + LEN(desc$)
      ELSE
         IF last$ <> c$ THEN
            IF LEFT$(c$, 2) = "A " THEN
               tmp$ = MID$(c$, 3)
            ELSEIF LEFT$(c$, 4) = "THE " THEN
               tmp$ = MID$(c$, 5)
            ELSE
               tmp$ = c$
            END IF
            IF tmp$ < lst$ THEN
               PRINT #7, tmp$
            END IF
            lst$ = tmp$
            f$ = c$
            PRINT c$
            a = a + 1
            b = b + LEN(c$)
            currtitle = tit
            t$ = CHR$(LEN(c$))
            tl = tit AND 255
            th = tit \ 256
            titptr$ = CHR$(tl) + CHR$(th)
            PUT #5, , titptr$
            PUT #2, , t$
            PUT #2, , c$
            tit = tit + LEN(c$) + 1
            last$ = c$
            num = num OR 32768   'Display this
         END IF
         desc$ = ""
      END IF
'      PRINT b$; c$; num;
      IF format AND 1 THEN PRINT "Animated ";
      IF format AND 2 THEN PRINT "Widescreen ";
      IF format AND 4 THEN PRINT "Foreign ";
      IF format AND 8 THEN PRINT "Subtitled ";
      IF format AND 16 THEN PRINT "Missing ";
      IF format AND 32 THEN PRINT "Stereo ";
      IF format AND 64 THEN PRINT "Long Play ";
      IF tapes%(num AND 8191) AND svhsbit THEN PRINT "S-VHS ";
      PRINT
      tl = currtitle AND 255
      th = currtitle \ 256
      dl = (descptr \ 2) AND 255
      dh = descptr \ 512
      nh = (num \ 256) + series * 32
      nl = num AND 255
      'IF series THEN nh = nh OR 32
      IF seriesno < 0 THEN seriesno = 0
      IF series > 1 THEN
         s$ = CHR$(seriesno)
      ELSE
         s$ = ""
      END IF
      IF series THEN
         s$ = s$ + CHR$(episodeno)
         IF episodeno > 127 THEN
            s$ = s$ + CHR$(nxtepisode)
         END IF
      END IF
      's$ = CHR$(seriesno+series*64) + CHR$(episodeno) + CHR$(nxtepisode)
      l = LEN(desc$)
      tmp = l + LEN(s$)
      IF tmp \ 2 <> tmp / 2 THEN desc$ = desc$ + CHR$(0): tmp = tmp + 1
      'IF tmp \ 3 <> tmp / 3 THEN desc$ = desc$ + " ": tmp = tmp + 1
      'IF tmp \ 3 <> tmp / 3 THEN desc$ = desc$ + " "
      IF desc$ = "" THEN desc$ = zero2$
      s$ = s$ + desc$   's$ + CHR$(l) + desc$
      'PUT #3, , s$
      'descptr = descptr + LEN(s$)
      'desc = desc + LEN(s$) + 1
      IF s$ = nuldesc$ THEN
         dl = 0
         dh = 0
      ELSEIF s$ <> t$ THEN
         PUT #3, , s$
         descptr = descptr + LEN(s$)
         desc = desc + LEN(s$) '+ 1
         t$ = s$
         savdl = dl
         savdh = dh
      ELSE
         dl = savdl
         dh = savdh
      END IF
      r$ = CHR$(tl) + CHR$(th) + CHR$(dl) + CHR$(dh)
      r$ = r$ + CHR$(nl) + CHR$(nh) + CHR$(format)
      PUT #4, , r$
   END IF
WEND

PUT #3, , zero2$
PUT #4, , zero2$
PUT #4, , zero2$
PUT #4, , zero2$
PUT #4, , zero$

PRINT a, b, c
CLOSE #1
CLOSE #2
CLOSE #3
b = LOF(4) \ 7
CLOSE #4
CLOSE #8

a = a - 1
tl = a AND 255
th = a \ 256
titptr$ = CHR$(tl) + CHR$(th)
PUT #5, 1, titptr$   'Number of titles
tl = b AND 255
th = b \ 256
titptr$ = CHR$(tl) + CHR$(th)
PUT #5, 3, titptr$   'Number of ptrs
CLOSE #5

tapes%(0) = 0
FOR a = 0 TO maxtape
   a$ = CHR$(tapes%(a))
   PUT #6, , a$
NEXT a
CLOSE #6
CLOSE #7

SYSTEM

FUNCTION cmpstr$ (a$)
'b$ = UCASE$(a$)
b$ = a$
cmp = 0
WHILE b$ <> ""
   c$ = LEFT$(b$, 1)
   IF LEFT$(b$, 4) = "AND " THEN
      cmp(cmp) = 14
      cmp(cmp + 1) = 13
      cmp = cmp + 2
      b$ = MID$(b$, 5)
      c$ = ""
   END IF
   IF LEFT$(b$, 4) = "THE " THEN
      cmp(cmp) = 14
      cmp(cmp + 1) = 12
      cmp = cmp + 2
      b$ = MID$(b$, 5)
      c$ = ""
   END IF
   IF c$ <> "" THEN
      b$ = MID$(b$, 2)
      IF INSTR(codes$(0), c$) THEN
         IF INSTR(codes$(1), c$) THEN
            cmp(cmp) = INSTR(codes$(1), c$) - 1
            cmp = cmp + 1
         ELSEIF INSTR(codes$(2), c$) THEN
            cmp(cmp) = 14
            cmp(cmp + 1) = INSTR(codes$(2), c$) - 1
            cmp = cmp + 2
         ELSEIF INSTR(codes$(3), c$) THEN
            cmp(cmp) = 15
            cmp(cmp + 1) = INSTR(codes$(3), c$) - 1
            cmp = cmp + 2
         END IF
      ELSE
         cmp(cmp) = 14
         cmp(cmp + 1) = 15
         cmp(cmp + 2) = ASC(c$) \ 16
         cmp(cmp + 3) = ASC(c$) AND 15
         cmp = cmp + 4
      END IF
   END IF
WEND
PRINT LEN(a$), cmp
c$ = CHR$(cmp)
cmp(cmp) = 0
FOR a = 0 TO cmp STEP 2
   c$ = c$ + CHR$(cmp(a) * 16 + cmp(a + 1))
NEXT
cmpstr$ = c$
END FUNCTION

FUNCTION decmpstr$ (a$)
b$ = ""
c = ASC(a$)
d = 2
b = 2
FOR a = 0 TO LEN(a$) * 2 - 3 STEP 2
   cmp(a) = ASC(MID$(a$, b)) \ 16
   cmp(a + 1) = ASC(MID$(a$, b)) AND 15
   b = b + 1
NEXT a
cmp = 0
WHILE cmp < c
   IF cmp(cmp) < 14 THEN
      b$ = b$ + MID$(codes$(1), cmp(cmp) + 1, 1)
      cmp = cmp + 1
   ELSEIF cmp(cmp) = 14 THEN
      d = cmp(cmp + 1)
      cmp = cmp + 2
      IF d < 12 THEN
         b$ = b$ + MID$(codes$(2), d + 1, 1)
      ELSEIF d = 12 THEN
         b$ = b$ + "THE "
      ELSEIF d = 13 THEN
         b$ = b$ + "AND "
      ELSEIF d = 15 THEN
         b$ = b$ + CHR$(cmp(cmp) * 16 + cmp(cmp + 1))
         cmp = cmp + 2
      END IF
   ELSEIF cmp(cmp) = 15 THEN
      b$ = b$ + MID$(codes$(3), cmp(cmp + 1) + 1, 1)
      cmp = cmp + 2
   END IF
WEND
decmpstr$ = b$

END FUNCTION

FUNCTION remove$ (in$, del$)
a = INSTR(in$, del$)
IF a THEN
   b = a + LEN(del$)
   fin = 1
   WHILE fin AND a > 0
      a = a - 1
      fin = 0
      IF a THEN
         IF MID$(in$, a, 1) = "(" THEN fin = 1
         IF MID$(in$, a, 1) = " " THEN fin = 1
      END IF
   WEND
   IF MID$(in$, b, 1) = ")" THEN b = b + 1
   IF a THEN a$ = LEFT$(in$, a)
   remove$ = a$ + MID$(in$, b)
END IF
END FUNCTION

DEFSNG A-Z
FUNCTION title$ (a$, b$)
   a = INSTR(a$, ")")
   b = INSTR(a$, "]")
   c = INSTR(a$, " - ")
   IF a = 0 THEN a = b
   IF b = 0 THEN b = a
   IF c = 0 THEN c = b
   IF b < a THEN a = b
   IF c < a OR (a = 0 AND c > 0) THEN
      a = c - 1
      IF a > 0 THEN
         t$ = LEFT$(a$, a)
      ELSE
         t$ = ""
      END IF

      IF t$ = b$ THEN t$ = ""
      IF b$ = "" AND t$ = "" THEN t$ = truncate$(a$)

      title$ = t$
   ELSE
      a = a - 1

      IF a > 0 THEN
         c$ = MID$(a$, a, 1)
         c = INSTR("0123456789", c$)
      ELSE
         c = 0
      END IF

      IF c THEN
         WHILE a > 1 AND MID$(a$, a, 1) <> " "
            a = a - 1
         WEND
      END IF

      IF c THEN
         t$ = LEFT$(a$, a - 1)
      ELSE
         t$ = ""
      END IF

      IF t$ = b$ THEN t$ = ""
      IF b$ = "" AND t$ = "" THEN t$ = truncate$(a$)

      title$ = t$
  
   END IF

END FUNCTION

DEFLNG A-Z
FUNCTION truncate$ (a$)
a = LEN(a$)
IF a THEN
   fin = 1
   WHILE fin
      fin = 0
      IF a THEN
         IF MID$(a$, a, 1) = " " THEN fin = 1: a = a - 1
         IF MID$(a$, a, 1) = "-" THEN fin = 1: a = a - 1
      END IF
   WEND
   truncate$ = LEFT$(a$, a)
END IF
END FUNCTION

