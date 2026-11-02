
SUBDIRS = bits missile rock1 rock2 rock3 ship powerups exhaust

POVRAY=x-povray  # Must be Povray 3.0

#all: pngtoppmpgm

#clean:
#	rm -f *~ pngtoppmpgm

#distclean: clean
#	rm -f Makefile

FILES = `cd $(srcdir) && find . \( -name "*.png" \) -print`
spritesdir = $(kde_datadir)/kasteroids/sprites

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(spritesdir)/rock1
	$(mkinstalldirs) $(DESTDIR)$(spritesdir)/rock2
	$(mkinstalldirs) $(DESTDIR)$(spritesdir)/rock3
	$(mkinstalldirs) $(DESTDIR)$(spritesdir)/ship
	$(mkinstalldirs) $(DESTDIR)$(spritesdir)/bits
	$(mkinstalldirs) $(DESTDIR)$(spritesdir)/missile
	$(mkinstalldirs) $(DESTDIR)$(spritesdir)/powerups
	$(mkinstalldirs) $(DESTDIR)$(spritesdir)/shield
	$(mkinstalldirs) $(DESTDIR)$(spritesdir)/exhaust
	@for file in $(FILES); do \
	 echo $(INSTALL_DATA) $(srcdir)/$$file $(DESTDIR)$(spritesdir)/$$file ;\
	 $(INSTALL_DATA) $(srcdir)/$$file $(DESTDIR)$(spritesdir)/$$file ;\
	done 

uninstall-local:
	for file in $(FILES); do \
	 rm -f $(DESTDIR)$(spritesdir)/$$file ;\
	done

