PrinThru (c)1999 Kevin Millican
===============================

This program is designed to run on a PC and pass the input from a serial port through to the printer port. Typical application is to print from a palmtop device through its serial connection with a PC.

The program is similar in function to Psion's RPRINT utility but will work properly with a series 5 as well as a 3c/3mx and at higher speeds. It should also work with any other device capable of printing directly to a serial port.

If you run the program without any command line parameters then you can press <space> to alter the default settings.

By default PrinThru uses the following settings :-

COM2
19200 baud
IRQ 3 (the default for COM2)
LPT1: (the Printer device name)
30 seconds print timeout.

The settings you select are saved for future use automatically in a file called PrinThru.INF

If you use ANY command line parameter the option to change settings is skipped but any existing PrinThru.INF settings will still be used.

The program will capture any input to the specified COM port and create a data file (PrinThru.DAT) on the host PC. When you press <space> or when no data has been received for the length of the specified timeout, then the entire capture file is binary copied to the desired printer port.

The beauty of this approach is that the program does not hog the printer port and can therefore be left running in the background if the serial port is not required for other tasks.

In tests I only achieved reliable printing at 115200 baud on a Pentium with fast UARTs.
57600 worked on a DX33 (16550AF UART) provided I ran it full screen.
19200 is reliable on a DX33 running a DOS session in a window.

HOWEVER - THERE IS LITTLE POINT IN USING SPEEDS ABOVE 19200 WHEN PRINTING WITH A SERIES 5. Eg. When printing a Word file with a moderate-sized bitmap at 57600 baud, PrinThru reported waiting times up to 20 seconds during the print process. At slower speeds the waiting times decrease but the overall printing time is hardly affected.

This program is FREEWARE - enjoy.

Comments suggestions to :-

kmillican@compuserve.com