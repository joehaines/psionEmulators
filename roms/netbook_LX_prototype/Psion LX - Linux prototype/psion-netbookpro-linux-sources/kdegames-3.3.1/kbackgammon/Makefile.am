SUBDIRS = pics sounds icons engines

INCLUDES = -I$(top_srcdir)/libkdegames -I$(top_srcdir)/libkdegames/kgame/ -I$(srcdir)/engines $(all_includes)
METASOURCES = AUTO

bin_PROGRAMS = kbackgammon
kbackgammon_SOURCES = main.cpp kbg.cpp kbgboard.cpp kbgtextview.cpp \
			kbgstatus.cpp
kbackgammon_LDFLAGS = $(all_libraries) $(KDE_RPATH)
kbackgammon_LDADD = $(LIB_KDEGAMES) $(LIB_KDEPRINT) ./engines/libkbgengines.la
kbackgammon_DEPENDENCIES = $(LIB_KDEGAMES_DEP)

xdg_apps_DATA = kbackgammon.desktop

KDE_ICON = kbackgammon

rcdir = $(kde_datadir)/kbackgammon
rc_DATA = kbackgammonui.rc eventsrc

messages: rc.cpp	
	LIST=`find . -name \*.h -o -name \*.hh -o -name \*.H -o -name \*.hxx -o -name \*.hpp -o -name \*.cpp -o -name \*.cc -o -name \*.c -o -name \*.ecpp -o -name \*.C`; \
	$(XGETTEXT) $$LIST -o $(podir)/kbackgammon.pot

