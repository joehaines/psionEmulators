#
# lxuser.sh	Setup system user stuff.
#

USER=lx

#
# Create system user dir if not present on mounted media
#

HOMEPATH=/home
RAMFSPATH=/var/users
MMCPATH=/media/mmc                                                              
CFPATH=/media/cf

if test ! -d "$HOMEPATH/$USER" 
then
	mkdir "$HOMEPATH/$USER"
	chown $USER.$USER "$HOMEPATH/$USER"
	chmod 0775 "$HOMEPATH/$USER"
fi

# And set up the User Ramdisk space

if test ! -d "$RAMFSPATH/$USER"
then
	mkdir "$RAMFSPATH"
	mkdir "$RAMFSPATH/$USER"
	chown $USER.$USER "$RAMFSPATH/$USER"
	chmod 0775 "$RAMFSPATH/$USER"
fi

# Then make a link to it

if test ! -L "$HOMEPATH/$USER/My RAMDisk"
then
	ln -s "$RAMFSPATH/$USER" "$HOMEPATH/$USER/My RAMDisk"
fi

# Set up placeholders for CF and MMC
# May need work to deal with already-present cards

if test ! -L "$HOMEPATH/$USER/.My CF Card"  -a  ! -L "$HOMEPATH/$USER/My CF Card" 
then
	ln -s $CFPATH "$HOMEPATH/$USER/.My CF Card"
	chmod 0000 "$HOMEPATH/$USER/.My CF Card"
fi

if test ! -L "$HOMEPATH/$USER/.My MMC Card"  -a  ! -L "$HOMEPATH/$USER/My MMC Card" 
then
	ln -s $MMCPATH "$HOMEPATH/$USER/.My MMC Card"
	chmod 0000 "$HOMEPATH/$USER/.My MMC Card"
fi


# Now default My Documents to the RAM disk

if test ! -L "/home/$USER/My Documents"
then
	ln -s "$HOMEPATH/$USER/My RAMDisk" "$HOMEPATH/$USER/My Documents"
fi



