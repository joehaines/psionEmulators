pixmapdir=$(datadir)/pixmaps/iagno
Gamesdir = $(datadir)/applications
appicondir=$(datadir)/pixmaps

INCLUDES = 			\
	-I$(top_srcdir)/libgames-support \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-DDATADIR=\""$(datadir)"\"		   \
	$(GNOME_GAMES_CFLAGS)

bin_PROGRAMS = gataxx

gataxx_SOURCES = \
	gataxx.c \
	gataxx.h \
	ataxx.c \
	ataxx.h \
	properties.c \
	properties.h

gataxx_LDADD = $(GNOME_GAMES_LIBS) \
	../libgames-support/libgames-support.la

appicon_DATA = gataxx.png

Games_in_files = gataxx.desktop.in
Games_DATA = $(Games_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

schemadir   = @GCONF_SCHEMA_FILE_DIR@
schema_in_files = gataxx.schemas.in
schema_DATA = $(schema_in_files:.schemas.in=.schemas)
@INTLTOOL_SCHEMAS_RULE@

EXTRA_DIST = AUTHORS \
	gataxx.desktop.in 	\
	gataxx.soundlist.in \
	$(appicon_DATA) \
	$(schema_in_files)

soundlistdir = $(sysconfdir)/sound/events
soundlist_in_files = gataxx.soundlist.in
soundlist_DATA = $(soundlist_in_files:.soundlist.in=.soundlist)
@INTLTOOL_SOUNDLIST_RULE@

SUBDIRS = sounds help

CLEANFILES = $(Games_DATA) $(schema_DATA) $(soundlist_DATA)

install-data-local:
if GCONF_SCHEMAS_INSTALL
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/gataxx/$(schema_DATA)
endif
