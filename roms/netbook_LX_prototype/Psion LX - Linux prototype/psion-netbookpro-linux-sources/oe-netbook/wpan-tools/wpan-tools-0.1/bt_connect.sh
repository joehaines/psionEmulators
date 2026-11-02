#!/bin/sh
#
# Establish a connection to a Bluetooth device for use by ppp/dialer
#
DEV=hci0
#
# Confirm with user
#
if ! zenity  --info --title="Bluetooth Setup"\
   --text="Connecting to your Bluetooth Device. It may take some time to scan."
then
	exit
fi
#
# Clean up any potential leftovers from last time
#
killall	rfcomm &> /dev/null 
rmmod rfcomm &> /dev/null
#
# check for /dev entry
#
if ! test -c /dev/rfcomm0 
then 
	mknod -m 666 /dev/rfcomm0 c 216 0
	mknod -m 666 /dev/rfcomm1 c 216 1
fi
modprobe rfcomm &> /dev/null
#
#
# check for the Bluetooth device
#
if ! hciconfig $DEV > /dev/null
then
	zenity --info --title="Bluetooth Setup"\
	 --text="No Bluetooth device Found."
	exit
fi
#
# Determine potential pairs
#
TGT=""
for T in `sdptool browse|grep Browsing|sed 's/Browsing //'|sed 's/ .*//'`
do
	echo $T
	TGT=`echo $TGT " " FALSE \"$T\"`
done
CHOICE=`zenity --list --radiolist\
	 --title="Please select your target device" \
	 --column="Choose" --column="Bluetooth Address" $TGT`
if test $CHOICE"Z" != "Z"
then
	#CHOICE=`echo $CHOICE|sed 's/"//g'`
	rfcomm connect 0 $CHOICE &
	zenity  --info --title="Bluetooth Setup"\
	   --text="You can now use /dev/rfcomm0 to make a dialup connection."

fi
