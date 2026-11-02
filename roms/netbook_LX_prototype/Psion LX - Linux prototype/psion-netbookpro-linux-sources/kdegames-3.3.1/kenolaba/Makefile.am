SUBDIRS = toolbar bitmaps

INCLUDES = -I$(top_srcdir)/libkdegames $(all_includes)
METASOURCES = AUTO
KDE_ICON = kenolaba

bin_PROGRAMS = kenolaba
kenolaba_SOURCES = Board.cpp Move.cpp BoardWidget.cpp AbTop.cpp \
		 kenolaba.cpp Spy.cpp Ball.cpp Network.cpp EvalDlg.ui \
         EvalDlgImpl.cpp EvalScheme.cpp
kenolaba_LDFLAGS = $(all_libraries) $(KDE_RPATH)
kenolaba_LDADD = $(LIB_KDEGAMES)
kenolaba_DEPENDENCIES = $(LIB_KDEGAMES_DEP)

xdg_apps_DATA = kenolaba.desktop

rcdir = $(kde_datadir)/kenolaba
rc_DATA = kenolabaui.rc

messages: rc.cpp
	$(XGETTEXT) *.cpp -o $(podir)/kenolaba.pot
