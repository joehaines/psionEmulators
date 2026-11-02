// -*- mode:C++; tab-width:2; c-basic-offset:2; indent-tabs-mode:nil -*-

/* Blackjack - draw.cpp
 * Copyright (C) 2003 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 1998 Jonathan Blandford <jrb@mit.edu>
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

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <dirent.h>
#include <ctype.h>
#include <iostream>
using namespace std;

#include "draw.h"
#include "blackjack.h"
#include "slot.h"
#include "card.h"
#include "chips.h"

#include "game.h"
#include "hand.h"

gchar *dealer_markup = NULL;
gchar *player_markup = NULL;

static void
bj_draw_hand_counts ()
{
  bj_game_show_hand_counts ();
}

void
bj_draw_set_dealer_text (gchar *markup)
{
  if (dealer_markup)
    g_free (dealer_markup);

  if (markup)
    dealer_markup = g_strdup (markup);
  else
    dealer_markup = NULL;
}

void
bj_draw_set_player_text (gchar *markup)
{
  if (player_markup)
    g_free (player_markup);

  if (markup)
    player_markup = g_strdup (markup);
  else
    player_markup = NULL;
}

static void
bj_draw_paint_text (void)
{
  PangoLayout *layout;
  gint x, y, width, height;

  if (playing_area)
    {
      layout = gtk_widget_create_pango_layout (playing_area, "");
      if (dealer_markup)
        pango_layout_set_markup (layout, dealer_markup, -1);
      x = y = 10;
      gtk_paint_layout (playing_area->style, playing_area->window, 
                        (GtkStateType) GTK_WIDGET_STATE (playing_area),
                        FALSE, NULL, playing_area, NULL, x, y, layout);
      pango_layout_get_size (layout, &width, &height);
      g_object_unref (layout);

      layout = gtk_widget_create_pango_layout (playing_area, "");
      if (player_markup)
        pango_layout_set_markup (layout, player_markup, -1);
      x = 10;
      y = y + PANGO_PIXELS (height);
      gtk_paint_layout (playing_area->style, playing_area->window, 
                        (GtkStateType) GTK_WIDGET_STATE (playing_area),
                        FALSE, NULL, playing_area, NULL, x, y, layout);
      g_object_unref (layout);
    }
}

void
bj_draw_playing_area_text (gchar *markup, gint x, gint y)
{
  PangoLayout *layout;
  if (playing_area)
    {
      layout = gtk_widget_create_pango_layout (playing_area, "");
      pango_layout_set_markup (layout, markup, -1);
      gtk_paint_layout (playing_area->style, playing_area->window, 
                        (GtkStateType) GTK_WIDGET_STATE (playing_area),
                        FALSE, NULL, playing_area, NULL, x, y, layout);
      g_object_unref (layout);
    }
}

static void
bj_draw_chips ()
{
  GList* chip_stack;
  gint x, y;
  GList* chip_list;
  GdkPixbuf *image;

  for (chip_stack = bj_chip_stack_get_list (); chip_stack; 
       chip_stack = chip_stack->next) 
    {
      hstack_type hstack = (hstack_type) chip_stack->data;
      chip_list = hstack->chips;
      if (chip_list != NULL)
        {
          chip_list = g_list_nth (chip_list, hstack->length - hstack->exposed);
          
          x = hstack->x;
          y = hstack->y;
          
          for (; chip_list; chip_list = chip_list->next)
            {
              chip_type *chip = (chip_type*)chip_list->data;
              
              image = bj_chip_get_pixbuf (bj_chip_get_id (chip->value));
	
              if (image != NULL)
                gdk_draw_pixbuf (surface,
                                 draw_gc,
                                 image,
                                 0, 0, 
                                 x, y, 
                                 -1, -1, 
                                 GDK_RGB_DITHER_MAX,
                                 0, 0);
	
              x += hstack->dx; y -= hstack->dy;
            }
        }
    }
  gdk_gc_set_clip_mask (draw_gc, NULL);
}

static void
bj_draw_cards ()
{
  GList* slot;
  gint x, y;
  GList* card_list;
  GdkPixmap *image;

  gdk_gc_set_clip_mask (draw_gc, bj_card_get_mask ()); 
  
  for (slot = bj_slot_get_list (); slot; slot = slot->next) 
    {
      hslot_type hslot = (hslot_type) slot->data;
      
      if ((card_list = hslot->cards))
        {
          card_list = g_list_nth (card_list, hslot->length - hslot->exposed);
          
          x = hslot->x;
          y = hslot->y;
          
          for (; card_list; card_list = card_list->next)
            {
              card_type *card = (card_type*)card_list->data;

              if (card->direction == DOWN) 
                image = bj_card_get_back_pixmap ();
              else 
                image = bj_card_get_picture (card->suit, card->value);
	
              gdk_gc_set_clip_origin (draw_gc, x, y);
              if (image != NULL)
                gdk_draw_drawable (surface, draw_gc, image, 0, 0, x, y, -1, -1);
	
              x += hslot->dx; y += hslot->dy;
            }
        }
    }
  gdk_gc_set_clip_mask (draw_gc, NULL); 
}

void
bj_draw_take_snapshot ()
{
  GList* slot;
  GdkPixbuf *chip_pixbuf;
  gint x, y, x_offset;

  gdk_draw_rectangle (surface, draw_gc, TRUE, 0, 0, -1, -1);

  // Draw chips under source chip stacks
  x = CHIP_X_ORIGIN;
  y = CHIP_Y_ORIGIN;
  x_offset = 0;
  for (int i=0; i < 4; i++)
    {
      chip_pixbuf = bj_chip_get_pixbuf (i);
      if (chip_pixbuf != NULL)
        gdk_draw_pixbuf (surface,
                         draw_gc,
                         chip_pixbuf,
                         0, 0, 
                         x + x_offset, y, 
                         -1, -1, 
                         GDK_RGB_DITHER_MAX,
                         0, 0);
      x_offset += bj_chip_get_width () + 5;
    }

  for (slot = bj_slot_get_list (); slot; slot = slot->next)
    {
      GdkPixbuf *slot_pixbuf;
      slot_pixbuf = bj_slot_get_pixbuf ();
      if (slot_pixbuf != NULL)
        gdk_draw_pixbuf (surface,
                         draw_gc,
                         slot_pixbuf,
                         0, 0, 
                         ((hslot_type) slot->data)->x, 
                         ((hslot_type) slot->data)->y,
                         -1, -1, 
                         GDK_RGB_DITHER_MAX,
                         0, 0);
    }
  bj_draw_cards ();
  bj_draw_chips ();
  if (bj_game_is_active () && bj_get_show_probabilities ())
    bj_draw_paint_text ();
  gdk_window_set_back_pixmap (playing_area->window, surface, 0);
}

void
bj_draw_refresh_screen()
{
  bj_draw_take_snapshot();
  gdk_window_clear (playing_area->window);
  if (bj_game_is_active () && bj_get_show_probabilities ())
    bj_draw_paint_text ();
  bj_draw_hand_counts ();
}
