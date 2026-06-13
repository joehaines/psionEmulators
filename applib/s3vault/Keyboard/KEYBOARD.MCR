PROC macro:
 global k%
 local a%,co%,a$(1),p%,e%,t%,ad%,s%,c%
 a%=ioopen(k%,"TTY:A",$100)
 rsset:(8,0,8,1,4) :rem 1200 8n1
 a$=" "
 ad%=addr(a$)+1
 do
  a%=ioread(k%,ad%,1)
  t%=peekb(ad%)
  useapp:("Active")
  if e%=2
   if t%=%H :a%=up%
   elseif t%=%P :a%=down%
   elseif t%=%I :a%=pgup%
   elseif t%=%Q :a%=pgdn%
   elseif t%=%M :a%=right%
   elseif t%=%O :a%=end%
   elseif t%=%K :a%=left%
   elseif t%=%G :a%=home%
   elseif t%=%S :a%=127 :s%=shift%
   endif
   a%=sendkey:(a%,s%+c%,1)
   if t%=%S :s%=0 :endif
   e%=0 :p%=0
  elseif p%=1
   a%=psion:(t%)
   p%=0
  elseif t%=0
   e%=2
  elseif t%=1
   p%=1
  elseif t%=3
   if c%=0
    c%=ctrl%
   else
    c%=0
   endif
  elseif t%=4
   a%=sendkey:(diamond%,s%,1)
  elseif t%=12
   a%=sendkey:(help%,0,1)
  elseif t%=15
   off
  elseif t%=19
   if s%=0
    s%=shift%
   else
    s%=0
   endif
  elseif t%=23
   a%=sendkey:(menu%,c%,1)
   p%=0
  elseif t%=25
   useapp:("Sys$shll")
   tofront:
  elseif t% > 31
   a%=sendkey:(t%,s%+c%,1)
  endif
 until a$=chr$(26)
 a%=ioclose(k%)
endp

proc rsset:(ba%,pa%,da%,st%,ha%)
 local frame%,srchar%(6),du%,er%
 frame%=da%-5
 srchar%(1)=ba% or (ba%*256)
 srchar%(2)=frame% or (pa%*256)
 srchar%(3)=(ha% and 255) or $1100
 srchar%(4)=$13
 er%=iow(k%,7,srchar%(1),du%)
 if er% :raise er% :endif
ENDP

