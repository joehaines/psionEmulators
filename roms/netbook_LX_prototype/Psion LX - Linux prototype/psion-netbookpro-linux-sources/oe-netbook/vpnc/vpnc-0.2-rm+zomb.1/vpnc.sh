#!/bin/sh -x
#script to do vpnc setup on Psion LX
# and filling in /etc/vpnc.conf as follows:
#
# IPSec gateway	<gateway IP>
# IPSec ID			<group name>
# IPSec secret	<group pre-shared key>
# Xauth username <username>

FILE=/etc/vpnc.conf

zenity  --info --title="VPNC Setup" --text="Setting up your VPN credentials"

GATEWAY=`zenity --entry --title="VPNC Setup" --text="Enter the Gateway Address" `
GROUP=`zenity --entry --title="VPNC Setup" --text="Enter the Group ID" `
SECRET=`zenity --entry --title="VPNC Setup" --text="Enter the Group Password" --hide-text`
USER=`zenity --entry --title="VPNC Setup" --text="Enter the Username" `

if test $GATEWAY"x" != "x" -a $GROUP"x" != "x" -a $SECRET"x" != "x" -a \
$USER"x" != "x"
then
	echo "IPSec gateway" $GATEWAY	>$FILE
	echo "IPSec ID" $GROUP		>> $FILE
	echo "IPSec secret" $SECRET	>> $FILE
	echo "Xauth username" $USER	>> $FILE
else
	zenity --info --title="VPNC Setup" --text="Data incomplete. Please try again."
fi
