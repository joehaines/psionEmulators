


pixmapdir = $(datadir)/pixmaps/mahjongg
Gamesdir = $(datadir)/applications
appicondir=$(datadir)/pixmaps

schemadir   = @GCONF_SCHEMA_FILE_DIR@
schema_in_files = mahjongg.schemas.in
schema_DATA = $(schema_in_files:.schemas.in=.schemas)
@INTLTOOL_SCHEMAS_RULE@

INCLUDES =						\
	-I$(top_srcdir)/libgames-support		\
	-I..						\
	-I$(srcdir)/..					\
	$(GNOME_GAMES_CFLAGS)				\
	-DDATADIR=\""$(datadir)"\"		   \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"

bin_PROGRAMS = mahjongg

mahjongg_SOURCES = 	\
	mahjongg.c	\
	drawing.c	\
	drawing.h	\
	solubility.c	\
	solubility.h	\
	mahjongg.h

mahjongg_LDADD =					\
	$(GNOME_GAMES_LIBS)				\
	../libgames-support/libgames-support.la		\
	$(INTLLIBS)

pixmap_DATA = default.png smooth.png postmodern.svg maya.png

appicon_DATA = gnome-mahjongg.png

EXTRA_DIST = mahjongg.desktop.in $(pixmap_DATA) $(appicon_DATA) \
	$(schema_in_files)

Games_in_files = mahjongg.desktop.in
Games_DATA = $(Games_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

SUBDIRS = help

install-data-local:
if GCONF_SCHEMAS_INSTALL
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/mahjongg/$(schema_DATA)
endif
	-$(mkinstalldirs) $(DESTDIR)$(scoredir)
	-for i in easy difficult confounding pyramid tictactoe cloud dragon bridges; do \
		touch $(DESTDIR)$(scoredir)/mahjongg.$$i.scores; \
		chown $(scores_user):$(scores_group) $(DESTDIR)$(scoredir)/mahjongg.$$i.scores; \
		chmod 664 $(DESTDIR)$(scoredir)/mahjongg.$$i.scores; \
	done
	-if test "x$(setgid)" = "xtrue"; then chgrp $(scores_group) $(DESTDIR)$(bindir)/mahjongg && chmod 2555 $(DESTDIR)$(bindir)/mahjongg ; fi
	$(mkinstalldirs) $(DESTDIR)$(Gamesdir)

CLEANFILES = $(Games_DATA) $(schema_DATA)
