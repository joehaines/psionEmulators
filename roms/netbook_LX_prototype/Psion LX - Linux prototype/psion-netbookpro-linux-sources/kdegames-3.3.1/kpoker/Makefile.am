SUBDIRS = sounds

INCLUDES = -I$(top_srcdir)/libkdegames $(all_includes) 
METASOURCES = AUTO
KDE_ICON = kpoker

bin_PROGRAMS = kpoker
kpoker_SOURCES = kpoker.cpp kpaint.cpp top.cpp main.cpp betbox.cpp \
	optionsdlg.cpp player.cpp playerbox.cpp newgamedlg.cpp poker.cpp
kpoker_LDFLAGS = $(all_libraries) $(KDE_RPATH)
kpoker_LDADD = $(LIB_KDEGAMES)
kpoker_DEPENDENCIES = $(LIB_KDEGAMES_DEP)

xdg_apps_DATA = kpoker.desktop

rcdir = $(kde_datadir)/kpoker
rc_DATA = kpokerui.rc

messages: rc.cpp
	$(XGETTEXT) *.cpp -o $(podir)/kpoker.pot 
