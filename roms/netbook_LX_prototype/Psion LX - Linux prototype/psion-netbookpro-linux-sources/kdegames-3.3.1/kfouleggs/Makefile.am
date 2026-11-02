INCLUDES = -I$(top_builddir)/libksirtet -I$(top_srcdir)/libksirtet -I$(top_srcdir)/libkdegames -I$(top_srcdir)/libkdegames/highscore $(all_includes)

SUBDIRS = pics

bin_PROGRAMS      = kfouleggs
kfouleggs_LDADD   = $(top_builddir)/libksirtet/common/libksirtetcommon.la
kfouleggs_LDFLAGS = $(all_libraries) $(KDE_RPATH)
kfouleggs_SOURCES = piece.cpp board.cpp ai.cpp field.cpp main.cpp prefs.kcfgc
METASOURCES       = board.moc ai.moc field.moc main.moc

rcdir   = $(kde_datadir)/kfouleggs
rc_DATA = kfouleggsui.rc

xdg_apps_DATA = kfouleggs.desktop
kde_kcfg_DATA = kfouleggs.kcfg

appdatadir = $(kde_datadir)/kfouleggs
appdata_DATA = eventsrc

messages: rc.cpp
	$(XGETTEXT) *.cpp -o $(podir)/kfouleggs.pot

# for system-wide highscore file
DESTBIN = $(DESTDIR)$(bindir)/$(bin_PROGRAMS)
DESTSCORES = $(HIGHSCORE_DIRECTORY)/$(bin_PROGRAMS).scores

install-data-local:
	@(test x$(HIGHSCORE_DIRECTORY) != x \
	&& echo "********************************************************" \
	&& echo "" \
	&& echo "This game is installed sgid \"games\" to use the" \
	&& echo "system-wide highscore file (in "$(HIGHSCORE_DIRECTORY)")." \
	&& echo "" \
	&& echo "If the system-wide highscore file does not exist, it is" \
        && echo "created with the correct ownership and permissions. See the" \
        && echo "INSTALL file in \"kdegames/libkdegames/highscore\" for details." \
	&& echo "" \
	&& echo "********************************************************" \
	) || true

install-exec-hook:
	@(test x$(HIGHSCORE_DIRECTORY) != x \
        && ((chown root:games $(DESTBIN) && chmod 2755  $(DESTBIN)) \
	|| echo "Error: Could not install the game with correct permissions !!" \
	)) || true

	@(test x$(HIGHSCORE_DIRECTORY) != x \
        && ((touch $(DESTSCORES) && chown root.games $(DESTSCORES) \
	&& chmod 0660 $(DESTSCORES)) \
	|| echo "Error: Could not create system-wide highscore file with correct permissions !!" \
	)) || true
