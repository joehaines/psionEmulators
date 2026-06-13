RealMaps v5.13
==============

CHANGES FROM v5.12:-
Improved map referencing and translation - faster and more accurate with skewed maps.

Grid option improved to work better in degrees+minutes(+seconds) mode. The grid spacing and colour can also be specified.

Added support for the additional serial port on netBook and Series 7. This should allow PCMCIA and CF (via PCMCIA adapter) GPS units to be used.

Added a facility to send a text file to the serial port before beginning moving map mode. The text file should be named rs232.txt and copied to the \system\apps\realmaps\ folder. This allows commands to be sent to the GPS (eg. to switch on NMEA mode on some SIRF units) prior to reading from the port.

Added a colour icon for netBook and Series 7 users.

Added automatic analysis of map referencing to spot and advise most common errors when defining points, loading a map, or manually when 'D' is pressed to display the information.

Added support for UTM coordinates.

Kevin Millican
Email: realmaps@millican.info