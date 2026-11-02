#!/bin/sh

if gpe-question --question "Are you sure you wish to restart the session?  Any unsaved documents will be lost." --buttons !gtk-yes !gtk-no; then
  matchbox-remote -exit
fi

