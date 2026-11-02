// -*- mode:C++; tab-width:2; c-basic-offset:2; indent-tabs-mode:nil -*-
/* Blackjack - menu.cpp
 * Copyright (C) 2003 William Jon McCann <mccann@jhu.edu>
 *
 * This game is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 */

#include <config.h>

#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <dirent.h>
#include <ctype.h>
#include <iostream>
using namespace std;

#include "gnome.h"
#include "blackjack.h"
#include "menu.h"
#include "dialog.h"
#include "draw.h"
#include "events.h"

#include "game.h"
#include "hand.h"

static GtkWidget *about = NULL;

static void
restart_game ()
{
  bj_game_new (bj_game_get_rules_file (), &seed);
};

static void
random_seed ()
{
  bj_game_new (bj_game_get_rules_file (), NULL);
};

#if 0
static void
new_rules (GtkWidget* w, gchar* file) 
{
  bj_game_new (file, NULL);
};
#endif

static void
about_destroy_callback (void)
{
  about = NULL;
}
 
static void
help_about_callback ()
{
  GdkPixbuf *pixbuf = NULL;
  const gchar *authors[] = {
	  "William Jon McCann <mccann@jhu.edu>",
	  "Eric Farmer <erfarmer201@comcast.net>",
	  NULL
  };

  const gchar *documenters[] = {
	  NULL
  };

  const gchar *translator_credits = _("translator-credits");

	char *filename = NULL;

	filename = gnome_program_locate_file (NULL,
		GNOME_FILE_DOMAIN_APP_PIXMAP,  ("gnome-blackjack.png"),
		TRUE, NULL);
	if (filename != NULL)
	{
		pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
		g_free (filename);
	}
  
  {
	int i=0;
	while (authors[i] != NULL) { authors[i]=_(authors[i]); i++; }
  }

  if (about) {
    gtk_window_present (GTK_WINDOW (about));
    return;
  }
  about = gnome_about_new ( _("Blackjack"), VERSION,
                            /* copyright notice */
                            "Copyright \xc2\xa9 2003-2004 "
                            "William Jon McCann, Eric Farmer",
                            _("Blackjack provides a casino-style "
                              "blackjack card game."),
                            (const char **)authors,
                            (const char **)documenters,
                            strcmp (translator_credits, "translator-credits") != 0? translator_credits :  NULL,
                            pixbuf);
	
	if (pixbuf != NULL)
		gdk_pixbuf_unref (pixbuf);
  gtk_window_set_transient_for (GTK_WINDOW (about), GTK_WINDOW (app));
  g_signal_connect (GTK_OBJECT (about),
                    "destroy",
                    (GtkSignalFunc) about_destroy_callback,
                    NULL);
  gtk_widget_show (about);
  return;
}

static void
settings_toolbar_callback (GtkWidget *widget, gpointer data)
{
  BonoboDockItem *toolbar_gdi;

  toolbar_gdi = gnome_app_get_dock_item_by_name (GNOME_APP (app), 
                                                 GNOME_APP_TOOLBAR_NAME);
  
  gboolean do_toolbar = GTK_CHECK_MENU_ITEM (widget)->active;
  
  bj_set_show_toolbar (do_toolbar);
}

GnomeUIInfo rules_sub_menu[] = {
  GNOMEUIINFO_END
};

GnomeUIInfo file_menu[] = {
  GNOMEUIINFO_END
};

GnomeUIInfo settings_menu[] = {
  GNOMEUIINFO_TOGGLEITEM_DATA (N_("_Toolbar"), 
                               N_("Show or hide the toolbar"), 
                               settings_toolbar_callback, NULL, NULL),

  GNOMEUIINFO_MENU_PREFERENCES_ITEM (show_preferences_dialog, NULL),

  GNOMEUIINFO_END
};

GnomeUIInfo help_menu[] = {
  GNOMEUIINFO_HELP("blackjack"),

  GNOMEUIINFO_MENU_ABOUT_ITEM (help_about_callback, NULL),

  GNOMEUIINFO_END
};

GnomeUIInfo game_menu[] = {
  
  GNOMEUIINFO_MENU_NEW_GAME_ITEM (random_seed, NULL),

  GNOMEUIINFO_MENU_RESTART_GAME_ITEM (restart_game, NULL),

  GNOMEUIINFO_SEPARATOR,

  GNOMEUIINFO_MENU_HINT_ITEM (show_hint_dialog, NULL),

  GNOMEUIINFO_SEPARATOR,

  GNOMEUIINFO_MENU_EXIT_ITEM (bj_quit_app, NULL),

  GNOMEUIINFO_END
};


GnomeUIInfo top_menu[] = {

  GNOMEUIINFO_MENU_GAME_TREE (game_menu),

  GNOMEUIINFO_MENU_SETTINGS_TREE (settings_menu),

  GNOMEUIINFO_MENU_HELP_TREE (help_menu),

  GNOMEUIINFO_END
};

GnomeUIInfo toolbar[] =
{
  GNOMEUIINFO_ITEM_STOCK (N_("New"), N_("Deal a new game"),
                          random_seed, GTK_STOCK_NEW),

  GNOMEUIINFO_ITEM_STOCK (N_("Restart"), N_("Start this game over"),
                          restart_game, GTK_STOCK_REFRESH),

  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_ITEM_STOCK (N_("Hint"), N_("Suggest a move"),
                          show_hint_dialog, GTK_STOCK_HELP),
  GNOMEUIINFO_END
};

void
bj_menu_create ()
{
  gnome_app_create_menus (GNOME_APP(app), top_menu);
  gnome_app_create_toolbar (GNOME_APP(app), toolbar);

  gboolean value = bj_get_show_toolbar ();
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (settings_menu[0].widget),
                                  value);
  bj_gui_show_toolbar (value);
}

void
bj_menu_install_hints (GnomeApp *app)
{
  gnome_app_install_menu_hints (GNOME_APP (app), top_menu);
}
