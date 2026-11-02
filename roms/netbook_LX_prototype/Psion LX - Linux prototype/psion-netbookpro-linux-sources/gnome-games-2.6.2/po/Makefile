# Makefile for program source directory in GNU NLS utilities package.
# Copyright (C) 1995, 1996, 1997 by Ulrich Drepper <drepper@gnu.ai.mit.edu>
#
# This file file be copied and used freely without restrictions.  It can
# be used in projects which are not available under the GNU Public License
# but which still want to provide support for the GNU gettext functionality.
# Please note that the actual code is *not* freely available.
#
# - Modified by Owen Taylor <otaylor@redhat.com> to use GETTEXT_PACKAGE
#   instead of PACKAGE and to look for po2tbl in ./ not in intl/
#
# - Modified by jacob berkman <jacob@ximian.com> to install
#   Makefile.in.in and po2tbl.sed.in for use with glib-gettextize

GETTEXT_PACKAGE = gnome-games
PACKAGE = gnome-games
VERSION = 2.6.2

SHELL = /bin/sh


srcdir = /var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/po
top_srcdir = /var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2
top_builddir = ..
VPATH = /var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/po

prefix = /usr
exec_prefix = /usr
datadir = /usr/share
libdir = /usr/lib
localedir = $(libdir)/locale
gnulocaledir = $(datadir)/locale
gettextsrcdir = $(datadir)/glib-2.0/gettext/po
subdir = po

INSTALL = /var/home/petern/oe/oe-netbook/tmp/staging/i686-linux/bin/install -c
INSTALL_DATA = ${INSTALL} -m 644
MKINSTALLDIRS = $(top_srcdir)//var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/mkinstalldirs

CC = ccache arm-linux-gcc -march=armv5te -mtune=xscale
GENCAT = @GENCAT@
GMSGFMT = /var/home/petern/oe/oe-netbook/tmp/staging/i686-linux/bin/msgfmt
MSGFMT = /var/home/petern/oe/oe-netbook/tmp/staging/i686-linux/bin/msgfmt
XGETTEXT = /var/home/petern/oe/oe-netbook/tmp/staging/i686-linux/bin/xgettext
INTLTOOL_UPDATE = $(top_builddir)/intltool-update
INTLTOOL_EXTRACT = $(top_builddir)/intltool-extract
MSGMERGE = INTLTOOL_EXTRACT=$(INTLTOOL_EXTRACT) $(INTLTOOL_UPDATE) --gettext-package $(GETTEXT_PACKAGE) --dist
GENPOT   = INTLTOOL_EXTRACT=$(INTLTOOL_EXTRACT) $(INTLTOOL_UPDATE) --gettext-package $(GETTEXT_PACKAGE) --pot

DEFS = -DHAVE_CONFIG_H
CFLAGS = -I/var/home/petern/oe/oe-netbook/tmp/staging/arm-linux/include -fexpensive-optimizations -fomit-frame-pointer -frename-registers -O2
CPPFLAGS = -I/var/home/petern/oe/oe-netbook/tmp/staging/arm-linux/include

INCLUDES = -I.. -I$(top_srcdir)/intl

COMPILE = $(CC) -c $(DEFS) $(INCLUDES) $(CPPFLAGS) $(CFLAGS) $(XCFLAGS)

SOURCES = 
POFILES =  am.po ar.po az.po be.po bg.po bn.po ca.po cs.po cy.po da.po de.po el.po en_CA.po en_GB.po es.po et.po eu.po fa.po fi.po fr.po ga.po gl.po gu.po he.po hi.po hr.po hu.po id.po it.po ja.po ko.po lt.po lv.po mk.po ml.po mn.po ms.po nl.po nn.po no.po pa.po pl.po pt.po pt_BR.po ro.po ru.po sk.po sl.po sq.po sr.po sr@Latn.po sv.po tr.po uk.po vi.po wa.po zh_CN.po zh_TW.po
GMOFILES =  am.gmo ar.gmo az.gmo be.gmo bg.gmo bn.gmo ca.gmo cs.gmo cy.gmo da.gmo de.gmo el.gmo en_CA.gmo en_GB.gmo es.gmo et.gmo eu.gmo fa.gmo fi.gmo fr.gmo ga.gmo gl.gmo gu.gmo he.gmo hi.gmo hr.gmo hu.gmo id.gmo it.gmo ja.gmo ko.gmo lt.gmo lv.gmo mk.gmo ml.gmo mn.gmo ms.gmo nl.gmo nn.gmo no.gmo pa.gmo pl.gmo pt.gmo pt_BR.gmo ro.gmo ru.gmo sk.gmo sl.gmo sq.gmo sr.gmo sr@Latn.gmo sv.gmo tr.gmo uk.gmo vi.gmo wa.gmo zh_CN.gmo zh_TW.gmo
DISTFILES = ChangeLog Makefile.in.in POTFILES.in $(GETTEXT_PACKAGE).pot \
$(POFILES) $(GMOFILES) $(SOURCES)

POTFILES = \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/agnes.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/aisleriot.schemas.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/auld_lang_syne.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/bakers_dozen.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/bakers_game.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/beleaguered_castle.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/block_ten.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/bristol.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/camelot.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/canfield.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/card.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/chessboard.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/clock.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/cover.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/dialog.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/diamond_mine.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/doublets.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/eagle_wing.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/easthaven.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/eight_off.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/elevator.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/escalator.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/first_law.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/fortress.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/fortunes.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/fourteen.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/freecell.desktop.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/freecell.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/gay_gordons.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/glenwood.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/golf.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/gypsy.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/helsinki.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/hopscotch.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/isabel.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/jamestown.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/jumbo.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/kansas.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/king_albert.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/klondike.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/labyrinth.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/lady_jane.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/maze.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/menu.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/monte_carlo.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/neighbor.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/odessa.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/osmosis.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/peek.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/pileon.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/plait.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/poker.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/quatorze.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/royal_east.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/scorpion.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/scuffle.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/seahaven.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/sir_tommy.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/sol.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/sol.desktop.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/sol.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/spider.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/spiderette.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/statistics.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/statistics.glade \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/straight_up.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/streets_and_alleys.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/template.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/ten_across.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/thieves.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/thirteen.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/thumb_and_pouch.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/translatable_game_names.h \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/treize.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/union_square.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/valentine.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/westhaven.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/whitehead.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/will_o_the_wisp.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/yield.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/yukon.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/aisleriot/zebra.scm \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/blackjack/data/blackjack.desktop.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/blackjack/data/blackjack.schemas.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/blackjack/src/blackjack.cpp \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/blackjack/src/card.cpp \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/blackjack/src/dialog.cpp \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/blackjack/src/events.cpp \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/blackjack/src/game.cpp \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/blackjack/src/hand.cpp \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/blackjack/src/menu.cpp \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/blackjack/src/player.cpp \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/blackjack/src/splash.cpp \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gataxx/ataxx.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gataxx/gataxx.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gataxx/gataxx.desktop.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gataxx/gataxx.schemas.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gataxx/gataxx.soundlist.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gataxx/properties.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gdk-card-image/card-styles.xml.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gdk-card-image/gdk-card-image.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/glines/glines.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/glines/glines.desktop.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/glines/glines.schemas.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnect/data/gnect.desktop.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnect/data/gnect.schemas.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnect/data/gnect.soundlist.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnect/src/gfx.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnect/src/main.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnect/src/prefs.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnect/src/theme.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnibbles/gnibbles.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnibbles/gnibbles.desktop.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnibbles/gnibbles.schemas.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnibbles/gnibbles.soundlist.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnibbles/main.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnibbles/preferences.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnibbles/scoreboard.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnobots2/game.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnobots2/gnobots.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnobots2/gnobots2.desktop.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnobots2/gnobots2.schemas.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnobots2/gnobots2.soundlist.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnobots2/graphics.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnobots2/menu.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnobots2/properties.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnobots2/statusbar.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnome-stones/atari.caves.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnome-stones/default.caves.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnome-stones/gnome-stones.desktop.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnome-stones/gnome-stones.schemas.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnome-stones/gnomekoban.caves.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnome-stones/io.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnome-stones/main.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnome-stones/player.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnome-stones/preferences.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnome-stones/status.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnome-stones/tutorial.caves.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnometris/gnometris.desktop.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnometris/gnometris.schemas.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnometris/main.cpp \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnometris/scoreframe.cpp \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnometris/tetris.cpp \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnomine/gnomine.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnomine/gnomine.desktop.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnomine/gnomine.schemas.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnotravex/gnotravex.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnotravex/gnotravex.desktop.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnotravex/gnotravex.schemas.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnotski/gnotski.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gnotski/gnotski.desktop.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gtali/clist.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gtali/gtali.desktop.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gtali/gtali.schemas.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gtali/gyahtzee.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gtali/setup.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/gtali/yahtzee.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/iagno/gnothello.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/iagno/iagno.desktop.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/iagno/iagno.schemas.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/iagno/iagno.soundlist.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/iagno/network.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/iagno/othello.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/iagno/properties.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/libgames-support/games-gconf.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/mahjongg/mahjongg.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/mahjongg/mahjongg.desktop.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/mahjongg/mahjongg.schemas.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/same-gnome/same-gnome.c \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/same-gnome/same-gnome.desktop.in \
	/var/home/petern/oe/oe-netbook/tmp/work/gnome-games-2.6.2-r1/gnome-games-2.6.2/same-gnome/same-gnome.schemas.in

CATALOGS =  am.gmo ar.gmo az.gmo be.gmo bg.gmo bn.gmo ca.gmo cs.gmo cy.gmo da.gmo de.gmo el.gmo en_CA.gmo en_GB.gmo es.gmo et.gmo eu.gmo fa.gmo fi.gmo fr.gmo ga.gmo gl.gmo gu.gmo he.gmo hi.gmo hr.gmo hu.gmo id.gmo it.gmo ja.gmo ko.gmo lt.gmo lv.gmo mk.gmo ml.gmo mn.gmo ms.gmo nl.gmo nn.gmo no.gmo pa.gmo pl.gmo pt.gmo pt_BR.gmo ro.gmo ru.gmo sk.gmo sl.gmo sq.gmo sr.gmo sr@Latn.gmo sv.gmo tr.gmo uk.gmo vi.gmo wa.gmo zh_CN.gmo zh_TW.gmo
CATOBJEXT = .gmo
INSTOBJEXT = .mo

.SUFFIXES:
.SUFFIXES: .c .o .po .pox .gmo .mo .msg .cat

.c.o:
	$(COMPILE) $<

.po.pox:
	$(MAKE) $(GETTEXT_PACKAGE).pot
	$(MSGMERGE) $< $(srcdir)/$(GETTEXT_PACKAGE).pot -o $*pox

.po.mo:
	$(MSGFMT) -o $@ $<

.po.gmo:
	file=$(srcdir)/`echo $* | sed 's,.*/,,'`.gmo \
	  && rm -f $$file && $(GMSGFMT) -o $$file $<

.po.cat:
	sed -f ../intl/po2msg.sed < $< > $*.msg \
	  && rm -f $@ && $(GENCAT) $@ $*.msg


all: all-yes

all-yes: $(CATALOGS)
all-no:

$(srcdir)/$(GETTEXT_PACKAGE).pot: $(POTFILES)
	$(GENPOT)

install: install-exec install-data
install-exec:
install-data: install-data-yes
install-data-no: all
install-data-yes: all
	if test -r "$(MKINSTALLDIRS)"; then \
	  $(MKINSTALLDIRS) $(DESTDIR)$(datadir); \
	else \
	  $(SHELL) $(top_srcdir)/mkinstalldirs $(DESTDIR)$(datadir); \
	fi
	@catalogs='$(CATALOGS)'; \
	for cat in $$catalogs; do \
	  cat=`basename $$cat`; \
	  case "$$cat" in \
	    *.gmo) destdir=$(gnulocaledir);; \
	    *)     destdir=$(localedir);; \
	  esac; \
	  lang=`echo $$cat | sed 's/\$(CATOBJEXT)$$//'`; \
	  dir=$(DESTDIR)$$destdir/$$lang/LC_MESSAGES; \
	  if test -r "$(MKINSTALLDIRS)"; then \
	    $(MKINSTALLDIRS) $$dir; \
	  else \
	    $(SHELL) $(top_srcdir)/mkinstalldirs $$dir; \
	  fi; \
	  if test -r $$cat; then \
	    $(INSTALL_DATA) $$cat $$dir/$(GETTEXT_PACKAGE)$(INSTOBJEXT); \
	    echo "installing $$cat as $$dir/$(GETTEXT_PACKAGE)$(INSTOBJEXT)"; \
	  else \
	    $(INSTALL_DATA) $(srcdir)/$$cat $$dir/$(GETTEXT_PACKAGE)$(INSTOBJEXT); \
	    echo "installing $(srcdir)/$$cat as" \
		 "$$dir/$(GETTEXT_PACKAGE)$(INSTOBJEXT)"; \
	  fi; \
	  if test -r $$cat.m; then \
	    $(INSTALL_DATA) $$cat.m $$dir/$(GETTEXT_PACKAGE)$(INSTOBJEXT).m; \
	    echo "installing $$cat.m as $$dir/$(GETTEXT_PACKAGE)$(INSTOBJEXT).m"; \
	  else \
	    if test -r $(srcdir)/$$cat.m ; then \
	      $(INSTALL_DATA) $(srcdir)/$$cat.m \
		$$dir/$(GETTEXT_PACKAGE)$(INSTOBJEXT).m; \
	      echo "installing $(srcdir)/$$cat as" \
		   "$$dir/$(GETTEXT_PACKAGE)$(INSTOBJEXT).m"; \
	    else \
	      true; \
	    fi; \
	  fi; \
	done
	if test "$(PACKAGE)" = "glib"; then \
	  if test -r "$(MKINSTALLDIRS)"; then \
	    $(MKINSTALLDIRS) $(DESTDIR)$(gettextsrcdir); \
	  else \
	    $(SHELL) $(top_srcdir)/mkinstalldirs $(DESTDIR)$(gettextsrcdir); \
	  fi; \
	  $(INSTALL_DATA) $(srcdir)/Makefile.in.in \
			  $(DESTDIR)$(gettextsrcdir)/Makefile.in.in; \
	else \
	  : ; \
	fi

# Define this as empty until I found a useful application.
installcheck:

uninstall:
	catalogs='$(CATALOGS)'; \
	for cat in $$catalogs; do \
	  cat=`basename $$cat`; \
	  lang=`echo $$cat | sed 's/\$(CATOBJEXT)$$//'`; \
	  rm -f $(DESTDIR)$(localedir)/$$lang/LC_MESSAGES/$(GETTEXT_PACKAGE)$(INSTOBJEXT); \
	  rm -f $(DESTDIR)$(localedir)/$$lang/LC_MESSAGES/$(GETTEXT_PACKAGE)$(INSTOBJEXT).m; \
	  rm -f $(DESTDIR)$(gnulocaledir)/$$lang/LC_MESSAGES/$(GETTEXT_PACKAGE)$(INSTOBJEXT); \
	  rm -f $(DESTDIR)$(gnulocaledir)/$$lang/LC_MESSAGES/$(GETTEXT_PACKAGE)$(INSTOBJEXT).m; \
	done
	if test "$(PACKAGE)" = "glib"; then \
	  rm -f $(DESTDIR)$(gettextsrcdir)/Makefile.in.in; \
	fi

check: all

dvi info tags TAGS ID:

mostlyclean:
	rm -f core core.* *.pox $(GETTEXT_PACKAGE).po *.old.po cat-id-tbl.tmp
	rm -fr *.o
	rm -f .intltool-merge-cache

clean: mostlyclean

distclean: clean
	rm -f Makefile Makefile.in POTFILES *.mo *.msg *.cat *.cat.m

maintainer-clean: distclean
	@echo "This command is intended for maintainers to use;"
	@echo "it deletes files that may require special tools to rebuild."
	rm -f $(GMOFILES)

distdir = ../$(GETTEXT_PACKAGE)-$(VERSION)/$(subdir)
dist distdir: update-po $(DISTFILES)
	dists="$(DISTFILES)"; \
	for file in $$dists; do \
	  ln $(srcdir)/$$file $(distdir) 2> /dev/null \
	    || cp -p $(srcdir)/$$file $(distdir); \
	done

update-po: Makefile
	$(MAKE) $(GETTEXT_PACKAGE).pot
	tmpdir=`pwd`; \
	cd $(srcdir); \
	catalogs='$(CATALOGS)'; \
	for cat in $$catalogs; do \
	  cat=`basename $$cat`; \
	  lang=`echo $$cat | sed 's/\$(CATOBJEXT)$$//'`; \
	  echo "$$lang:"; \
	  if $$tmpdir/$(INTLTOOL_UPDATE) --gettext-package $(GETTEXT_PACKAGE) --dist -o $$tmpdir/$$lang.new.po $$lang; then \
	    if cmp $$lang.po $$tmpdir/$$lang.new.po >/dev/null 2>&1; then \
	      rm -f $$tmpdir/$$lang.new.po; \
            else \
	      if mv -f $$tmpdir/$$lang.new.po $$lang.po; then \
	        :; \
	      else \
	        echo "msgmerge for $$lang.po failed: cannot move $$tmpdir/$$lang.new.po to $$lang.po" 1>&2; \
	        rm -f $$tmpdir/$$lang.new.po; \
	        exit 1; \
	      fi; \
	    fi; \
	  else \
	    echo "msgmerge for $$cat failed!"; \
	    rm -f $$tmpdir/$$lang.new.po; \
	  fi; \
	done

# POTFILES is created from POTFILES.in by stripping comments, empty lines
# and Intltool tags (enclosed in square brackets), and appending a full
# relative path to them
POTFILES: POTFILES.in
	( if test 'x$(srcdir)' != 'x.'; then \
	    posrcprefix='$(top_srcdir)/'; \
	  else \
	    posrcprefix="../"; \
	  fi; \
	  rm -f $@-t $@ \
	    && (sed -e '/^#/d' 						\
		    -e "s/^\[.*\] +//" 					\
		    -e '/^[ 	]*$$/d' 				\
		    -e "s@.*@	$$posrcprefix& \\\\@" < $(srcdir)/$@.in	\
		| sed -e '$$s/\\$$//') > $@-t \
	    && chmod a-w $@-t \
	    && mv $@-t $@ )

Makefile: Makefile.in.in ../config.status POTFILES
	cd .. \
	  && CONFIG_FILES=$(subdir)/$@.in CONFIG_HEADERS= \
	       $(SHELL) ./config.status

# Tell versions [3.59,3.63) of GNU make not to export all variables.
# Otherwise a system limit (for SysV at least) may be exceeded.
.NOEXPORT:
