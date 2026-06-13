GBattery v.1.31

Features:
- graphical battery usage monitor
- alarm when power left drops below preset value
- estimates hours left
- estimates when to replace batteries
- monitors short time power usage
- optional self discharge setting for rechargebles
- many user definable settings
- build in chime function
- freeware

About
=====
No explanation needed

Reset Short Term Usage
======================
Resets the short term usage to zero.

Refresh
=======
Forces a screen update. The screen is automatically updated approxiamatly every 12 seconds
when GBattery is in the foreground. This option forces an instant screen update.

Chime status
============
Shows if chime is set and when next chime will be.

Exit
====
Terminates the program.

Chime
=====
Enable or disable chime function. Specify what hours to enable chime and if chime should
be on during weekends.
If chime is set, it will be trigger 20 seconds before the hour. This is done to prevent
interfernce with the standard alarm function.
Psion will automatically switch off if it was switched on due to the chime. In the most
unlikely case that you switched on your Psion just before the chime it might also perform
a automatic switch off.

Chime Status
============
Shows the status of the Chime.

Be Quiet
========
Keep quiet for a certain time. Usefull when attending a meeting and you don not want to
be disturbed. Notice that alarms will still occur, only the sound is off.

Battery
=======
Set battery type. The nominal capacity is the number of mAh. This value is printed on most
recharchables. The actual value can be lower than listed because of permenant power loss
of recharcheables.
For Alkalines this value will be most likely between 1300 and 2200.
Self dicharge is the avarage power loss per day. Rechargebles suffer from self dicharge
which is approxiatly 1% of their nominal capacity (a fully charge cell will be empty in
a couple of months even when you do not use it). For alkalines this value should be zero.

Alarm
=====
Define how often GBattery should evaluate your batteries when the program is in the
background (in the foreground re-evaluation is preset on 12 seconds).
Define when to trigger the first alarm and every next alarm. The next alarm is triggered
when power drops below first alarm - k * repeat interval (k = 1,2,3...).
The 'check batteries every..' in combination with the alarm settings determine the
precission of the alarm. Suppose you set the alarm on 60 minutes left and your the check
interval is set on 60 minutes. In the worst case you will never hear an alarm because the
check (in the background ) can just be done when the power left drops below 60 minutes. The
next check interval will be when the batteries are almost empty.
Note that every time the Psion is switched on, a check will be performed (except when
the Psion is switched on due to a chime, because this could interfer with the auto
switch off after the chime).

Scale
=====
Redefine the maximum scale of the gauges.
The gauges can also be resized (not re scaled) with ctrl-menu.

Short Term Usage
================
Determines how short term usage measurement is done and how it is represented on the screen.
There are two representations: one with the global usage as a thick bar and the short
term usage as a small bar above the other representation is the other way around.
When the global usage is shown as main bar then the listed usage on the right hand of
the gauge represents the global usage. When the main bar represent the short term usage
then the time over whitch the short term usage is measured is listed on the right hand
side of the gauge.
Optional you can specify to reset the short term usage power every time the Psion is
switched on.

Hot keys
========
Specify if and what hot key you want to use. 
CTRL - <hot-key> will bring GBattery to the foreground.
CTRL - Shift <hot-key> will switch the system screen to the foreground.
When these last two function interfere with other programs you should first start the
other programs and then start GBattery. GBattery will try to assign the hot keys and if
it fails it will warn you and continue without hot-keys.
When GBattery is in the foreground you can switch it back to the background with Esc.

Cntrl-menu
==========
Changes the size of the status window.