#!/bin/sh
zenity --title="Connect to Internet" --info --text="Please insert device to use for internet connection (modem or wifi card)."

RET=`zenity --list --radiolist --title="Select Device" --column="Choose" --column="Device" FALSE "Modem" FALSE "Wireless LAN" FALSE "Ethernet"`

case $RET in
	"Modem" )
		TYPE=ppp;
		gpe-su -c 'gkdial -a  > /dev/null 2>&1';;
	"Wireless LAN" )
		TYPE=wlan;
		gpe-conf network;;
	"Ethernet" )
		TYPE=ethernet;
		gpe-conf network;;
	* )
		zenity --title="Connect to Internet" --info --text="No device selected, exiting..."
		exit 0;;
esac

if [ $TYPE = ppp ]; then
	zenity --question --title "Test Connection" --text "Do you want to run the dialler to test your connection?"
	if [ $? = 0 ]; then
		gkdial;
	fi
fi

exit 0
                                                                                                                                                                         esa                                                                                                                                                                                        
