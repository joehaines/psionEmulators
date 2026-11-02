/* -*- mode:C; indent-tabs-mode:t; tab-width:8; c-basic-offset:2; -*- */

/* gnome-stones - sound.c
 *
 * Time-stamp: <2003/06/19 16:11:28 mccannwj>
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

#include <esd.h>
#include <gnome.h>
#include "sound.h"

gboolean sound_enabled = TRUE;

gint title_music = -1;
gboolean playing_title_music = FALSE;

#define MAX_SAMPLES 1024

gint samples[MAX_SAMPLES];
gint numsamples=0;


gboolean
get_sound_enabled ()
{
  return sound_enabled;
}

void
set_sound_enabled (gboolean value)
{
  sound_enabled = value;
  if (! sound_enabled)
    stop_title_music ();
}

void
sound_init (void)
{
  numsamples = 0; 
  /* g_print ( "gnome-stones: sound init\n" ); */
}

void
sound_close (void)
{
#ifndef NO_ESD
 int i;

 stop_title_music ();

 for (i=0; i<numsamples; ++i)
   esd_sample_free (gnome_sound_connection_get (), samples[i]);
 
 /* g_print ( "gnome-stones: sound close\n" ); */
 gnome_sound_shutdown ();
#endif /* NO_ESD */
}

void
sound_play (gint sound_id)
{
#ifndef NO_ESD
 if (sound_id < 0)
   return;

 if (! sound_enabled)
   return;

/* we are waiting for the esound hackers to implement esd_sample_kill */
/* FIXME: esd_sample_kill( gnome_sound_connection, sound_id );*/
 esd_sample_play (gnome_sound_connection_get (), sound_id);
#endif
}


gint
sound_register (char *name)
{
 gint sample_id = -1;
#ifndef NO_ESD
 char *buf; 
 char *fullname;


 if (numsamples >= MAX_SAMPLES-1)
   return -1;

 buf = g_build_filename ("gnome-stones", name, NULL);
 fullname = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_APP_SOUND,
                                       buf, TRUE, NULL);

 sample_id = gnome_sound_sample_load (name, fullname);

 if (sample_id >= 0)
   samples[numsamples++] = sample_id;
/* else
   g_print ("gnome-stones: cannot register sound %s\n", buf); */

 g_free (fullname);
 g_free (buf);
#endif
 return sample_id;
}


void
play_title_music (void)
{
#ifndef NO_ESD
  if (! sound_enabled)
    return;
  
  if (title_music < 0)
    title_music = sound_register ("title.wav");
  
  if(! playing_title_music && title_music>=0)
    {
      playing_title_music = TRUE; 
      esd_sample_loop (gnome_sound_connection_get (), title_music);
    }
#endif
}

void
stop_title_music (void)
{
#ifndef NO_ESD
 if (playing_title_music)
   {
     playing_title_music = FALSE; 

     /* FIXME: we want esd_sample_kill once it is implemented */
     esd_sample_stop (gnome_sound_connection_get (), title_music);
   }
#endif
}




/* Local Variables: */
/* mode:c */
/* eval:(load-library "time-stamp") */
/* eval:(make-local-variable 'write-file-hooks) */
/* eval:(add-hook 'write-file-hooks 'time-stamp) */
/* eval:(setq time-stamp-format '(time-stamp-yyyy/mm/dd time-stamp-hh:mm:ss user-login-name)) */
/* End: */
