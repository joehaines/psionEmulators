The PC RCom v1 disks contain these directories and programs:

\WIN31\*.*            The RCom files that are installed on
                      Windows-based PCs by the SETUP.EXE program.
\DOS\*.*              The RCom files that are installed on DOS-based
                      PCs by the DOSSETUP.BAT installation batch file.
                      The files that are common to both Windows and DOS
                      setups. A batch program to install the RTF drivers.
\PRINTDRV\*.*         Extra printer drivers for the Series 3/3a
\RPRINT\*.*           A program for printing from the Series 3/MC/HC
                      to a printer that is attached to the PC and a
                      batch file that can be used to install the extra
                      printer drivers.
\RTF\*.DYL            RTF (Microsoft Word file) handlers for the
                      Series 3/3a.
\OPL\*.EXE            Programs that can be used to translate OPL
                      files to OPO files which can be run on the
                      Series 3/MC/HC.
\SCR\*.SCR            Sample scripts for use with the Comms
                      application supplied on the 3Link lead (for the
                      Series 3/3a).

Each directory has its own README.TXT file of documentation.

TROUBLESHOOTING
---------------
If RCom has problems running on a networked PC, check with your Network
Administrator that your network adaptor is using a different "IRQ" to your
serial ports. Some network adaptors are set to use IRQ3 which is usually
used by the COM2 serial port. (IRQ4 is for COM1.) If you have such a clash
this can cause problems for one or even both of the serial ports. Change your
network adaptor to use a different IRQ: older 8-bit adaptors should try
IRQs 5 or 7, while 16-bit adaptors can also try IRQs 10 or 11.

The System screen
     
If the System screen is displayed while the link is active then file 
transfers can be slowed down. This is because the System screen 
detects when a file is created and scans ALL the disks to determine 
what has changed. A good idea is to display something else, such as 
the Time application, while using RCom, as this stops the System 
screen from scanning.
     
The first time RCom tries to establish a link with your computer, it 
has to copy a program file down to it. If this takes a long time, it 
may well be because the System screen is displayed: display something
else, such as Time, and RCom should be able to establish the link 
quickly.

