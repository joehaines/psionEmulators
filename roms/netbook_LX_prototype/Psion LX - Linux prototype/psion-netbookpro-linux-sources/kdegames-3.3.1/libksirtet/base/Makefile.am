INCLUDES = -I$(top_srcdir)/libkdegames -I$(top_srcdir)/libkdegames/highscore $(all_includes)

noinst_LTLIBRARIES        = libksirtetbase.la
libksirtetbase_la_LDFLAGS = $(all_libraries)

noinst_HEADERS            = kzoommainwindow.h piece.h gtetris.h factory.h highscores.h \
                            board.h settings.h field.h inter.h main.h
libksirtetbase_la_SOURCES = kzoommainwindow.cpp main.cpp field.cpp piece.cpp highscores.cpp \
                            factory.cpp gtetris.cpp board.cpp settings.cpp \
                            inter.cpp baseprefs.kcfgc
METASOURCES               = AUTO
