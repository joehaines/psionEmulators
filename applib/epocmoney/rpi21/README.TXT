RPI Calculator 2.1
(C) Richard Smedley 1997-1998
=============================

Have you ever wondered if your wages are really keeping up with the cost of
living, or whether constantly increasing prices are really justified?

RPI converts money from one year to another, so that you can compare how
much things used to cost with how much they cost now. For example, £25.00
in 1990 is worth the same as £29.40 in 1995.

This is extremely useful for things like seeing if your wages are really
keeping up with the cost of living, or finding out whether the value of
your house/car has increased by more than inflation.

Several datafiles are supplied, allowing you to perform a wide range of
calculations; including UK Pounds Sterling (RPI) and US Dollars (CPI). You
can even use the program for making future guestimations based on current
values!


INSTALLATION
------------

This program is compatible with all EPOC32 machines, such as the S5 and
Geofox.

1. Create the folder "System\Apps\RPI" on any drive. You may need
   to select `Show system folder' from your system prefs to do this.

2. Copy the following files into that folder:

    America.dat
    England.dat
    RPI.aif
    RPI.app
    RPI.hlp
    Wages_UK.dat

The RPI icon will now appear on your Extras bar.

The first time you use the program, the Options window will automatically
appear for you to select which data file you wish to use.


USING THE PROGRAM
-----------------

The RPI window contains the following boxes:

    Amount - the amount that you want to convert

    From - the "from" year

    To - the "to" year

Enter the required info into each box, and then press the return key.

For example, suppose that in 1996 you were paying £68.50 per week in rent,
and wanted to check whether or not your rent has increased in line with the
overall rate of inflation. Enter the following:

    Amount: 68.50
    From:   1996
    To:     1998

The program will then display the answer: £73.08

This means that if you're now paying more than £73.08 for your rent, then
you'll know that your rent has increased by *more* than the current rate
of inflation. Likewise, if you're paying less than this, you'll know that
it has increased by *less* than the rate of inflation.

(That example uses the English.dat datafile!)

You can perform similar calculations in the same way on things like, for
example, the price of a loaf of bread or a can of pop, or the amount that
you paid for your house compared with its current value, and so on. The
potential uses are almost infinite.


DATA FILES
----------

The data files used by this program are simply standard database files
which were created using the S5's inbuilt Database application.

  ENGLAND.DAT
    Source:         Retail Prices Index (UK)
    Years covered:  1900 to 1998 inclusive
    Available from: BBC2 teletext P276 (weekends only)

  AMERICA.DAT
    Source:         Consumer Price Index (US All Urban)
    Years covered:  1913 to 1997 inclusive

  WAGES_UK.DAT
    Source:         Average UK wages
    Years covered:  1976 to 1997 inclusive

Data files for other countries can be easily created if you have access to
the required info, by using one of the above files as a template.


DISTRIBUTION
------------

This program is freeware, but remains copyright (C) Richard Smedley 1998.
It may be freely distributed for non-commercial purposes providing all the
original files remain fully intact and unmodified. Standard disclaimer
applies; you use this program at your own risk.


CONTACTING THE AUTHOR
---------------------

If you have any comments about this program, or if you want to contact me
for any reason, then please feel free to write to me at this address:

    Richard Smedley
    PO Box 59
    Sutton-In-Ashfield
    Notts
    NG17 3HP
    England

    rsmedley@cix.co.uk
    http://www.cix.co.uk/~rsmedley/s5/


VERSION HISTORY
---------------

V2.1 (09.08.98)

  - Now allows you to predict future values, useful if you need to work
    one or two years into the future and want to make rough guestimates
    of how much things will be worth.

  - Added another new datafile: Wages_UK.dat, for making calculations
    based on average UK wage levels.

  - Whenever you select a different datafile, the values in the "From"
    and "To" boxes show the minimum & maximum years that you can use for
    your calculations.

  - Bug fix: Pressing the backspace key no longer opens the Help window.

V2.0 (25.06.98)

  - Completely re-wrote the program, making numerous internal changes.

  - Now includes a datafaile for performing American calculations (in US
    Dollars) as well as English calculations.

V1.0 (27.05.98)

  - The first release.
