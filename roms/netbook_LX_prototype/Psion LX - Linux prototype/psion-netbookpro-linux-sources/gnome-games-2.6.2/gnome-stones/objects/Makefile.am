GSTONES_SRC_DIR = $(srcdir)/../

objectdir= $(libdir)/gnome-stones/objects

INCLUDES = 						\
	-I$(GSTONES_SRC_DIR)				\
	$(GNOME_GAMES_CFLAGS)				\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-I$(includedir)

object_LTLIBRARIES = libgnome-stones.la libgnomekoban.la

libgnome_stones_la_SOURCES = gnome-stones.c
libgnome_stones_la_LDFLAGS = -module -avoid-version
libgnomekoban_la_SOURCES   = gnomekoban.c
libgnomekoban_la_LDFLAGS = -module -avoid-version
