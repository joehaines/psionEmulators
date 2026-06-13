HERMES
======

An (almost) fully featured emulation of a DEC
vt100 terminal, with vt102 extensions.

Please note that no file transfer facilities
or scripting are supported in this release.

Hermes Installation
===================

Simply copy all files EXCEPT README.TXT to a
folder called \System\Apps\Hermes

You may need to alter the System preferences to
see the \System folder. If the folder does not
already exist, then create it.

If you have an earlier version of Hermes,
then you should delete the contents of
\System\Apps\Hermes before copying the new
version in.

IMPORTANT - if you are upgrading from a previous
version of Hermes, you will find that two errors
are reported whenaver any old version of Hermes
configuration is loaded. The settings will be
reset to defaults, losing all function key
definitions. I'm afraid that there is no way to
upgrade the config files.

NOTE to EPOC ROM 1.00 users:
	Software handshaking is unreliable with this
	ROM version, and may cause hangs. Try killing
	Hermes with Ctrl-Shift-Fn-K.

Bug Reports to:
	rpanton@iota.demon.co.uk

