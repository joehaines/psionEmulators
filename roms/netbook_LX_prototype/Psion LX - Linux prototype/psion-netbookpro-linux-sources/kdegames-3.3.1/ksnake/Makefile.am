
INCLUDES = -I$(top_srcdir)/libkdegames -I$(top_srcdir)/libkdegames/highscore/ $(all_includes)

noinst_HEADERS = game.h rattler.h board.h bitmaps.h \
		level.h ball.h snake.h basket.h startroom.h \
		pixServer.h progress.h levels.h view.h version.h

ksnake_SOURCES = game.cpp rattler.cpp board.cpp level.cpp ball.cpp \
		snake.cpp basket.cpp startroom.cpp \
		pixServer.cpp progress.cpp levels.cpp\
		view.cpp main.cpp settings.kcfgc appearance.ui  general.ui

ksnake_LDFLAGS = $(all_libraries) $(KDE_RPATH)
ksnake_LDADD = $(LIB_KDEUI) $(LIB_KDEGAMES) -lm
ksnake_DEPENDENCIES = $(LIB_KDEGAMES_DEP)

METASOURCES = AUTO

bin_PROGRAMS = 	ksnake

######### build rules ################

xdg_apps_DATA = ksnake.desktop
kde_kcfg_DATA = ksnake.kcfg

KDE_ICON = ksnake

SUBDIRS = data

rcdir = $(kde_datadir)/ksnake
rc_DATA = ksnakeui.rc

messages: rc.cpp
	$(XGETTEXT) *.cpp -o $(podir)/ksnake.pot
