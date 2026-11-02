#!/bin/sh

infoprint "Unmounting cards"

if { mount | grep /media/cf > /dev/null; } ; then umount -f /media/cf; fi
if { mount | grep /media/cf2 > /dev/null; } ; then umount -f /media/cf2; fi
if { mount | grep /media/mmc > /dev/null; } ; then umount -f /media/mmc; fi

#We cannot tell the difference between 'failed because not present' or 
# 'failed because pcmcia system is confused about what cards are present'
#this test might be useful:
# if [ "`/etc/pcmcia/ide stop hda | grep usage | cut -f1 -d:`" = "usage" ]; 
# Could add something to pcmcia scripts to make them smarter
#in practice unmounting ecplicitly first seems to help a lot

/etc/pcmcia/ide stop hda > /dev/null

/etc/pcmcia/ide stop hdc > /dev/null 

zenity --info --title "CF and MMC Card control" --text="You can now remove your CF or MMC cards safely" 2> /dev/null
