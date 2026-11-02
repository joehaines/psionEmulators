/* AisleRiot - card.c
 * Copyright (C) 1998, 2003 Jonathan Blandford <jrb@mit.edu>
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

#include "sol.h"
#include "card.h"
#include <gdk-pixbuf/gdk-pixbuf.h>

GdkPixmap *default_background_pixmap; 
GdkPixbuf *slot_pixbuf;
GdkBitmap *mask;

typedef struct sol_deck {
  GdkPixmap *card[52];
  GdkPixmap *back;
} sol_deck;

static sol_deck sol_cards;

static GdkPixmap* get_pixmap (const char* filename);

GdkPixmap* get_card_picture (gint suit, gint rank ) 
{
  if (rank == 14) rank = ACE;

  if (rank >= ACE && rank <= KING) {
    return sol_cards.card[(rank - 1) + suit * 13];
  }
  return NULL;
}

GdkPixmap* get_background_pixmap () {
  
  return default_background_pixmap;
}

GdkPixbuf* get_slot_pixbuf () {
  return slot_pixbuf;
}

GdkPixmap* get_card_back_pixmap () {
  return sol_cards.back;
}

int get_card_width () {
  int width, height;
  gdk_drawable_get_size(get_card_back_pixmap(), &width, &height);
  return width;
}

int get_card_height () {
  int width, height;
  gdk_drawable_get_size(get_card_back_pixmap(), &width, &height);
  return height;
}

int get_horiz_offset () {
  return get_card_width() + x_spacing;
}

int get_vert_offset () {
  return get_card_height() + y_spacing;
}

int get_vert_start () {
  return 30;
}

int get_horiz_start () {
  return 30;
}

static GdkPixmap* get_pixmap (const char* filename)
{
  GdkPixmap* ret;
  GdkPixbuf *im;
  char* fullname = gnome_program_locate_file (NULL,
		  GNOME_FILE_DOMAIN_APP_PIXMAP,
		  filename, TRUE, NULL);

  if (fullname == NULL)
    return NULL; 

  im = gdk_pixbuf_new_from_file (fullname, NULL);
  if (im != NULL) {
    gdk_pixbuf_render_pixmap_and_mask (im, &ret, NULL, 127);
    gdk_pixbuf_unref (im);
  } else {
    ret = NULL;
  } 
  g_free (fullname);

  return ret;
}

static GdkPixbuf* get_pixbuf (const char* filename)
{
  GdkPixbuf *im;
  char* fullname = gnome_program_locate_file (NULL,
                                              GNOME_FILE_DOMAIN_APP_PIXMAP,
                                              filename, TRUE, NULL);

  if (fullname == NULL)
    return NULL; 

  im = gdk_pixbuf_new_from_file (fullname, NULL);
  g_free (fullname);

  return im;
}

void load_pixmaps (GtkWidget* app, GdkCardDeckOptions deck_options) 
{
  /* KDE image order: clubs, spades, hearts, diamonds.
     GDK image order: clubs, diamonds, hearts, spades.

     The KDE images also are in reverse order, starting from Ace
  */   

  int kde_suit[4] = { CLUB, SPADE, HEART, DIAMOND };
  int card, suit;

  for (suit = CLUB; suit <= SPADE; suit++) {
    for (card = ACE; card <= KING; card++) {
      char name[32];
      GdkPixbuf *img;
      int index;

      if (card == ACE)
        index = 0;
      else
        index = 14 - card;

      sprintf(name, "cards-aisleriot/%d.png", index * 4 + suit + 1);

      sol_cards.card[card - 1 + kde_suit[suit] * 13] = img = get_pixmap(name);
    }
  }

  sol_cards.back = get_pixmap("cards-aisleriot/back.png");

  mask = NULL;
  slot_pixbuf = get_pixbuf ("cards-aisleriot/plain.png");
  default_background_pixmap = get_pixmap ("cards-aisleriot/baize.png");
}

void free_pixmaps () 
{
  gtk_object_destroy (card_deck);
  if (slot_pixbuf != NULL)
    gdk_pixbuf_unref (slot_pixbuf);
  if (default_background_pixmap != NULL)
    g_object_unref (default_background_pixmap);
}

void add_card(GList** card_list, hcard_type temp_card) {
  if (*card_list)
	 g_list_append(*card_list, temp_card);
  else {
	 *card_list = g_list_alloc();
	 (*card_list)->data = temp_card;
  }
}
