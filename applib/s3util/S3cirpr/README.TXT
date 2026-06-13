Printing from a Series 3c to a JetEYE 9580

Date: January 16th 1997.

These files enable your Series 3c to print to an IrDA-compliant
IR printer or printer adapter, including the Extended Systems
JetEYE 9580

To use them:

Copy accessx.ldd and loadacx.img into the img directory of
your machine. You must be create this directory if it does not
already exist.

"Loadacx" should appear under the IMG icon in the System screen.
Run this program (highlight it and press Enter) and follow the
instructions to install the ir drivers. After this, IR printing
will be possible from any printing-enabled S3c application.

It is not necessary to keep the loader program (loadacx.img)
on the Psion once it has been run, although you will need to run
it again if the machine is ever reset.

To print using infrared: select the 'Print setup' menu option
within an application, and tab through the range of devices
until "Infrared" appears. Still within the print setup dialog,
select the correct printer model and align the Siena with the
printer's IR port. Then select 'Print' to start IrDA printing.

Note: accessx.ldd is an upgraded version of accessir.ldd that 
additionally enables printing to JetEYE 9580 printer adapter.
