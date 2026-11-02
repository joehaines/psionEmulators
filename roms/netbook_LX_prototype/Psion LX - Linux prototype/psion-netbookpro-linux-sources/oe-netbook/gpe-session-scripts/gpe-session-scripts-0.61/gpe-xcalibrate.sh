#! /bin/sh
if test -x /usr/bin/xtscal; then
    xtscal -debug -e2cal >/etc/xcalibrate.conf
fi
