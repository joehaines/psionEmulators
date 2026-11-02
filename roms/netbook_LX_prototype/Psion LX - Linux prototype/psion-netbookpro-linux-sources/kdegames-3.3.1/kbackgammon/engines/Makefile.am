noinst_LTLIBRARIES = libkbgengines.la

libkbgengines_la_SOURCES = dummy.cpp
libkbgengines_la_LIBADD  = offline/libkbgoffline.la gnubg/libkbggnubg.la \
                           generic/libkbggeneric.la fibs/libkbgfibs.la   \
			   nextgen/libkbgnextgen.la

INCLUDES= $(all_includes)

METASOURCES = AUTO

SUBDIRS = offline generic fibs gnubg nextgen

dummy.cpp:
	echo > dummy.cpp

