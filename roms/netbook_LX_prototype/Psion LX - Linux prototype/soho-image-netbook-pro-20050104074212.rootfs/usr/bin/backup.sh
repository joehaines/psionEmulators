 #!/bin/sh -x
#script to do simple backup on Psion LX

#Notes: hda could be CF slot or PCMCIA slot. 
#If one is filled and second added then that's hdc
#1st USB device is sda, next sdb and so on.
#This script cannot know if hda is in CF slot or PCMCIA slot.
#Best thing for itto do is probably use whatever card is present if there is only one:
#If there is more than one, ask user which to use, glossing over CF/PCMCIA prob
#OO upgrade may mean CF card is often present butnot to be used for backup. 
#Could give them all a recognisable ID so they can be auto-ignored.


#zenity --title "Checking your files" --progress 
zenity --title="Backing up your files and data" --info --text="Please insert your backup card, then indicate which device you want to back-up to."

CHOICE=`zenity --list --radiolist --title="Please select your backup device" --column="Choose" --column="Media" FALSE "CF Card" FALSE "MMC Card" FALSE "USB Dongle" FALSE "PC Card"`

case $CHOICE in
	"CF Card" )
#		DEVICE=/dev/hda1;
		MNTPT=/media/cf;;
	"MMC Card" )
#		DEVICE=/dev/mmcblk0p1;
		MNTPT=/media/mmc;;
	"USB Dongle" )
#		DEVICE=/dev/sda1;
		MNTPT=/media/usb;;
	"PC Card" )
#		DEVICE=/dev/hdc1;
		MNTPT=/media/pc;;
	* )
		exit 0;;
esac
		
if [ "`mount | grep ${MNTPT}`" = "" ]; then 
#	DIYMOUNT=yes
#	if ! mount ${DEVICE} ; then 
		zenity --error --text="I cannot find your ${CHOICE}, please try removing and re-inserting it, then re-run this script."
		exit 1
#	fi
fi

BASESIZE=`du -s -h "${HOME}/" | cut -f1`
DOCSSIZE=`du -s -h "${HOME}/My Documents/" | cut -f1`
if ! DIRS=`zenity --list --checklist --title="Please select what to back up" --column="Choose" --column="Data" --column="Size" TRUE "Config and Contacts" $BASESIZE TRUE "My Documents" $DOCSSIZE`; then
	#cancelled
        exit 0
fi

DIRNAMEBASE=`date +%Y.%m.%d`
COUNT=1; while [ -e "${MNTPT}/${DIRNAMEBASE}-${COUNT}" ]; do 
	echo "$COUNT"; COUNT=$((COUNT+1)); 
done
mkdir "${MNTPT}/${DIRNAMEBASE}-${COUNT}"

#should check available space here
if echo $DIRS | grep "Config and Contacts" ; then
	cp -a  "${HOME}/*" "${MNTPT}/${DIRNAMEBASE}-${COUNT}/"
fi	   
if echo $DIRS | grep "My Documents" ; then
	mkdir "${MNTPT}/${DIRNAMEBASE}-${COUNT}/My Documents"
	cp -a  "${HOME}/My Documents/*" "${MNTPT}/${DIRNAMEBASE}-${COUNT}/My Documents/"
fi

#	if [ "x$DIYMOUNT" = "xyes" ]; then
#		umount ${DEVICE}
#	fi
zenity --info --title="Backup Complete" --text="Your files have been saved in a folder called ${DIRNAMEBASE}-${COUNT}"

