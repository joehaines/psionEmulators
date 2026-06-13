Psion Serie 5 Message Suite (1.10F)
===================================
DE Version (Version 029)
Juni 1998

ÄUSSERST WICHTIG: Message Suite ist nicht zur "EasyFax" Software kompatibel. 
Ist eine Version von EasyFax auf Ihrem Serie 5 installiert, so müssen Sie 
diese VOR dem Installieren von Message Suite entfernen. Anleitung siehe unter 
"Was mache ich, wenn EasyFax installiert ist".

Bei der Installation von Message Suite werden der Systemsteuerung des Serie 5 
neue Elemente hinzugefügt (Netzwerk, Modem). In der Systemsteuerung und dem 
E-mail-Programm müssen gewisse Einstellungen vorgenommen werden, bevor Sie 
die Message Suite Programme nutzen können. Weitere Einzelheiten hierzu finden 
Sie im Message Suite Benutzerhandbuch.

Wenn Sie eine frühere Version von Message Suite haben...
========================================================

Haben Sie die Message Suite Version 1.0 auf Ihrem Serie 5, so müssen Sie sie 
vor dem Installieren dieser Version nicht entfernen. Diese Installation ersetzt 
frühere Versionen von Message Suite automatisch. Da die Informationen zu 
Einstellungen und Service Provider vom Upgrade nicht betroffen sind, brauchen 
Sie Message Suite nicht erneut einzurichten.

Vor dem Installieren zusätzlicher Software oder dem Ändern wichtiger Einstellungen 
wird dringend empfohlen, den Serie 5 rückzusichern.

Message Suite über einen PC mit Win 95/NT installieren
======================================================

Auf Ihrem PC muß PsiWin v2.X installiert sein.

Message Suite, PsiWin 2.1 und andere Psion-Programme beinhalten jetzt "EPOC Install", 
über das man vom PC auf dem Serie 5 Programme installieren kann. 

Um festzustellen, ob Sie bereits EPOC Install haben, klicken Sie mit der rechten 
Maustaste auf das Symbol "Psion Arbeitsplatz" auf dem Desktop und prüfen Sie, ob 
es die Option "Neues Programm installieren" gibt. Andernfalls sollten Sie zuerst 
SETUP.EXE ausführen, um das Programm "EPOC Install" zu installieren. 

Nun können Sie Message Suite, die Service-Provider-Vorlagen oder sonstigen Programme 
mit der Option "Neues Programm installieren" oder durch Doppelklicken einer .SIS-Datei 
im Windows Explorer auf Ihrem Serie 5 installieren. 

Wenn Sie zum ersten Mal ein Programm oder eine Datei mit EPOC Install installieren, 
wird der Systemsteuerung des Serie 5 das Symbol "Software" hinzugefügt. Weitere 
Einzelheiten hierzu finden Sie im Message Suite Benutzerhandbuch.

Message Suite über andere Computer installieren
===============================================
Auf Ihrem Computer muß Software zum Kopieren von Dateien auf Ihren Serie 5 
installiert sein, und Sie brauchen ein geeignetes Kabel, um Ihren Computer 
an den Serie 5 anzuschließen.
 
Wenn Sie einen PC mit Windows 3.xx haben, können Sie PsiWin 1.xx benutzen.
Wenn Sie einen Macintosh-Computer haben, können Sie PsiMac oder Psion MacConnect benutzen.

Wenn Sie zum ersten Mal Software auf Ihrem Serie 5 installieren:

Schließen Sie den Serie 5 an, und kopieren Sie die Datei instexe.exe auf den Serie 5. 
Am besten kopiert man diese Datei ins Wurzelverzeichnis "\". Suchen Sie dann diese 
Datei im Systembildschirm, wählen Sie sie aus, und drücken Sie auf Enter, um sie 
auszuführen. Nach einigen Minuten verschwindet die Datei vom Bildschirm, und das Symbol 
"Software" wird in der Systemsteuerung des Serie 5 installiert.

Nur für MacConnect-Benutzer:

Sie können Message Suite mit der Installationsfunktion von MacConnect von der 
.SIS-Datei installieren. Wählen Sie im Psion-Menü PsiTools, drücken Sie auf 
"Installieren", und gehen Sie zu dem Ordner, in dem Sie Msgsuite.sis gespeichert 
haben. Wählen Sie diese Datei und klicken Sie auf "Öffnen". Befolgen Sie nun die 
auf dem Bildschirm erscheinenden Anweisungen, um die Message Suite Programme auf 
Ihrem Serie 5 zu installieren.

Alle anderen Benutzer:

Kopieren Sie die Datei msgsuite.sis auf den Serie 5 (es ist wiederum am besten, 
sie in den Ordner "\" zu kopieren). Wählen Sie die Datei im Systembildschirm und 
drücken Sie auf Enter. Nun beginnt die Installation der Message Suite Programme. 
Befolgen Sie die auf dem Bildschirm erscheinenden Anweisungen, und stellen Sie 
sicher, daß Sie dieselbe Disk für alle Message Suite Komponenten verwenden. 
Die .SIS-Datei wird nach beendeter Installation gelöscht, um Speicher zu sparen.

Kommunikations-Link verwenden
=============================

Zum Installieren vom Programmen mit EPOC Install müssen Sie auf Ihrem Serie 5 
den Kommunikations-Link benutzen. Dieser muß auf 'Kabel' gestellt, und der 
Serie 5 an den PC angeschlossen werden.

Für die Message Suite Programme muß der Link auf 'Aus' gestellt werden. Ist er 
in Gebrauch (z.B. auf 'Kabel' gestellt), so wird er beim Herstellen der Verbindung 
zum Internet automatisch auf 'Aus' gestellt. Vor dem erneuten Anschluß an den 
PC muß er daher wieder eingestellt werden.

Was mache ich, wenn EasyFax installiert ist?
============================================

Haben Sie zuvor EasyFax installiert, so sollten Sie die folgenden Dateien löschen, 
damit das Senden und Empfangen von Faxen ordnungsgemäß erfolgen kann:

   \SYSTEM\APPS\EASYFAX\EASYFAX.AIF
   \SYSTEM\APPS\EASYFAX\EASYFAX.APP
   \SYSTEM\APPS\EASYFAX\EASYFAX.MBM
   \SYSTEM\APPS\EASYFAX\EASYFAX.R01
   \SYSTEM\APPS\EASYFAX\EASYFAX.R10
   \SYSTEM\APPS\EASYFAX\EASYFAX.HLP
   \SYSTEM\LIBS\FAXSTR.DLL
   \SYSTEM\LIBS\FAXTRANS.DLL
   \SYSTEM\LIBS\FAXVIEW.DLL
   \SYSTEM\PRINTERS\FAXPRINT.PDL
   \SYSTEM\PRINTERS\FAXPRINT.PDR
   \SYSTEM\PRINTERS\FAXPRINT.UDL

Löschen Sie außerdem alle alten Faxdateien, um Speicher zu sparen. Diese befinden 
sich gewöhnlich im Ordner "\Fax\", können aber auch anderswo gespeichert sein. 
Sie haben jedoch immer die Form "name.fax". Dabei ist zu beachten, daß gelöschte 
Fax verlorengehen, also empfiehlt es sich, alle Faxe, die Sie behalten wollen, 
auszudrucken.

Weitere Informationen
=====================

Sie können zahlreiche Standardmodems verwenden oder einen neuen Eintrag für Ihr 
Modem erzeugen. Message Suite enthält bereits Einträge für:

	- Hayes-kompatibles Modem (für alle nicht gelisteten Modems)
	- Psion Travel Modem
	- Psion Dacom Modem (z.B. Meteor, Surfer)
   	- US Robotics Sportster
	- Elsa Microlink	
	- Nokia Data Card
	- Direktverbindung (z.B. für einen Kabelanschluß an NT RAS)

Beachten Sie, daß Sie für Desktop-Modems Null-Modemadapter/-Kabel und Ihr Anschlußkabel 
brauchen.

Es sind Vorlagen für beliebte Internet Service Provider vorhanden. Sie sollten diese 
Dateien überprüfen, und wenn möglich für Ihren Provider anpassen. Es gibt u.a. die 
folgenden Vorlagen:

DE:	- Compuserve (Deutschland), Cybernet AG, Metronet, Psionworld (Deutschland), 	
	- T-Online (Deutsche Telekom AG)	

Weitere Informationen finden Sie unter http://www.psion.com/international