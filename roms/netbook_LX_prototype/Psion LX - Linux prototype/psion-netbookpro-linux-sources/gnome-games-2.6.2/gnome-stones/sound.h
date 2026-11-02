/* gnome-stones - sound.h
 *
 * Time-stamp: <2003/06/17 14:56:08 mccannwj>
 *
 * Copyright (C) 2001 Michal Benes
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


#ifndef SOUND_H
#define SOUND_H


void sound_init( void );

void sound_close( void );

void sound_play( gint sound_id );

gint sound_register( char *name );

void play_title_music( void );

void stop_title_music( void );

gboolean get_sound_enabled (void);

void set_sound_enabled (gboolean value);


#endif /* SOUND_H */



/* Local Variables: */
/* mode:c */
/* eval:(load-library "time-stamp") */
/* eval:(make-local-variable 'write-file-hooks) */
/* eval:(add-hook 'write-file-hooks 'time-stamp) */
/* eval:(setq time-stamp-format '(time-stamp-yyyy/mm/dd time-stamp-hh:mm:ss user-login-name)) */
/* End: */
