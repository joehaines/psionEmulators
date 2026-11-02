INCLUDES =  \
	-I$(top_srcdir)/gdk-card-image             \
	-I$(top_srcdir)/libgames-support           \
	-DDATADIR=\""$(datadir)"\"		   \
	$(GNOME_GAMES_CXXFLAGS)			   \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"

bin_PROGRAMS = blackjack

blackjack_SOURCES = 		\
	strategy.h	\
	strategy.cpp	\
	player.h	\
        player.cpp	\
	hand.h		\
	hand.cpp	\
	game.h		\
	game.cpp	\
	blackjack.h	\
	blackjack.cpp	\
	splash.h	\
	splash.cpp	\
	slot.h		\
	slot.cpp	\
	dialog.h	\
	dialog.cpp	\
	events.h	\
	events.cpp	\
	draw.h		\
	draw.cpp	\
	menu.h		\
	menu.cpp	\
	press_data.h    \
	press_data.cpp  \
	card.h		\
	card.cpp	\
	chips.h		\
	chips.cpp

blackjack_LDADD = $(GNOME_GAMES_LIBS) \
	../../gdk-card-image/libgdkcardimage.la \
	../../libgames-support/libgames-support.la \
	@UCB_LDFLAGS@

