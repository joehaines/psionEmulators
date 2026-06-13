Version 1.10e of MonthVu - Update Release

This update adds an option to start the calendar on any day of the week, and to ignore all non-repeating entries more than a specified number of days in the past. This latter feature will help those users with very large agenda files that had difficulty loading under previous versions on account of overflow

NEW USERS:

Read the installation notes in the Reference manual, and if you encounter any problems in the installation, read the END of the manual that has trouble-shooting notes. If you are in too much of a hurry to read the manual, just copy the MONTHVU.OPA or MONTHVUS.OPA (Siena) file into the APP directory on your local drive and install it from the system screen. For most users, that's all that's needed, but if you encounter overflow, you'll have to adjust the MAX SIZE OF ITEM TEXT in the 2nd preference screen (see end of manual for notes on how to tune this value).

REGISTERED/EXISTING MonthVu USERS - READ THIS CAREFULLY!!!!!

1. Before upgrading to this new version, make a note of your registration ID and any custom preferences you may have set and then delete the following 3 files in the \app\ directory (the Bin and PRF files will only be there if you are upgrading from an old version of MonthVu).

	MONTHVU.OPA
	MONTHVU.BIN
	MONTHVU.PRF

Also delete all files in \APP\MONTHVU if there is a MONTHVU sub-directory underneath APP.

2. Copy the MONTHVU.OPA file into the appropriate \app directory.

3. If you have an earlier version of MonthVu installed, remove the application from the System screen and re-install it.

4. Run MonthVu. If there are no problems, restore your preferences, and re-insert your Registration ID. You may also want to look at the About dialog and make your title heaps and repeat heaps somewhat smaller as MonthVu will have made them as large as memory permits. Note that MonthVu will automatically create a MONTHVU sub-directory under APP and all files except for the MONTHVU.OPA file will be placed in that sub-directory. If you get an overflow on your heap, try adjusting the MAX SIZE OF ITEM TEXT preference item down to say 40 or so. If that's still not enough, take it down to 15 or even 10 to get it to work. If you're willing to "fiddle", you MIGHT be able to add as much as 1,000 to 1,500 bytes to the heap, but make sure you can still get into the More Preferences dialog without a NO SYSTEM MEMORY error. 

5. NON-ENGLISH PSION USERS: Read the section on the Preference Item: <<Day View, Jump To & Switch Accel Keys>> as you will have to change the letters there to get the Psion/G (goto Agenda) feature to work properly in whatever language your Psion is operating in.

6. If you have problems installing this new version, consult the manual, or immediately send an email to Pimlico Software: cesd@gorilla-haven.org

Note that the manual is in PSION Word format - download to your Psion to read it, or use Psi-Win to convert it to MS Word format, etc.  On a MAC, make sure that you do not have it strip the line-feeds in the file or the Psion will hang when it tries to read the file!

Regards,
CESD