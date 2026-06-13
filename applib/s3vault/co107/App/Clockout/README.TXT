Clockout
Automated timesheet entry routines.  This has been
renamed from "TimeKeeper" which is an established
Protronics application.

This program assists in the maintenance of daily
timesheets for work.  It also maintains leave.
Although it is tailored to the rules of my
employer, you may find it useful as a basis for
own system.

Installation
MacSys must be installed on your system.
Copy the in this packages onto a Series 3a or
higher, in the following locations :

\app\clockout.opa
\app\clockout\clockout.pic (optional)
\app\clockout\clockout.rsc (optional)
\app\clockout\yearskel.spr Skeleton year summary sheet
\app\clockout\weekskl1.spr Skeleton weekly sheet
\app\clockout\weekskl2.spr Skeleton weekly sheet
\macro\mco\coedit.mco
\macro\mco\coeditl.mco
\macro\mco\cologin.mco

Example spreadsheets should be installed in
directory \spr\clockout\, and can be tailored
to your own requirements :

time98.spr		Example year summary
time98xx.spr	Example timesheet, where xx vaaries with each release

Install the timekeeper application wherever
suitable on the system screen, and assign hotkeys
as follows (or as preferred) :

coedit	Ctrl-T - Edit this week's timesheet
coeditl	Ctrl-W - Edit last week's timesheet
cologin	Ctrl-L - Automatically clock-in or out


Mike Smith
