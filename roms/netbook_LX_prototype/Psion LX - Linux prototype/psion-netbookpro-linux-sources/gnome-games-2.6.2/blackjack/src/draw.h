/* Blackjack - draw.h
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

#ifndef DRAW_H
#define DRAW_H
#include <gtk/gtk.h>

#define CHIP_X_ORIGIN 25
#define CHIP_Y_ORIGIN 340

void bj_draw_set_dealer_text (gchar *);
void bj_draw_set_player_text (gchar *);
void bj_draw_playing_area_text (gchar *, gint, gint);
void bj_draw_dealer_probabilities ();

void bj_draw_take_snapshot (void);
void bj_draw_refresh_screen (void);

#endif
