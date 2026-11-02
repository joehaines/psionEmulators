#!/bin/sh

mount -t proc proc /proc 

if [ -e /sys ] && grep -q sysfs /proc/filesystems; then
  mount sysfs /sys -t sysfs
fi

exit 0
