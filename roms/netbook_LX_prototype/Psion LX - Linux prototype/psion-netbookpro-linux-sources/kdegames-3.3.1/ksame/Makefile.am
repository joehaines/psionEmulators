
bin_PROGRAMS = ksame
INCLUDES = -I$(top_srcdir)/libkdegames -I$(top_srcdir)/libkdegames/highscore $(all_includes)

ksame_SOURCES= main.cpp StoneField.cpp StoneWidget.cpp KSameWidget.cpp
ksame_LDFLAGS = $(all_libraries) $(KDE_RPATH)
ksame_LDADD = $(LIB_KDEGAMES) $(LIB_KIO)
ksame_DEPENDENCIES = $(LIB_KDEGAMES_DEP)

METASOURCES = AUTO

noinst_HEADERS = StoneField.h StoneWidget.h KSameWidget.h version.h

xdg_apps_DATA = ksame.desktop

appdir = $(kde_datadir)/ksame
app_DATA = stones.png ksameui.rc eventsrc

KDE_ICON = ksame

EXTRA_DIST = $(app_DATA)

messages: rc.cpp
	$(XGETTEXT) *.cpp -o $(podir)/ksame.pot
