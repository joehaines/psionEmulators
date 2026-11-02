
bin_PROGRAMS = LinNeighborhood
LinNeighborhood_SOURCES = browsewrap.c data.c filesystem.c gtk_dialog.c gtk_gui.c \
gtk_mountdlg.c gtk_sharewindow.c gtk_tree.c guiwrap.c io.c libsmb.c lmhosts.c \
main.c preferences.c smbbrowse.c smbmount.c utility.c
noinst_HEADERS = browsewrap.h data.h define.h filesystem.h gtk_dialog.h gtk_gui.h \
gtk_mountdlg.h gtk_sharewindow.h gtk_tree.h guiwrap.h io.h libsmb.h preferences.h \
samba.h smbif.h utility.h

LinNeighborhood_LDADD = @GTK_LIBS@
INCLUDES = @GTK_CFLAGS@ -DLIBDIR=\"$(libdir)\" -DDATADIR=\"$(datadir)\"
LIBS = @GTK_LIBS@ -lpthread

EXTRA_DIST = picture

AM_CFLAGS = -O2 -g -Wall -Werror -DGTK_ENABLE_BROKEN