INCLUDES = -I$(top_builddir)/libksirtet -I$(top_srcdir)/libksirtet -I$(top_srcdir)/libkdegames/highscore -I$(top_srcdir)/libkdegames $(all_includes)

noinst_LTLIBRARIES             = libksirtetcommon.la
libksirtetcommon_la_LDFLAGS = $(all_libraries) -no-undefined
libksirtetcommon_la_DEPENDENCIES = $(LIB_KDEGAMES_DEP)
libksirtetcommon_la_LIBADD  = $(LIB_KDEGAMES) $(top_builddir)/libksirtet/lib/libksirtetmultiplayers.la $(top_builddir)/libksirtet/base/libksirtetbase.la

noinst_HEADERS              = types.h factory.h misc_ui.h highscores.h \
                              board.h ai.h field.h settings.h inter.h main.h
libksirtetcommon_la_SOURCES = types.cpp factory.cpp misc_ui.cpp \
                              highscores.cpp \
                              board.cpp ai.cpp field.cpp settings.cpp \
                              inter.cpp main.cpp commonprefs.kcfgc
METASOURCES                 = misc_ui.moc board.moc ai.moc field.moc \
                              settings.moc inter.moc main.moc
