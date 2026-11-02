SUBDIRS= data wav

INCLUDES = -I$(top_srcdir)/libkdegames -I$(top_srcdir)/libkdegames/highscore $(all_includes)
METASOURCES = AUTO

bin_PROGRAMS = ksmiletris
ksmiletris_SOURCES = main.cpp gamewindow.cpp gamewidget.cpp \
		     screenwidget.cpp mirrorwidget.cpp npiecewidget.cpp
ksmiletris_LDFLAGS = $(all_libraries) $(KDE_RPATH)
ksmiletris_LDADD = $(LIB_KDEGAMES)
ksmiletris_DEPENDENCIES = $(LIB_KDEGAMES_DEP)

xdg_apps_DATA = ksmiletris.desktop

KDE_ICON = ksmiletris

rcdir = $(kde_datadir)/ksmiletris
rc_DATA = ksmiletrisui.rc

messages: rc.cpp
	$(XGETTEXT) *.cpp -o $(podir)/ksmiletris.pot
