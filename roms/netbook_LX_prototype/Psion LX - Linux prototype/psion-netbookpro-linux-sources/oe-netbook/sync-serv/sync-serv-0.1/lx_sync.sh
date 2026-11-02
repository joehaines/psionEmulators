#!/bin/sh
#
# Runs the sync server, checking for pre-requisites first
#
cd $HOME/.gpe/

if ! test -f calendar
then
	zenity  --info --title="Desktop Synchronization Setup"\
	   --text="Calendar File Not Found - Please Run Calendar Application First"
	exit 1;
fi
if ! test -f contacts
then
	zenity  --info --title="Desktop Synchronization Setup"\
	   --text="Contacts File Not Found - Please Run Contacts Application First"
	exit 1;
fi
if ! test -f todo
then
	zenity  --info --title="Desktop Synchronization Setup"\
	   --text="Todo File Not Found - Please Run Todo Application First"
	exit 1;
fi
if ! test -f categories
then
	zenity  --info --title="Desktop Synchronization Setup"\
	   --text="Categories File Not Found - Please Run Todo Application First"
	exit 1;
fi
#
# on with the show
#
zenity  --info --title="Desktop Synchronization Setup"\
   --text="Please press Synchronize on your Windows machine to start" &
 
sync_serv 1101 > /dev/null

