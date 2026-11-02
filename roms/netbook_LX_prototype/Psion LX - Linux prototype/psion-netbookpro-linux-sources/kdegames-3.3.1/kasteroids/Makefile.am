SUBDIRS = . sprites sounds

INCLUDES = -I$(top_srcdir)/libkdegames -I$(top_srcdir)/libkdegames/highscore $(all_includes)
METASOURCES = AUTO

bin_PROGRAMS = kasteroids
kasteroids_SOURCES = main.cpp view.cpp ledmeter.cpp toplevel.cpp settings.kcfgc
kasteroids_LDFLAGS = $(all_libraries) $(KDE_RPATH)
kasteroids_LDADD = $(LIB_KDEGAMES) -lsoundserver_idl -lqtmcop
kasteroids_DEPENDENCIES = $(LIB_KDEGAMES_DEP)

picsdir = $(kde_datadir)/kasteroids/sprites
pics_DATA = bg.png

xdg_apps_DATA = kasteroids.desktop
kde_kcfg_DATA = kasteroids.kcfg

EXTRA_DIST = $(pics_DATA)
KDE_ICON = kasteroids

rcdir = $(kde_datadir)/kasteroids
rc_DATA = kasteroidsui.rc

messages: rc.cpp
	$(XGETTEXT) *.cpp -o $(podir)/kasteroids.pot

