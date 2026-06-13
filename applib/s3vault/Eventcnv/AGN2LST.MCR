rem AGN2LST.MCR v1.0
rem Macro for use with Event Convert.
rem This macro prints this months events to a file using the 
rem General printer device driver.

PROC macro:
UseApp:("Agenda")
SendKey:(633,8,1) rem Psion-Y
SendKey:(256,0,1) rem Up
SendKey:(256,0,1) rem Up
SendKey:(9,0,1) rem Tab
SendKey:(103,0,1) rem G  rem select general print driver
SendKey:(13,0,1) rem Enter
SendKey:(257,0,1) rem Down
SendKey:(9,0,1) rem Tab
SendKey:(102,0,1) rem F  rem select print to file 
SendKey:(13,0,1) rem Enter
SendKey:(13,0,1) rem Enter
SendKey:(624,8,1) rem Psion-P
SendKey:(116,0,1) rem T
SendKey:(116,0,1) rem T  rem select this month calendar
SendKey:(256,0,1) rem Up
SendKey:(9,0,1) rem Tab
SendKey:(256,0,1) rem Up
SendKey:(258,0,1) rem Right  rem this line deselects to-do entries
SendKey:(13,0,1) rem Enter
SendKey:(13,0,1) rem Enter  rem start printing
SendKey:(633,8,1) rem Psion-Y
SendKey:(256,0,1) rem Up
SendKey:(256,0,1) rem Up
SendKey:(9,0,1) rem Tab
SendKey:(112,0,1) rem P
SendKey:(258,0,1) rem Right  rem select Psi Print
SendKey:(13,0,1) rem Enter
SendKey:(257,0,1) rem Down
SendKey:(9,0,1) rem Tab
SendKey:(112,0,1) rem P  rem select parallel printer port
SendKey:(13,0,1) rem Enter
SendKey:(13,0,1) rem Enter
runappc: ("RunOpl","Eventcnv.opo")  rem run Event Convert
ENDP
