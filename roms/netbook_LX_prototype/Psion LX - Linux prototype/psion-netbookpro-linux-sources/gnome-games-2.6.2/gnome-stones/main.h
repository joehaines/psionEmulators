/* gnome-stones - sound.h
 *
 * Time-stamp: <2003-06-19 10:06:32 callum>
 *
 * Copyright (C) 2001, 2003 Michal Benes
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#ifndef MAIN_H
#define MAIN_H

#include <gnome.h>
#include <gconf/gconf-client.h>

/* You should leave 'USE_GNOME_CANVAS' undefined, because this game
   currently doesn't support all features with gnome_canvas stuff
   enabled.  */
#undef USE_GNOME_CANVAS

#undef USE_KEY_RELEASE

#define APP_NAME "gnome-stones"

/* Definitions */

#define START_DELAY 3000
#define END_DELAY 3000
#define CURTAIN_DELAY 20
#define COUNTDOWN_DELAY 20

#define GAME_COLS  20
#define GAME_ROWS 12
#define GAME_SCROLL_MAX 3
#define GAME_SCROLL_MIN 6

#ifdef USE_KEY_RELEASE
#define GAME_EVENTS (GDK_KEY_PRESS_MASK             |\
		     GDK_KEY_RELEASE_MASK)
;
#else
#define GAME_EVENTS (GDK_KEY_PRESS_MASK)
#endif


void quit_cb (GtkWidget *widget, gpointer data);
GConfClient *get_gconf_client (void);

void joystick_set_properties (guint32 deviceid, gfloat switch_level);
gboolean load_game (const gchar *filename, guint start_cave);

#endif /* MAIN_H */



/* Local Variables: */
/* mode:c */
/* eval:(load-library "time-stamp") */
/* eval:(make-local-variable 'write-file-hooks) */
/* eval:(add-hook 'write-file-hooks 'time-stamp) */
/* eval:(setq time-stamp-format '(time-stamp-yyyy/mm/dd time-stamp-hh:mm:ss user-login-name)) */
/* End: */
