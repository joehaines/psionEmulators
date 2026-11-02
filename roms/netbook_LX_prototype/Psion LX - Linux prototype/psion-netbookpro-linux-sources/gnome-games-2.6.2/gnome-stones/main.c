/* -*- mode:C; indent-tabs-mode:t; tab-width:8; c-basic-offset:2; -*- */

/* gnome-stones - main.c
 *
 * Time-stamp: <2003-07-22 07:32:46 callum>
 *
 * Copyright (C) 1998, 2003 Carsten Schaar
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>
#include <libgnomeui/gnome-window-icon.h>
#include <stdlib.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>
#include "main.h"
#include "game.h"
#include "cave.h"
#include "object.h"
#include "status.h"
#include "player.h"
#include "preferences.h"
#include "io.h"
#include "view.h"
#include "sound.h"
#include <X11/Xlib.h>
#include <X11/Xatom.h>

/*****************************************************************************/
/* Used widgets. */

static GtkWidget     *gstones_view;
static GdkPixmap     *title_image;
static GdkPixmap     *title_template;

static gint           player_x_direction= 0;
static gint           player_y_direction= 0;
static gboolean       player_push       = FALSE;


/****************************************************************************/
/* Some constants.  */

/* This message will be printed in the statusline, if no message was
   specified in a game file.  */

static char *default_message= N_("This is GNOME Stones");
static GConfClient *gconf_client;


/****************************************************************************/
/* Commandline options.  */

/* Command-line arguments understood by this module.  */
static const struct poptOption options[] = {
  {"game", 'g', POPT_ARG_STRING, &default_game, 0,
   N_("Game to play"), N_("FILENAME")},
  {NULL, '\0', 0, NULL, 0}
};

/****************************************************************************/
/* The menus */
static GnomeUIInfo game_menu[];
static GnomeUIInfo help_menu[];
static GnomeUIInfo settings_menu[];
static GnomeUIInfo main_menu[];


/****************************************************************************/

static void
curtain_start (GStonesCave *cave);


static void
game_start_cb (GtkWidget *widget, gpointer data);


static void
show_scores_dialog (gint pos);

static void
menu_set_sensitive (gboolean state);


/****************************************************************************/
/* Image stuff  */

static GdkPixbuf *
load_image_from_path (const char *relative_name)
{
  gchar         *filename;
  GdkPixbuf *image= NULL;

  filename= gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_APP_PIXMAP,
                                       relative_name, TRUE, NULL);
  if (filename)
    image= gdk_pixbuf_new_from_file (filename, NULL);

  g_free (filename);
  
  if (!image)
    {
      GtkWidget *widget;
      char       buffer[1024];
      g_snprintf (buffer, sizeof(buffer), 
		  _("An error occured while loading the image file \"%s\".\n"
		    "Please make sure, that GNOME Stones is "
		    "correctly installed!"), relative_name);
					    
      widget= gtk_message_dialog_new (GTK_WINDOW(app), 
                                      GTK_DIALOG_DESTROY_WITH_PARENT,
                                      GTK_MESSAGE_ERROR, 
                                      GTK_BUTTONS_OK, 
                                      buffer);
      gtk_dialog_run(GTK_DIALOG(widget));

      gtk_widget_destroy(widget);
    }
  
  return image;
}

static void
draw_text_centered (GdkDrawable *pixmap, gchar *text)
{
  GdkGC *gc;
  gchar *markup;
  gchar *template = "<span size=\"%d\" weight=\"bold\" foreground=\"black\">%s</span>";
  PangoRectangle extent;
  gint text_size;
  gint x,y;
  PangoLayout *layout;

  g_return_if_fail (pixmap);
  
  /* FIXME: This involves an ugly, ugly hack. */

  gc = gstones_view->style->black_gc;
  layout = gtk_widget_create_pango_layout (gstones_view, "");
  pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);
  text_size = 36*PANGO_SCALE; /* A guess. */
  markup = g_strdup_printf (template, text_size, text);
  pango_layout_set_markup (layout, markup, -1);
  pango_layout_get_extents (layout, &extent, NULL);
  /* Now correct the guess. */
  text_size = text_size*GAME_COLS*STONE_SIZE*0.8*PANGO_SCALE/extent.width;
  g_free (markup);
  markup = g_strdup_printf (template, text_size, text);
  pango_layout_set_markup (layout, markup, -1);
  pango_layout_get_extents (layout, NULL, &extent);
  x = (GAME_COLS*STONE_SIZE - extent.width/PANGO_SCALE)/2;
  y = (GAME_ROWS*STONE_SIZE - extent.height/PANGO_SCALE)/2;
  
  gdk_draw_layout (pixmap, gc, x, y, layout);
  g_free (markup);
  g_object_unref (layout);
}

static void
title_image_load (void)
{
  GdkPixbuf *image;
  GdkGC *gc;
  GdkPixmap *tile;

  image = load_image_from_path ("gnome-stones/title-tile.png");
  if (image != NULL) 
    {
      gdk_pixbuf_render_pixmap_and_mask (image, &tile, NULL, 127);
      gdk_pixbuf_unref (image);
    }

  title_template = gdk_pixmap_new (gstones_view->window,
                                   GAME_COLS * STONE_SIZE,
                                   GAME_ROWS * STONE_SIZE, -1);

  gc = gdk_gc_new (gstones_view->window);
  if (tile != NULL)
    {
      gdk_gc_set_tile (gc, tile);
      gdk_gc_set_fill (gc, GDK_TILED);
      gdk_draw_rectangle (title_template, gc,
                          TRUE, 0, 0, -1, -1);
    }

  draw_text_centered (title_template, _("GNOME Stones"));

  title_image = gdk_pixmap_new (gstones_view->window,
                                GAME_COLS * STONE_SIZE,
                                GAME_ROWS * STONE_SIZE, -1);
}



/****************************************************************************/

/* Game widget handling.  */

#ifdef USE_GNOME_CANVAS
static GnomeCanvasItem *game_items[GAME_COLS][GAME_ROWS];
static GdkPixbuf   *game_current_images[GAME_COLS][GAME_ROWS];


static void
game_update_image (GtkWidget *widget)
{
  GdkPixbuf *image;
  int x1, y1, x2, y2, x, y;

  x1 = x_offset;
  y1 = y_offset;
  x2 = GAME_COLS+x_offset-1;
  y2 = GAME_ROWS+y_offset-1;
  
  for (x = x1; x <= x2; x++)
    for (y = y1; y <= y2; y++)
      {
	
	gboolean cave_mode;
	
	if ((curtain_mode == GAME_CURTAIN_CLOSING && 
	     (x+y > curtain+x_offset+y_offset)) ||
	    (curtain_mode == GAME_CURTAIN_OPENING && 
	     (x+y < curtain+x_offset+y_offset)))
	  {
	    cave_mode= TRUE;
	    image= curtain_pixbuf_image;
	  }
	else if (display_mode == GAME_DISPLAY_IMAGE)
	  {
	    cave_mode= FALSE;
	  }
	else
	  {
	    cave_mode= TRUE;

	    image= cave_get_pixbuf_image (cave, x+1, y+1);
	  }
	
	if (image != game_current_images[x-x_offset][y-y_offset])
	  {
	    gnome_canvas_item_set (game_items[x-x_offset][y-y_offset],
				   "image", image,
				   NULL);
	    game_current_images[x-x_offset][y-y_offset]= image;
	  }
      }

  gnome_canvas_update_now (GNOME_CANVAS (widget));
}
#endif



static void
game_update_title (void)
{
  GdkGC *gc;

  /* Replace image with template.  */
  gdk_draw_drawable (title_image,
                     gstones_view->style->black_gc, title_template,
                     0, 0,
                     0, 0, GAME_COLS*STONE_SIZE, GAME_ROWS*STONE_SIZE);

  if (game)
    {
      static char *gametitle = N_("Game title");
      static char *cavename = N_("Start cave");

      guint width, height;
      PangoLayout *layout;
      PangoFontDescription *pfd;
      GList *tmp = g_list_nth (game->start_caves, start_cave);

      layout = gtk_widget_create_pango_layout (gstones_view, _(gametitle));
      pfd = pango_font_description_from_string ("[sans-serif][bold][16]");
      pango_layout_set_font_description (layout, pfd);
      
      pango_layout_get_pixel_size (layout, &width, &height);

#ifdef USE_ORIG_TITLE_SCREEN
      gc = gstones_view->style->black_gc;
#else
      gc = gstones_view->style->black_gc;
#endif

      gdk_draw_layout (title_image, gc,
                       50, 
                       GAME_ROWS*STONE_SIZE-30-height*3-height/2,
                       layout);

      pango_layout_set_text (layout,
                             game->title,
                             strlen (_(game->title)));

      gdk_draw_layout (title_image, gc,
		       80, 
                       GAME_ROWS*STONE_SIZE-30-height*2-height/2,
                       layout);

      pango_layout_set_text (layout, 
                             _(cavename),
                             strlen (_(cavename)));

      gdk_draw_layout (title_image, gc,
		       50, 
                       GAME_ROWS*STONE_SIZE-30-height,
                       layout);

      pango_layout_set_text (layout, 
                             tmp->data,
                             strlen (tmp->data));

      gdk_draw_layout (title_image, gc,
		       80, 
                       GAME_ROWS*STONE_SIZE-30,
                       layout);

    }  

  /* Redraw the game widget.  */
  gtk_widget_queue_draw_area (gstones_view, 0, 0,
                              GAME_COLS * STONE_SIZE,
                              GAME_ROWS * STONE_SIZE);
}


static guint last_keyval= GDK_VoidSymbol;

static gint
game_widget_key_press_callback (GtkWidget   *widget,
				GdkEventKey *event,
				gpointer     client_data)
{
  player_push= event->state & GDK_SHIFT_MASK;
  
  switch (event->keyval)
    {
    case GDK_KP_8:
    case GDK_KP_Up:
    case GDK_Up:
      if (state == STATE_RUNNING)
	{
	  /* Move player.  */
	  player_x_direction=  0;
	  player_y_direction= -1;
	  last_keyval      = event->keyval;
	  return TRUE;
	}
      else if ((state == STATE_TITLE) && game)
	{
	  if (start_cave+1 < g_list_length (game->start_caves))
	    {
	      start_cave++;
	      game_update_title ();
	    }
	  return TRUE;
	}
      return FALSE;
    case GDK_KP_4:
    case GDK_KP_Left:
    case GDK_Left:
      player_x_direction= -1;
      player_y_direction=  0;
      last_keyval      = event->keyval;
      return TRUE;
    case GDK_KP_6:
    case GDK_KP_Right:
    case GDK_Right:
      player_x_direction=  1;
      player_y_direction=  0;
      last_keyval      = event->keyval;
      return TRUE;
    case GDK_KP_2:
    case GDK_KP_Down:
    case GDK_Down:
      if (state == STATE_RUNNING)
	{
	  /* Move player.  */
	  player_x_direction=  0;
	  player_y_direction=  1;
	  last_keyval      = event->keyval;
	  return TRUE;
	}
      else if ((state == STATE_TITLE) && game)
	{
	  if (start_cave > 0)
	    {
	      start_cave--;
	      game_update_title ();
	    }
	  return TRUE;
	}
      return FALSE;
    case GDK_Escape:
      if ((state == STATE_RUNNING) && cave)
	{
	  if (player_push)
	    curtain_start (NULL);
	  else
	    cave_player_die (cave);
	}
      return TRUE;
    case GDK_P:
    case GDK_p:
      if ((state == STATE_RUNNING) && cave)
	{
	  cave_toggle_pause_mode (cave);
	  
	  if (cave->flags & CAVE_PAUSING)
	    gnome_appbar_set_status (GNOME_APPBAR (statusbar), _("Pause"));
	  else
	    gnome_appbar_refresh (GNOME_APPBAR (statusbar));
	}
      return FALSE;
    case GDK_KP_Enter:
    case GDK_Return:
      if (state == STATE_TITLE)
	{
	  game_start_cb (NULL, NULL);
	  return TRUE; 
	}
      return FALSE;
    }

  return FALSE;
};


static gint
game_widget_key_release_callback (GtkWidget   *widget,
				  GdkEventKey *event,
				  gpointer     client_data)
{
  if (last_keyval == event->keyval)
    {
      last_keyval       =  GDK_VoidSymbol;
      return TRUE;
    }

  return FALSE;
}


#ifdef USE_GNOME_CANVAS
/* We need this variable to be global, because we can only fill this
   canvas after loading the needed images.  */
GtkWidget *canvas;

/* This function need 'canvas' and 'curtain_pixbuf_image' to have well
   defined values.  */

static gboolean
game_widget_fill (void)
{
  guint x;
  guint y;
  
  for (x= 0; x < GAME_COLS ; x+= 1)
    for (y= 0; y < GAME_COLS ;  y+= 1)
      {
	game_items[x][y]=
	  gnome_canvas_item_new (GNOME_CANVAS_GROUP (gnome_canvas_root 
						     (GNOME_CANVAS 
						      (canvas))),
				 gnome_canvas_pixbuf_get_type (),
				 "pixbuf", curtain_pixbuf_image,
				 "x", (double) x*STONE_SIZE,
				 "y", (double) y*STONE_SIZE,
				 "width", (double) , gdk_pixbuf_get_width (curtain_pixbuf_image),
				 "height", (double) gdk_pixbuf_get_height (curtain_pixbuf_image),
				 NULL);
	
      }
  
  return TRUE;
}
#endif /* USE_GNOME_CANVAS */

/****************************************************************************/
/* High score enabling/disabling */

static void 
update_score_state (void)
{
        gchar **names = NULL;
        gfloat *scores = NULL;
        time_t *scoretimes = NULL;
	gint top;

	top = gnome_score_get_notable (APP_NAME, NULL, &names,
                                       &scores, &scoretimes);
	if (top > 0) {
		gtk_widget_set_sensitive (game_menu[6].widget, TRUE);
		g_strfreev(names);
		g_free(scores);
		g_free(scoretimes);
	} else {
		gtk_widget_set_sensitive (game_menu[6].widget, FALSE);
	}
}


/****************************************************************************/
/* Timeout stuff

   In the following, we list all needed timeout handles.  */

static guint iteration_timeout= 0;
static guint countdown_timeout= 0;

/****************************************************************************/

GStonesCave *curtain_cave;

static gint
start_cave_delay_timeout (gpointer data);

static void
iteration_start (GStonesCave *cave);

static void
curtain_ready (ViewCurtainMode mode)
{
  switch (mode)
    {
    case VIEW_CURTAIN_OPEN:  

      if (curtain_cave)
	{
	  (GSTONES_VIEW (gstones_view))->curtain_display_mode= 
	     CURTAIN_DISPLAY_NONE;

	  state= STATE_WAITING_TO_START;
	  g_timeout_add (START_DELAY, start_cave_delay_timeout, curtain_cave);
	}
      else
	{
  	  play_title_music();
	  state= STATE_TITLE;
	}
      break;
      

    case VIEW_CURTAIN_CLOSED:
      /* If the iteration has been running, we stop it now.  */
      if (iteration_timeout) g_source_remove (iteration_timeout);
      iteration_timeout=0;
      
      /* An existing cave must be deleted.  */
      if (cave)
	{
	  cave_free (cave);
	  cave= NULL;
	}
      
      if (curtain_cave)
	{
	  view_display_cave (GSTONES_VIEW (gstones_view), curtain_cave);
	  status_set_mode (STATUS_GAME_INFO);
	  
	  iteration_start (curtain_cave);
	  
	  /* Print message to screen.  */
	  if (curtain_cave->message)
	    gnome_appbar_set_default (GNOME_APPBAR (statusbar), curtain_cave->message);
	  else
	    gnome_appbar_set_default (GNOME_APPBAR (statusbar), _(default_message));
	}
      else
	view_display_image (GSTONES_VIEW (gstones_view), title_image);
      
      break;
      default:
      break;
    }
}


/* Start a curtain sequence.  

   'cave' determines, what happens after the curtain sequence.
   Setting 'cave' to NULL means, that the title image will be shown
   after the sequence.  Setting 'cave' to a non NULL value, lets the
   program load a new cave and start it.  */

static void
curtain_start (GStonesCave *cave)
{
  curtain_cave= cave;
  state       = STATE_CURTAIN;
  menu_set_sensitive (cave != NULL);

  
  view_set_curtain_mode (GSTONES_VIEW (gstones_view), 
			 VIEW_CURTAIN_ANIMATE, curtain_ready);
}



/****************************************************************************/


static gint
start_cave_delay_timeout (gpointer data)
{
  GStonesCave *cave= (GStonesCave *) data;
  
  if (state == STATE_WAITING_TO_START)
    { 
      status_set_mode (STATUS_CAVE_INFO);
      cave_start (cave);
      
      state= STATE_RUNNING;
    }

  return FALSE;
}



/*****************************************************************************/
/* Countdown stuff

   The following functions are needed to implement the timer countdown,
   when the player has finished a level.  */

static gint
countdown_timeout_function (gpointer data)
{
  GStonesCave *cave   = (GStonesCave *)data;
  GStonesCave *newcave= NULL;
  
  if (state == STATE_COUNTDOWN)
    {
      if (cave->timer > 0)
	{
	  cave->timer-= 1000;
	  if (cave->timer < 0) cave->timer= 0;

	  player_set_time (player, cave->timer, FALSE);
	  
	  /* FIXME: The added value should be configureable on a per
             game basis.  */
	  player_inc_score (player, 1);
	  
	  return TRUE;
	}
      
      if (cave->next)
	newcave= gstones_cave_load (game, cave->next);
	
      curtain_start (newcave);

      if (!newcave)
	{
	  gint pos;

	  pos= gnome_score_log (player->score, NULL, TRUE);

	update_score_state ();

	  gnome_app_flash (GNOME_APP (app), _("Congratulations, you win!"));
          show_scores_dialog (pos);

	}
      else
	{
	  if (newcave->is_intermission) player_inc_lives (player, 1);
	}

    }
  return FALSE;
}

/* Start a countdown.  

   'cavename' determines, what happens after the countdown.
   Setting 'cavename' to NULL means, the title image will be shown
   after the countdown.  Setting 'cavename' to a non NULL value, lets
   the program load a new cave and start it.  */

static void
countdown_start (GStonesCave *cave)
{
  /* When showing the countdown, there is no iteration in the
     background anymore.  */
  if (iteration_timeout) g_source_remove (iteration_timeout);
  iteration_timeout=0;

  state    = STATE_COUNTDOWN;
  
  countdown_timeout= 
    g_timeout_add (COUNTDOWN_DELAY, 
		   (GSourceFunc) countdown_timeout_function, 
		   (gpointer) cave);
}



/*****************************************************************************/

/* This widget will be used when querying the joystick device.  */
static GtkWidget *joystick_widget   = NULL;

/* The joystick should only be queried, if 'joystick_widget' has
   focus.  This variable is used to store the focus information.  */
static gboolean   joystick_has_focus= FALSE;


void
joystick_set_properties (guint32 deviceid, gfloat switch_level)
{
  joystick_deviceid= deviceid;
#if 0
  if (joystick_deviceid != GDK_CORE_POINTER)
    gdk_input_set_mode (joystick_deviceid, GDK_MODE_SCREEN);
#endif
  joystick_switch_level= switch_level;
}


static void
joystick_get_information (gint *x_direction, gint *y_direction)
{
  /* FIXME: This function should only return a joystick movement, if
     the game_widget has focus.  */
#if 0
  if (joystick_deviceid != GDK_CORE_POINTER && joystick_has_focus)
    {
      gdouble x;
      gdouble y;
      
      gint xi;
      gint yi;
      
      gint xdir= 0;
      gint ydir= 0;
      
      GdkModifierType mask;
      
      gdk_input_window_get_pointer (GTK_WIDGET (joystick_widget)->window, 
				    joystick_deviceid, 
				    &x, &y, NULL, NULL, NULL, &mask);
      gdk_window_get_origin (GTK_WIDGET (joystick_widget)->window, &xi, &yi);
      
      /* Unfortunatly there is now way, to get the joystick values
         directly.  So we have to revert the calculation that GDK does
         here.  Propably one should extend GDK with an input mode like
         GDK_MODE_RAW.  */

      x= ((gdouble) xi+x)/gdk_screen_width ()*100.0;
      y= ((gdouble) yi+y)/gdk_screen_height ()*100.0;
      
      if (x > joystick_switch_level)
	xdir= 1;
      else if (x < -joystick_switch_level)
	xdir= -1;
      else if (y > joystick_switch_level)
	ydir= 1;
      else if (y < -joystick_switch_level)
	ydir= -1;
      
      if (x_direction) *x_direction= xdir;
      if (y_direction) *y_direction= ydir;
    }
#endif
}


static gint
joystick_focus_change_event (GtkWidget     *widget,
			     GdkEventFocus *event,
			     gpointer       client_data)
{
  joystick_has_focus= (event->in != 0);
  
  return TRUE;
}

static void
joystick_set_widget (GtkWidget *widget)
{
  g_return_if_fail (widget);

  gtk_widget_set_extension_events (widget, GDK_EXTENSION_EVENTS_ALL);
  
  gtk_widget_set_events (widget, gtk_widget_get_events (widget) | 
			 GDK_FOCUS_CHANGE);
  g_signal_connect (GTK_OBJECT (widget), "focus_in_event",
                    (GtkSignalFunc) joystick_focus_change_event, 0);
  g_signal_connect (GTK_OBJECT (widget), "focus_out_event",
                    (GtkSignalFunc) joystick_focus_change_event, 0);

  joystick_widget= widget;
}



/*****************************************************************************/


CaveFlags flags= 0;

static gint
iteration_timeout_function (gpointer data)
{
  GStonesCave *cave  = (GStonesCave *)data;
  gint         retval= TRUE;

  if (state == STATE_TITLE)
    return TRUE;

  joystick_get_information (&player_x_direction, &player_y_direction);
  
  cave_iterate (cave, player_x_direction, player_y_direction, player_push);

  player_x_direction= 0;
  player_y_direction= 0;
  
  if (!(flags & CAVE_FINISHED) && (cave->flags & CAVE_FINISHED))
    {
      
      if (!(cave->flags & CAVE_PLAYER_EXISTS) )
	{
          player_inc_lives (player, -1);
	  /* No score for extra time */
	  cave->timer=0;
	}

      if (!(cave->flags & CAVE_PLAYER_EXISTS) && !cave->is_intermission)
	{
	  
	  if (player->lives != 0)
	    { 
	      /* Restart cave.  */
	      GStonesCave *newcave= gstones_cave_load (game, cave->name);
	      
	      curtain_start (newcave);
	    }
	  else
	    {
	      /* That's it.  No lives anymore.  */
	      gint pos= gnome_score_log (player->score, NULL, TRUE);

		update_score_state ();

	      /* FIXME: somewhere is a bug, that makes this needed.  */
	      if (pos > 10) pos= 0;
	      
	      gnome_app_flash (GNOME_APP (app), _("Game over!"));
              show_scores_dialog (pos);
	      curtain_start (NULL);
	    }
	}
      else
	countdown_start (cave);
      
      retval= TRUE;
      
    }

  /* Manage scrolling.  */
  view_calculate_offset (GSTONES_VIEW (gstones_view), cave);

#ifdef USE_GNOME_CANVAS  
  game_update_image (gstones_view);
#else
  gtk_widget_queue_draw_area (gstones_view, 0, 0,
                              GAME_COLS * STONE_SIZE,
                              GAME_ROWS * STONE_SIZE);
#endif
  flags= cave->flags;

  return retval;
}


static void
iteration_start (GStonesCave *newcave)
{
  /* FIXME: bad hack.  */
  cave = newcave;
  flags= 0;

  cave_set_player (cave, player);
  status_set_cave (cave->name);
  
  iteration_timeout= 
    g_timeout_add (game->frame_rate, iteration_timeout_function, cave);
}



/****************************************************************************/
/* Additional game stuff.  */

gboolean
load_game (const gchar *filename, guint _start_cave)
{
  gchar *basename;
  char buffer[256];
  GStonesGame *newgame;
  gint lastcave;

  if (!filename)
    return FALSE;

  /* Zero length strings cause interesting problems later, so catch them now. */
  if (*filename == '\0')
    return FALSE;
  
  basename = g_path_get_basename (filename);

  /* We don't need to load a game twice.  */
  if (game && (g_utf8_collate (game->filename, filename) == 0))
    return TRUE;

  /* Load game.  */
  newgame= gstones_game_load (filename);

  if (newgame)
    {
      /* Remove all running timeouts.  */
      if (iteration_timeout) g_source_remove (iteration_timeout);
      iteration_timeout=0;
      if (countdown_timeout) g_source_remove (countdown_timeout);
      countdown_timeout=0;

      /* Set display into title mode.  */
      view_display_image (GSTONES_VIEW (gstones_view), title_image);
      view_set_curtain_mode (GSTONES_VIEW (gstones_view), 
			     VIEW_CURTAIN_OPEN, NULL);

      /* Set program state into title state.  */
      state= STATE_TITLE;

      /* Remove any existing game and cave.  */
      if (cave) cave_free (cave);
      if (game) game_free (game);

      /* Set the global variables, concerning the game to some default
         values.  We don't need to remove 'player'.  This will be done
         automatically, when starting a game.  */
      cave      = NULL;
      game      = newgame;

      lastcave = g_list_length (game->start_caves) - 1;
      start_cave= CLAMP (_start_cave, 0, lastcave);

      /* Update the title image.  */
      game_update_title ();
      
      /* Give feedback in statusbar.  */
      sprintf (buffer, "Game '%s' loaded.", basename);

      gnome_app_flash (GNOME_APP (app), buffer);
    } else
      return FALSE;
  
  g_free (basename);
  return TRUE;
}


static void
show_scores_dialog (gint pos)
{
  GtkWidget *dialog;

  dialog = gnome_scores_display (_("GNOME Stones"), APP_NAME, NULL, pos);
  if (dialog != NULL) {
    gtk_window_set_transient_for (GTK_WINDOW(dialog), GTK_WINDOW(app));
    gtk_window_set_modal (GTK_WINDOW(dialog), TRUE);
  }
}


/*****************************************************************************/
/* Menu callback functions */


static void
game_start_cb (GtkWidget *widget, gpointer data)
{
  GList       *tmp = NULL;
  GStonesCave *cave= NULL;
  
  stop_title_music();

  if (!game) return;
  
  /* Initialize the player information.  */
  if (player) player_free (player);
  player= player_new (game);
  if (!player) return;

  tmp = g_list_nth (game->start_caves, start_cave);
  cave= gstones_cave_load (game, tmp->data);

  if (cave)
    curtain_start (cave);
}


static void
game_stop_cb (GtkWidget *widget, gpointer data)
{
    curtain_start (NULL);
}


static void
game_pause_cb (GtkWidget *widget, gpointer data)
{
  if ((state == STATE_RUNNING) && cave)
    {
      cave_toggle_pause_mode (cave);
      
      if (cave->flags & CAVE_PAUSING)
	    gnome_appbar_set_status (GNOME_APPBAR (statusbar), _("Pause"));
      else
	gnome_appbar_refresh (GNOME_APPBAR (statusbar));
    }
}

static void
level_restart_cb (GtkWidget * widget, gpointer date)
{
  cave_player_die (cave);
}

static void
preferences_cb (GtkWidget *widget, gpointer data)
{
  preferences_dialog_show ();
}


void
quit_cb (GtkWidget *widget, gpointer data)
{
  preferences_save_global ();
  sound_close();

  gtk_main_quit ();
}


static void
show_scores_cb (GtkWidget *widget, gpointer data)
{
  show_scores_dialog (GPOINTER_TO_INT (data));
}


static void
about_cb (GtkWidget *widget, gpointer data)
{
  static GtkWidget *about= NULL;
  GdkPixbuf *pixbuf = NULL;
  
  const gchar *authors[]= {
    N_("Main game:"),
    "Carsten Schaar <nhadcasc@fs-maphy.uni-hannover.de>",
    "",
    N_("Sokoban levels:"),
    "Herman Mansilla",
    NULL
  };
  gchar *documenters[] = {
                          NULL
                         };
  /* Translator credits */
  gchar *translator_credits = _("translator-credits");

  if (about != NULL) {
    gtk_window_present (GTK_WINDOW(about));
    return;
  }
  {
	  char *filename = NULL;

	  filename = gnome_program_locate_file (NULL,
			  GNOME_FILE_DOMAIN_APP_PIXMAP,  ("gnome-stones.png"),
			  TRUE, NULL);
	  if (filename != NULL)
	  {
		  pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
		  g_free (filename);
	  }
  }
  about= gnome_about_new (_("GNOME Stones"), VERSION,
			  "Copyright \xc2\xa9 1998-2004 Carsten Schaar",
			  _("Mine through dirt and collect gems."),
			  (const char **)authors,
                          (const char **)documenters,
                          strcmp (translator_credits, "translator-credits") != 0 ? translator_credits : NULL,
			  pixbuf);
  
  if (pixbuf != NULL)
		gdk_pixbuf_unref (pixbuf);
  
  gtk_window_set_transient_for (GTK_WINDOW (about), GTK_WINDOW (app));
  g_signal_connect (G_OBJECT (about), "destroy", G_CALLBACK (gtk_widget_destroyed), &about);
  gtk_widget_show (about);
}



/*****************************************************************************/
/* Menu definitions */


static GnomeUIInfo game_menu[]= {
  GNOMEUIINFO_MENU_NEW_GAME_ITEM(game_start_cb, NULL),
  GNOMEUIINFO_MENU_END_GAME_ITEM(game_stop_cb, NULL),
  GNOMEUIINFO_ITEM_STOCK(N_("_Restart level"), N_("Restart the current level"), level_restart_cb, GTK_STOCK_REFRESH),
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_MENU_PAUSE_GAME_ITEM (game_pause_cb, NULL),
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_MENU_SCORES_ITEM(show_scores_cb, NULL),
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_MENU_QUIT_ITEM(quit_cb, NULL),
  GNOMEUIINFO_END
};

static GnomeUIInfo help_menu[]= {
  GNOMEUIINFO_HELP(APP_NAME),
  GNOMEUIINFO_MENU_ABOUT_ITEM(about_cb, NULL),
  GNOMEUIINFO_END
};

static GnomeUIInfo settings_menu[]= {
  GNOMEUIINFO_MENU_PREFERENCES_ITEM(preferences_cb, NULL),
  GNOMEUIINFO_END
};

static GnomeUIInfo main_menu[]= 
{
  GNOMEUIINFO_MENU_GAME_TREE(game_menu),
  GNOMEUIINFO_MENU_SETTINGS_TREE(settings_menu),
  GNOMEUIINFO_MENU_HELP_TREE(help_menu),
  GNOMEUIINFO_END
};


/*****************************************************************************/
/* Menu sensitivity code. */
static void menu_set_sensitive (gboolean state)
{
  gtk_widget_set_sensitive (game_menu[1].widget, state);
  gtk_widget_set_sensitive (game_menu[2].widget, state);
  gtk_widget_set_sensitive (game_menu[4].widget, state);
}


/*****************************************************************************/
/* Startup sequence.  */


static gboolean
scan_public_game_directory (void)
{
  game_directory_scan (CAVESDIR);

  return TRUE;
}


static gboolean
scan_private_game_directory (void)
{
  char *dir= gnome_util_home_file ("gnomes-stones/games");
  
  game_directory_scan (dir);

  g_free (dir);
  return TRUE;
}


static gboolean
scan_public_plugin_directory (void)
{
  char *dir= gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_APP_LIBDIR, 
                                        "gnome-stones/objects/", FALSE, NULL);

  plugin_load_plugins_in_dir (dir);

  g_free (dir);
  return TRUE;
}


static gboolean
scan_private_plugin_directory (void)
{
  char *dir= gnome_util_home_file ("gnomes-stones/objects");
  
  plugin_load_plugins_in_dir (dir);

  g_free (dir);
  return TRUE;
}


/*****************************************************************************/
/* Startup sequence.  */


typedef struct
{
  gboolean (*function)(void);
  char     *description;
} StartupSequence;


StartupSequence startup_sequence[]=
{
  {
    scan_private_plugin_directory,
    N_("Scanning private object directory...")
  },
  {
    scan_public_plugin_directory,
    N_("Scanning public object directory...")
  },
#ifdef USE_GNOME_CANVAS
  {
    game_widget_fill,
    N_("Filling game widget...")
  },
#endif /* USE_GNOME_CANVAS */
  {
    scan_private_game_directory,
    N_("Scanning private game directory...")
  },
  {
    scan_public_game_directory,
    N_("Scanning public game directory...")
  },
  {
    preferences_restore,
    N_("Restoring preferences...")
  }


};


static gint startup_function (gpointer data)
{
  static guint index= 0;
  static guint max  = 0;
  
  if (max == 0)
    max= sizeof (startup_sequence)/sizeof (startup_sequence[0]);

  gnome_appbar_set_status (GNOME_APPBAR (statusbar), 
			   _(startup_sequence[index].description));
  
  startup_sequence[index].function ();
  
  gnome_appbar_set_progress_percentage (GNOME_APPBAR (statusbar),
			     (gfloat) (index+1)/(gfloat) max);
  index++;

  if (index == max)
    {
      state= STATE_TITLE;
      gnome_appbar_refresh (GNOME_APPBAR (statusbar));
    }
  
  return (index != max);
}


/*****************************************************************************/
/* Main function.  */

GConfClient *
get_gconf_client ()
{
  if (!gconf_client) 
    gconf_client = gconf_client_get_default ();
  return gconf_client;
}

int
main (int argc, char *argv[])
{

  GtkWidget *frame= NULL;
  GtkWidget *vbox = NULL;
  GtkWidget *table= NULL;
  
  gnome_score_init (APP_NAME);

  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  gnome_program_init (APP_NAME, VERSION,
      		      LIBGNOMEUI_MODULE, 
       		      argc, argv,
       		      GNOME_PARAM_POPT_TABLE, options,
       		      GNOME_PARAM_APP_DATADIR, DATADIR,
		      GNOME_PARAM_APP_LIBDIR, LIBDIR, NULL);

  gconf_init (argc, argv, NULL);
  gconf_client = get_gconf_client ();
  gconf_client_add_dir (gconf_client, "/apps/gnome-stones",
                        GCONF_CLIENT_PRELOAD_NONE, NULL);

  gnome_window_icon_set_default_from_file (GNOME_ICONDIR"/gnome-stones.png");
  /* That's what a gnome application needs:  */
  app = gnome_app_new ("gnome-stones", _("GNOME Stones"));
  gtk_window_set_resizable  (GTK_WINDOW (app), FALSE);

  /* ... a menu line, ... */
  gnome_app_create_menus (GNOME_APP (app), main_menu);
  menu_set_sensitive (FALSE);
  
  /* ... a toolbar, ... */
  /*  gnome_app_create_toolbar (GNOME_APP (app), toolbar); */

  /* ... a statusline ... */
  statusbar= gnome_appbar_new (TRUE, TRUE, GNOME_PREFERENCES_USER);
  gnome_app_set_statusbar (GNOME_APP (app), statusbar);

  gnome_appbar_set_default (GNOME_APPBAR (statusbar), _(default_message));

  gnome_app_install_menu_hints(GNOME_APP (app), main_menu);

  /* and, last but not least the game display.  */
  vbox = gtk_vbox_new (FALSE, 0);

  gstones_view= view_new (load_image_from_path ("gnome-stones/curtain.png"));

  frame= gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_widget_show (frame);
  gtk_container_add (GTK_CONTAINER (frame), gstones_view);

  gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);

  table= status_widget_get ();
  gtk_widget_show (table);
  gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 0);
  
  gnome_app_set_contents (GNOME_APP (app), vbox);
  
  gtk_widget_set_events (app, gtk_widget_get_events (app) | GAME_EVENTS);

 /* are there any high scores? */

  update_score_state ();

  g_signal_connect (GTK_OBJECT (app), "key_press_event",
		      GTK_SIGNAL_FUNC (game_widget_key_press_callback), 0);
  g_signal_connect (GTK_OBJECT (app), "key_release_event",
		      GTK_SIGNAL_FUNC (game_widget_key_release_callback), 0);
  g_signal_connect (GTK_OBJECT (app), "delete_event",
		      GTK_SIGNAL_FUNC (quit_cb), 0);

  joystick_set_widget (app);

  gtk_widget_show (app);

  title_image_load ();

  view_display_image (GSTONES_VIEW (gstones_view), title_image);

  session_management_init ();

  g_idle_add (startup_function, NULL);

  sound_init ();

  if (pref_get_sound_enabled ())
    play_title_music();

  gtk_main ();

  return 0;
}
