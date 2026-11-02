 #!/bin/sh -x
#script to set location of personal data on Psion LX


CURRENT=`readlink "$HOME/My Documents" | sed -e "s!$HOME/!!"`

zenity --info --title="Choose file storage location" --text="You have a choice where the LX stores your files. They can be kept in RAM or on an MMC or CF Card. If you have a Card then we strongly recommend that you store your documents on those as you are much less likely to lose any important data. Anything stored in RAM will be lost when the machine reboots or if it crashes. You always store your files in 'My Documents'. This is where you choose where that refers to. Currently My Documents refers to $CURRENT"


CHOICE=`zenity --list --radiolist --title="Please select where 'My Documents' are stored" --column="Choose" --column="Media" FALSE "My RAMDisk" FALSE "My CF Card" FALSE "My MMC Card" FALSE "My PC Card"`

#COPY=zenity --

case $CHOICE in
	"My RAMDisk" )
		rm "$HOME/My Documents"
		ln -sf "${HOME}/My RAMDisk" "${HOME}/My Documents"
		;;
	"My CF Card" )
		if [ -L "$HOME/My CF Card" ]; then
			rm "$HOME/My Documents"
			ln -sf "$HOME/My CF Card" "$HOME/My Documents"
		else
			zenity --error --title "Card not present" --text "You cannot set 'My Documents to be on a CF Card unless one is present. I didn't find one."
		fi
		;;
	"My MMC Card" )
		if [ -L "$HOME/My MMC Card" ]; then
			rm "$HOME/My Documents"
			ln -sf "$HOME/My MMC Card" "$HOME/My Documents"
		else
			zenity --error --title "Card not present" --text "You cannot set 'My Documents to be on an MMC Card unless one is present. I didn't find one"
		fi
		;;
	"My PC Card" )
		if [ -L "$HOME/My PC Card" ]; then
			rm "$HOME/My Documents"
			ln -sf "$HOME/My PC Card" "$HOME/My Documents"
		else
			zenity --error --title "Card not present" --text "You cannot set 'My Documents to be on a PC Card unless one is present. I didn't find one."
		fi
		;;
	* )
		exit 0;;
esac

#if we get here then the files need copying to the new spot
if [ "$CURRENT" != "$CHOICE" ]; then
	mv "${HOME}/${CURRENT}/"* "${HOME}/${CHOICE}/"
fi

zenity --info --title="'My Documents' relocation completed" --text="The new location for 'My Documents' is $CHOICE. Your existing files have been moved from $CURRENT to $CHOICE."
