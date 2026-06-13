Clock5   v1.81  (1999-10-14)
======================================

Clock 5 är en Series 5/5mx/Geofox/Ericsson MC218 lösenordsskyddad skärmsläckare 
- ganska lik det som finns för Mac och Windows. Naturligtvis behöver du väl inte 
en skärmsläckare på samma sätt för din Psion/'fox/Ericsson - varför skulle 
skärmen behöver släckas? Med detta i bakhuvudet tyckte jag åtminstone att 
programmet skulle kunna göra något nyttigt - att visa en stor klocka kanske...


Idéen baseras huvudsakligen på Dan Comiskeys "Clock"-program för 3a/c. Ett 
större antal utökningar planeras för kommande versioner, så besök Pscience5 på:
http://ourworld.compuserve.com/homepages/martin_guthrie 
regelbundet för nya versioner och mer information.

Installation
============
Installation sker nu via den standardiserade .sis-fil-metoden. 
Det finns sex filer:

Clock5.sis		(Clock5:s installationsfil)
Alarm.sis		(Symbians opx-fil)
SysRAM1.sis		(Psions opx-fil)
Systinfo.sis		(RMRs opx-fil)
ReadMe.txt		(denna fil)
Changes.txt		(ändringar sedan senaste version)

Clock5.sis, Alarm.sis, SysRAM1.sis, och Systinfo.sis kan installeras på din 
maskin antingen genom att använda Psions EPOC installationsprogram (levereras 
med PsiWin 2.1/2.2/2.3) eller genom att använda Add/Remove-ikonen i Psions 
kontrollpanel om du redan har (t.ex.) Email installerad. Om det senare är fallet 
är det bara att kopiera .sis-filerna till din Psion och antingen leta upp dem 
med hjälp av Add/Remove-ikonen i Psions kontrollpanel eller bara dubbelklicka 
dem för att automatiskt köra installationsrutinen

Under installationen av Clock5 så har du möjlighet att installera ljudfiler. 
Dessa gör det möjligt att använda Clock5 som en talande klocka i läget "John 
Blund". (OBS! Maskinens Owner Information-skärm måste vara avstängd för att 
denna finess skall fungera). Dessa filer är dock ganska stora (~100 kbyte), så 
möjlighet ges att inte installera dem. Clock5 fungerar utan dem, utom just denna 
finess.

Clock5:s ikon skall nu finnas i din Extrapanel, klar att köra.

Om du vill lägga till egna logo förutom de inbyggda i Clock5, så är det bara att 
skapa ett nytt bibliotek kallat "Logos" i det nya System\Apps\Clock5\-
biblioteket (vilken diskenhet beror på var du installerat Clock5) och placera 
.mbm-filerna i det nya biblioteket. De kommer att hittas automatiskt av Clock5. 
Det finns några exempellogo på Pscience5 webplats.

"Incompatible opx version" under installationen
===============================================
Om du får detta felmeddelande när du installerar Symbians OPX-filer, så kan en 
av dessa två saker vara problemet:

1) Du har en äldre version av Systinfo.opx eller Sysram1.opx på din Psion och 
detta förorsakar konflikt med den nya versionen. Symbian har gett ut nya 
versioner av dessa opx-filer med ER5/5mx-kompatibilitet. De nya filerna kommer 
att fungera bra på Geofox/S5 – men alla såda maskiner tenderar att bli 
förvillade om den finns en kopia av den gamla opx-filen någonstans i maskinen 
parallellt med de(n) nya.

Därför rekommenderar jag att leta efter äldre versioner av sysram1.opx och 
Systinfo.opx, antingen i c:/system/opx/ eller d:/system/opx/ innan Clock5 
installeras. Förresten, äldre program fungerar fint med de nya versionerna av 
dessa opx-filer.

2) Ett program körs just nu som använder sig av Sysram1.opx eller Systinfo.opx 
när du försöker installera Clock5 (t.ex Macro5). Stänga dessa program innan du 
installerar Clock5.


Vänliga hälsningar,

Martin Guthrie
martin_guthrie@csi.com

