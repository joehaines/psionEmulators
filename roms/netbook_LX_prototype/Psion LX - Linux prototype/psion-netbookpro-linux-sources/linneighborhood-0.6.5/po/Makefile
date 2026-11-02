# Makefile for program source directory in GNU NLS utilities package.
# Copyright (C) 1995-1997, 2000, 2001 by Ulrich Drepper <drepper@gnu.ai.mit.edu>
#
# This file file be copied and used freely without restrictions.  It can
# be used in projects which are not available under the GNU Public License
# but which still want to provide support for the GNU gettext functionality.
# Please note that the actual code is *not* freely available.

PACKAGE = LinNeighborhood
VERSION = 0.6.5

SHELL = /bin/sh


srcdir = /var/home/petern/oe/oe-netbook/tmp/work/linneighborhood-0.6.5-r7/linneighborhood-0.6.5/po
top_srcdir = /var/home/petern/oe/oe-netbook/tmp/work/linneighborhood-0.6.5-r7/linneighborhood-0.6.5
top_builddir = ..
VPATH = /var/home/petern/oe/oe-netbook/tmp/work/linneighborhood-0.6.5-r7/linneighborhood-0.6.5/po

prefix = /usr
exec_prefix = /usr
datadir = /usr/share
localedir = $(datadir)/locale
gettextsrcdir = $(datadir)/gettext/po
subdir = po

INSTALL = /var/home/petern/oe/oe-netbook/tmp/staging/i686-linux/bin/install -c
INSTALL_DATA = ${INSTALL} -m 644
MKINSTALLDIRS = /var/home/petern/oe/oe-netbook/tmp/work/linneighborhood-0.6.5-r7/linneighborhood-0.6.5/mkinstalldirs
mkinstalldirs = $(SHELL) `case "$(MKINSTALLDIRS)" in /*) echo "$(MKINSTALLDIRS)" ;; *) echo "$(top_builddir)/$(MKINSTALLDIRS)" ;; esac`

CC = ccache arm-linux-gcc -march=armv5te -mtune=xscale
GMSGFMT = /var/home/petern/oe/oe-netbook/tmp/staging/i686-linux/bin/msgfmt
MSGFMT = /var/home/petern/oe/oe-netbook/tmp/staging/i686-linux/bin/msgfmt
XGETTEXT = /var/home/petern/oe/oe-netbook/tmp/staging/i686-linux/bin/xgettext
MSGMERGE = msgmerge

DEFS = -DHAVE_CONFIG_H
CFLAGS = -I/var/home/petern/oe/oe-netbook/tmp/staging/arm-linux/include -fexpensive-optimizations -fomit-frame-pointer -frename-registers -O2
CPPFLAGS = -I/var/home/petern/oe/oe-netbook/tmp/staging/arm-linux/include

INCLUDES = -I.. -I$(top_srcdir)/intl

COMPILE = $(CC) -c $(DEFS) $(INCLUDES) $(CPPFLAGS) $(CFLAGS) $(XCFLAGS)

POFILES =  de.po sv.po ja.po fr.po ru.po es.po pt_BR.po tr.po pl.po it.po zh_TW.po
GMOFILES =  de.gmo sv.gmo ja.gmo fr.gmo ru.gmo es.gmo pt_BR.gmo tr.gmo pl.gmo it.gmo zh_TW.gmo
DISTFILES = ChangeLog Makefile.in.in POTFILES.in $(PACKAGE).pot \
$(POFILES) $(GMOFILES)

POTFILES = \
     /var/home/petern/oe/oe-netbook/tmp/work/linneighborhood-0.6.5-r7/linneighborhood-0.6.5/src/gtk_dialog.c \
     /var/home/petern/oe/oe-netbook/tmp/work/linneighborhood-0.6.5-r7/linneighborhood-0.6.5/src/gtk_gui.c \
     /var/home/petern/oe/oe-netbook/tmp/work/linneighborhood-0.6.5-r7/linneighborhood-0.6.5/src/gtk_mountdlg.c \
     /var/home/petern/oe/oe-netbook/tmp/work/linneighborhood-0.6.5-r7/linneighborhood-0.6.5/src/gtk_tree.c \
     /var/home/petern/oe/oe-netbook/tmp/work/linneighborhood-0.6.5-r7/linneighborhood-0.6.5/src/io.c \
     /var/home/petern/oe/oe-netbook/tmp/work/linneighborhood-0.6.5-r7/linneighborhood-0.6.5/src/preferences.c \
     /var/home/petern/oe/oe-netbook/tmp/work/linneighborhood-0.6.5-r7/linneighborhood-0.6.5/src/smbbrowse.c \
     /var/home/petern/oe/oe-netbook/tmp/work/linneighborhood-0.6.5-r7/linneighborhood-0.6.5/src/smbmount.c \
     /var/home/petern/oe/oe-netbook/tmp/work/linneighborhood-0.6.5-r7/linneighborhood-0.6.5/src/utility.c \
     /var/home/petern/oe/oe-netbook/tmp/work/linneighborhood-0.6.5-r7/linneighborhood-0.6.5/src/gtk_sharewindow.c

CATALOGS =  de.gmo sv.gmo ja.gmo fr.gmo ru.gmo es.gmo pt_BR.gmo tr.gmo pl.gmo it.gmo zh_TW.gmo

.SUFFIXES:
.SUFFIXES: .c .o .po .pox .gmo .mo

.c.o:
	$(COMPILE) $<

.po.pox:
	$(MAKE) $(PACKAGE).pot
	$(MSGMERGE) $< $(srcdir)/$(PACKAGE).pot -o $*.pox

.po.mo:
	$(MSGFMT) -o $@ $<

.po.gmo:
	file=$(srcdir)/`echo $* | sed 's,.*/,,'`.gmo \
	  && rm -f $$file && $(GMSGFMT) --statistics -o $$file $<


all: all-yes

all-yes: $(CATALOGS)
all-no:

# Note: Target 'all' must not depend on target '$(srcdir)/$(PACKAGE).pot',
# otherwise packages like GCC can not be built if only parts of the source
# have been downloaded.

$(srcdir)/$(PACKAGE).pot: $(POTFILES) $(srcdir)/POTFILES.in
	$(XGETTEXT) --default-domain=$(PACKAGE) --directory=$(top_srcdir) \
	  --add-comments --keyword=_ --keyword=N_ \
	  --files-from=$(srcdir)/POTFILES.in \
	&& test ! -f $(PACKAGE).po \
	   || ( rm -f $(srcdir)/$(PACKAGE).pot \
		&& mv $(PACKAGE).po $(srcdir)/$(PACKAGE).pot )


install: install-exec install-data
install-exec:
install-data: install-data-yes
	if test "$(PACKAGE)" = "gettext"; then \
	  $(mkinstalldirs) $(DESTDIR)$(gettextsrcdir); \
	  $(INSTALL_DATA) $(srcdir)/Makefile.in.in \
			  $(DESTDIR)$(gettextsrcdir)/Makefile.in.in; \
	else \
	  : ; \
	fi
install-data-no: all
install-data-yes: all
	$(mkinstalldirs) $(DESTDIR)$(datadir)
	@catalogs='$(CATALOGS)'; \
	for cat in $$catalogs; do \
	  cat=`basename $$cat`; \
	  lang=`echo $$cat | sed 's/\.gmo$$//'`; \
	  dir=$(localedir)/$$lang/LC_MESSAGES; \
	  $(mkinstalldirs) $(DESTDIR)$$dir; \
	  if test -r $$cat; then \
	    $(INSTALL_DATA) $$cat $(DESTDIR)$$dir/$(PACKAGE).mo; \
	    echo "installing $$cat as $(DESTDIR)$$dir/$(PACKAGE).mo"; \
	  else \
	    $(INSTALL_DATA) $(srcdir)/$$cat $(DESTDIR)$$dir/$(PACKAGE).mo; \
	    echo "installing $(srcdir)/$$cat as" \
		 "$(DESTDIR)$$dir/$(PACKAGE).mo"; \
	  fi; \
	done

# Define this as empty until I found a useful application.
installcheck:

uninstall:
	catalogs='$(CATALOGS)'; \
	for cat in $$catalogs; do \
	  cat=`basename $$cat`; \
	  lang=`echo $$cat | sed 's/\.gmo$$//'`; \
	  rm -f $(DESTDIR)$(localedir)/$$lang/LC_MESSAGES/$(PACKAGE).mo; \
	done
	if test "$(PACKAGE)" = "gettext"; then \
	  rm -f $(DESTDIR)$(gettextsrcdir)/Makefile.in.in; \
	else \
	  : ; \
	fi

check: all

dvi info tags TAGS ID:

mostlyclean:
	rm -f core core.* *.pox $(PACKAGE).po *.new.po
	rm -fr *.o

clean: mostlyclean

distclean: clean
	rm -f Makefile Makefile.in POTFILES *.mo

maintainer-clean: distclean
	@echo "This command is intended for maintainers to use;"
	@echo "it deletes files that may require special tools to rebuild."
	rm -f $(GMOFILES)

distdir = ../$(PACKAGE)-$(VERSION)/$(subdir)
dist distdir:
	$(MAKE) update-po
	@$(MAKE) dist2
# This is a separate target because 'update-po' must be executed before.
dist2: $(DISTFILES)
	dists="$(DISTFILES)"; \
	for file in $$dists; do \
	  cp -p $(srcdir)/$$file $(distdir); \
	done

update-po: Makefile
	$(MAKE) $(PACKAGE).pot
	if test "$(PACKAGE)" = "gettext"; then PATH=`pwd`/../src:$$PATH; fi; \
	cd $(srcdir); \
	catalogs='$(GMOFILES)'; \
	for cat in $$catalogs; do \
	  cat=`basename $$cat`; \
	  lang=`echo $$cat | sed 's/\.gmo$$//'`; \
	  echo "$$lang:"; \
	  if $(MSGMERGE) $$lang.po $(PACKAGE).pot -o $$lang.new.po; then \
	    mv -f $$lang.new.po $$lang.po; \
	  else \
	    echo "msgmerge for $$cat failed!"; \
	    rm -f $$lang.new.po; \
	  fi; \
	done
	$(MAKE) update-gmo

update-gmo: Makefile $(GMOFILES)
	@:

POTFILES: POTFILES.in
	( if test 'x$(srcdir)' != 'x.'; then \
	    posrcprefix='$(top_srcdir)/'; \
	  else \
	    posrcprefix="../"; \
	  fi; \
	  rm -f $@-t $@ \
	    && (sed -e '/^#/d' -e '/^[ 	]*$$/d' \
		    -e "s@.*@	$$posrcprefix& \\\\@" < $(srcdir)/$@.in \
		| sed -e '$$s/\\$$//') > $@-t \
	    && chmod a-w $@-t \
	    && mv $@-t $@ )

Makefile: Makefile.in.in ../config.status POTFILES
	cd .. \
	  && CONFIG_FILES=$(subdir)/$@.in CONFIG_HEADERS= \
	       $(SHELL) ./config.status

# Tell versions [3.59,3.63) of GNU make not to export all variables.
# Otherwise a system limit (for SysV at least) may be exceeded.
.NOEXPORT:
