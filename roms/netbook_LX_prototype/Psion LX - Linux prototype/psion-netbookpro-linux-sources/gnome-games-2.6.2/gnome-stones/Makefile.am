SUBDIRS  = objects graphics sounds help

schemadir   = @GCONF_SCHEMA_FILE_DIR@
schema_in_files = gnome-stones.schemas.in
schema_DATA = $(schema_in_files:.schemas.in=.schemas)
@INTLTOOL_SCHEMAS_RULE@

Gamesdir = $(datadir)/applications

Pixmapdir= $(datadir)/pixmaps
Cavesdir = $(datadir)/gnome-stones

INCLUDES =						\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DDATADIR=\""$(datadir)"\"		   	\
	-DLIBDIR=\""$(libdir)"\"			\
	-DGNOMEPIXMAPDIR=\""$(datadir)/pixmaps"\"	\
	-DCAVESDIR=\""$(Cavesdir)"\"			\
	-I$(top_srcdir)/libgames-support		\
	$(GNOME_GAMES_CFLAGS)				\
	$(ESD_CFLAGS)


bin_PROGRAMS = gnome-stones


gnome_stones_SOURCES =				\
        main.c					\
	main.h					\
	types.h					\
	game.c					\
	game.h					\
	cave.c					\
	cave.h					\
	object.c				\
	object.h				\
	player.c				\
	player.h				\
	status.c				\
	status.h                                \
	io.c                                    \
	io.h                                    \
	preferences.c                           \
	preferences.h                           \
	view.c                                  \
	view.h					\
	sound.c					\
	sound.h				       

gnome_stones_LDADD =				\
	$(GNOME_GAMES_LIBS)			\
	../libgames-support/libgames-support.la \
	$(ESD_LIBS)				\
	$(INTLLIBS)

gnome_stones_LDFLAGS=-export-dynamic

Pixmap_DATA = gnome-stones.png gnome-stones-file.png
Games_in_files = gnome-stones.desktop.in
Games_DATA = $(Games_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

Caves_in_files = default.caves.in tutorial.caves.in gnomekoban.caves.in atari.caves.in
Caves_DATA = $(Caves_in_files:.caves.in=.caves)
@INTLTOOL_CAVES_RULE@

gtkrcdir = $(datadir)
gtkrc_DATA = gnome-stonesrc gnome-stonesrc.ko

Mimedir    = $(datadir)/mime-info
Mime_DATA  = gnome-stones.keys gnome-stones.mime

EXTRA_DIST    =					\
	$(Pixmap_DATA)				\
	$(Games_in_files)			\
	$(Caves_in_files)			\
	$(gtkrc_DATA)				\
	$(schema_in_files)			\
	TODO					\
	README					\
	gnome-stones.keys.in			\
	gnome-stones.mime			\
	gnome-stones-file.png

install-data-local:
if GCONF_SCHEMAS_INSTALL
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/gnome-stones/$(schema_DATA)
endif
	-$(mkinstalldirs) $(DESTDIR)$(scoredir)
	-touch $(DESTDIR)$(scoredir)/gnome-stones.scores
	-chown $(scores_user):$(scores_group) $(DESTDIR)$(scoredir)/gnome-stones.scores
	-chmod 664 $(DESTDIR)$(scoredir)/gnome-stones.scores
	-if test "x$(setgid)" = "xtrue"; then chgrp $(scores_group) $(DESTDIR)$(bindir)/gnome-stones && chmod 2555 $(DESTDIR)$(bindir)/gnome-stones ; fi

gnome-stones.keys: gnome-stones.keys.in Makefile
	sed -e 's^\@icondir\@^$(Pixmapdir)^g' < $(srcdir)/gnome-stones.keys.in > gnome-stones.keys

CLEANFILES = $(Games_DATA) $(Caves_DATA) $(schema_DATA) gnome-stones.keys
