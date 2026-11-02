#!/bin/sh
#
# Establish connection using IrDa for use by ppp/dialer
#
#DEV=pxa_ir
DEV=irda0
TF=/tmp/.irda.$$
#
# Confirm with user
#
if ! zenity  --info --title="IrDA Setup"\
   --text="Connecting to your IrDA Device. Check that it is on and in view of the IrDA port."
then
	exit
fi
#
# Initialize for IrDA connection
#
if ! test -c /dev/ircomm0 
then 
	mknod -m 666 /dev/ircomm0 c 161 0
	mknod -m 666 /dev/ircomm1 c 162 1
fi
modprobe ircomm-tty
modprobe ircomm
modprobe pxa_ir  
#
#
# Start the IrDA process
#
irattach $DEV -s &
#
# Confirm everything good to go 
# after a short sleep
#
sleep 1
ps>$TF
grep irattach $TF
if grep irattach $TF > /dev/null 
then
	zenity  --info --title="IrDA Setup"\
	   --text="You can now use /dev/ircomm0 to make a dialup connection."
else
	zenity --info --title="IrDA Setup" --text="No IrDA device Found."
fi
