/*
 * gataxx.c - main gui code for gataxx
 * Written by Chris Rogers (gandalf@pobox.com)
 * Based on iagno code written by Ian Peters (itp@gnu.org)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * For more details see the file COPYING.
 */

#include "config.h"
#include <gnome.h>
#include <libgnomeui/gnome-window-icon.h>
#include <gdk/gdkkeysyms.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <sys/time.h>
#include <string.h>
#include <games-clock.h>
#include <gconf/gconf-client.h>
#include <games-gconf.h>

#include "gataxx.h"
#include "ataxx.h"
#include "properties.h"

GdkPixmap *buffer_pixmap = NULL;
GdkPixmap *tiles_pixmap = NULL;
GdkPixmap *tiles_mask = NULL;

GnomeAppBar *appbar;
GtkWidget *window;
GtkWidget *drawing_area;
GtkWidget *tile_dialog;
GtkWidget *black_score;
GtkWidget *white_score;
GtkWidget *time_display;

GConfClient *gataxx_gconf_client = NULL;

gint flip_pixmaps_id = 0;
gint statusbar_id;
guint whose_turn = BLACK_TURN;
guint game_in_progress = 0;
guint black_computer_level;
guint white_computer_level;
guint black_computer_id = 0;
guint white_computer_id = 0;
guint computer_speed = COMPUTER_MOVE_DELAY;
gint animate;
guint tiles_to_flip = 0;

gint64 milliseconds_total = 0;
gint64 milliseconds_current_start = 0;

gint timer_valid = 0;

gint bcount;
gint wcount;

gint8 pixmaps[7][7] = {{0,0,0,0,0,0,0},
		       {0,0,0,0,0,0,0},
		       {0,0,0,0,0,0,0},
		       {0,0,0,0,0,0,0},
		       {0,0,0,0,0,0,0},
		       {0,0,0,0,0,0,0},
		       {0,0,0,0,0,0,0}};

gint8 board[7][7] = {{0,0,0,0,0,0,0},
		     {0,0,0,0,0,0,0},
		     {0,0,0,0,0,0,0},
		     {0,0,0,0,0,0,0},
		     {0,0,0,0,0,0,0},
		     {0,0,0,0,0,0,0},
		     {0,0,0,0,0,0,0}};


MoveHistory * game = NULL;

extern guint flip_final_id;

int session_flag = 0;
int session_xpos = -1;
int session_ypos = -1;
int session_position = 0;

gchar *tile_set = NULL;

guint selected_x;
guint selected_y;
guint selected_piece;

static const struct poptOption options[] = {
  {NULL, 'x', POPT_ARG_INT, &session_xpos, 0, NULL, NULL},
  {NULL, 'y', POPT_ARG_INT, &session_ypos, 0, NULL, NULL},
  /*
    #ifdef HAVE_ORBIT
    {"ior", '\0', POPT_ARG_STRING, &ior, 0, N_("IOR of remote Iagno server"),
    N_("IOR")},
    #endif
  */
  {NULL, '\0', 0, NULL, 0}
};

GnomeUIInfo game_menu[] = {
        GNOMEUIINFO_MENU_NEW_GAME_ITEM(new_game_cb, NULL),

	GNOMEUIINFO_SEPARATOR,

	GNOMEUIINFO_MENU_UNDO_MOVE_ITEM(undo_move_cb, NULL),
	
	GNOMEUIINFO_SEPARATOR,

        GNOMEUIINFO_MENU_QUIT_ITEM(quit_game_cb, NULL),

	GNOMEUIINFO_END
};

/*
GnomeUIInfo black_level_radio_list[] = {
	{ GNOME_APP_UI_ITEM, N_("_Disabled"),
	  N_("Disable the computer player"),
	  black_level_cb, 0, NULL, GNOME_APP_PIXMAP_DATA, NULL, 0, 0, NULL },

	{ GNOME_APP_UI_ITEM, N_("Level _One"),
	  N_("Enable the level 1 computer player"),
	  black_level_cb, (gpointer) 1, NULL, GNOME_APP_PIXMAP_DATA, NULL, 0,
	  0, NULL },

	{ GNOME_APP_UI_ITEM, N_("Level _Two"),
	  N_("Enable the level 2 computer player"),
	  black_level_cb, (gpointer) 2, NULL, GNOME_APP_PIXMAP_DATA, NULL, 0,
	  0, NULL },

	{ GNOME_APP_UI_ITEM, N_("Level Th_ree"),
	  N_("Enable the level 3 computer player"),
	  black_level_cb, (gpointer) 3, NULL, GNOME_APP_PIXMAP_DATA, NULL, 0,
	  0, NULL },

	GNOMEUIINFO_END
};

GnomeUIInfo white_level_radio_list[] = {
	{ GNOME_APP_UI_ITEM, N_("_Disabled"),
	  N_("Disable the computer player"),
	  white_level_cb, (gpointer) 0, NULL, GNOME_APP_PIXMAP_DATA, NULL, 0, 0, NULL },

	{ GNOME_APP_UI_ITEM, N_("Level _One"),
	  N_("Enable the level 1 computer player"),
	  white_level_cb, (gpointer) 1, NULL, GNOME_APP_PIXMAP_DATA, NULL, 0, 0, NULL },

	{ GNOME_APP_UI_ITEM, N_("Level _Two"),
	  N_("Enable the level 2 computer player"),
	  white_level_cb, (gpointer) 2, NULL, GNOME_APP_PIXMAP_DATA, NULL, 0, 0, NULL },

	{ GNOME_APP_UI_ITEM, N_("Level Th_ree"),
	  N_("Enable the level 3 computer player"),
	  white_level_cb, (gpointer) 3, NULL, GNOME_APP_PIXMAP_DATA, NULL, 0, 0, NULL },
	GNOMEUIINFO_END
};

GnomeUIInfo black_level_menu[] = {
	GNOMEUIINFO_RADIOLIST(black_level_radio_list),
	GNOMEUIINFO_END
};

GnomeUIInfo white_level_menu[] = {
	GNOMEUIINFO_RADIOLIST(white_level_radio_list),
	GNOMEUIINFO_END
};

GnomeUIInfo comp_menu[] = {
};
*/

/*
GnomeUIInfo settings_computer_submenu[] = {
        GNOMEUIINFO_SUBTREE_HINT(N_("_Dark"),
				 N_("Configure the dark computer player"),
				 black_level_menu),
        GNOMEUIINFO_SUBTREE_HINT(N_("_Light"), 
				 N_("Configure the light computer player"),
				 white_level_menu),

	GNOMEUIINFO_SEPARATOR,

	GNOMEUIINFO_TOGGLEITEM(N_("_Quick moves"),
			       N_("Turn on quick computer moves"),
			       quick_moves_cb, NULL),
	GNOMEUIINFO_END
};
*/

GnomeUIInfo settings_menu[] = {
	GNOMEUIINFO_MENU_PREFERENCES_ITEM (properties_cb, NULL),
        GNOMEUIINFO_END
};

GnomeUIInfo help_menu[] = {
        GNOMEUIINFO_HELP("gataxx"),
	GNOMEUIINFO_MENU_ABOUT_ITEM(about_cb, NULL),
	GNOMEUIINFO_END
};

GnomeUIInfo mainmenu[] = {
        GNOMEUIINFO_MENU_GAME_TREE(game_menu),
        GNOMEUIINFO_MENU_SETTINGS_TREE(settings_menu),
        GNOMEUIINFO_MENU_HELP_TREE(help_menu),
	GNOMEUIINFO_END
};

static void undo_set_sensitive (gboolean state)
{
  gtk_widget_set_sensitive (game_menu[2].widget, state);
}

gboolean
quit_game_cb (GtkWidget *widget, gpointer data)
{
  if (flip_pixmaps_id) {
    g_source_remove (flip_pixmaps_id);
    flip_pixmaps_id = 0;
  }
  if (black_computer_id) {
    g_source_remove (black_computer_id);
    black_computer_id= 0;
  }
  if (white_computer_id) {
    g_source_remove (white_computer_id);
    white_computer_id = 0;
  }

  if (buffer_pixmap)
    g_object_unref (buffer_pixmap);
  if (tiles_pixmap)
    g_object_unref (tiles_pixmap);
  if (tiles_mask)
    g_object_unref (tiles_mask);

  gtk_main_quit ();

  return FALSE;
}

void
new_game_cb (GtkWidget *widget, gpointer data)
{
  init_new_game ();
}

void
undo_move_cb (GtkWidget *widget, gpointer data)
{

  gint8 which_computer;
  gint i, j;
  MoveHistory * temp;
  
  if ((black_computer_level && white_computer_level) || !game || !game->prev)
    return;
  
  if (flip_final_id) {
    g_source_remove (flip_final_id);
    flip_final_id = 0;
  }
  
  game_in_progress = 1;

  temp = game;
  game = game->prev;
  g_free (temp);
  
  if (black_computer_level || white_computer_level) {
    if (black_computer_level)
      which_computer = BLACK_TURN;
    else
      which_computer = WHITE_TURN;
    while (game->prev && game->me == which_computer) {
      pixmaps[game->x][game->y] = 100;
      temp = game;
      game = game->prev;
      g_free (temp);
    }
  }

  pixmaps[game->x][game->y] = 100;  
  memcpy (board, game->board, sizeof (gint8) * 7 * 7);
  
  whose_turn = game->me;
  
  if (whose_turn == WHITE_TURN)
    gui_message (_("Light's move"));
  else
    gui_message (_("Dark's move"));
  
  wcount = 0;
  bcount = 0;
  
  for (i = 0; i < 7; i++)
    for (j = 0; j < 7; j++) {
      if (board[i][j] == WHITE_TURN)
	wcount++;
      if (board[i][j] == BLACK_TURN)
	bcount++;
    }
  
  gui_status ();
  
  if (timer_valid) {
    games_clock_stop (GAMES_CLOCK (time_display));
    gtk_widget_set_sensitive (time_display, FALSE);
    games_clock_set_seconds (GAMES_CLOCK (time_display), 0);
    timer_valid = 0;
  }
  
  tiles_to_flip = 1;

  /* Cancel impending computer moves. */
  if (black_computer_id) {
    g_source_remove (black_computer_id);
    black_computer_id = 0;
  }
  if (white_computer_id) {
    g_source_remove (white_computer_id);
    white_computer_id = 0;
  }
  
  check_computer_players ();
}

void
black_level_cb (GtkWidget *widget, gpointer data)
{
  int tmp;
  
  tmp = atoi ((gchar *)data);
  
  gconf_client_set_int (gataxx_gconf_client, "/apps/gataxx/blacklevel",
                        tmp, NULL);
  
  black_computer_level = tmp;
  
  if (game_in_progress) {
    games_clock_stop (GAMES_CLOCK (time_display));
    gtk_widget_set_sensitive (time_display, FALSE);
    games_clock_set_seconds (GAMES_CLOCK (time_display), 0);
    timer_valid = 0;
  }

  check_computer_players();
}

void
white_level_cb(GtkWidget *widget, gpointer data)
{
  int tmp;
  
  tmp = atoi ((gchar *)data);
  
  gconf_client_set_int (gataxx_gconf_client, "/apps/gataxx/whitelevel",
                       tmp, NULL);
  
  white_computer_level = tmp;
  
  if (game_in_progress) {
    games_clock_stop(GAMES_CLOCK(time_display));
    gtk_widget_set_sensitive(time_display, FALSE);
    games_clock_set_seconds(GAMES_CLOCK(time_display), 0);
    timer_valid = 0;
  }

  check_computer_players();
}

void
about_cb(GtkWidget *widget, gpointer data)
{

  static GtkWidget *about;
  GdkPixbuf *pixbuf = NULL;
  const gchar *authors[] = {"Chris Rogers", NULL};
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
			  GNOME_FILE_DOMAIN_APP_PIXMAP,  ("gataxx.png"),
			  TRUE, NULL);
	  if (filename != NULL)
	  {
		  pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
		  g_free (filename);
	  }
  }
  
  about = gnome_about_new(_("gataxx"), VERSION,
			  "Copyright \xc2\xa9 1999-2004 Chris Rogers",
  			  _("gataxx is a GNOME port of the old game ataxx. "
			    "It is derived from Ian Peters' iagno code."),
  			  (const char **)authors,
  			  (const char **)documenters,
  			  strcmp (translator_credits, "translator-credits") != 0 ? translator_credits : NULL,
			  pixbuf);
  if (pixbuf != NULL)
		gdk_pixbuf_unref (pixbuf);
		
  g_signal_connect (G_OBJECT (about), "destroy", G_CALLBACK
		      (gtk_widget_destroyed), &about);
  gtk_window_set_transient_for (GTK_WINDOW (about), GTK_WINDOW(window));
  
  gtk_widget_show(about);
}

void
properties_cb (GtkWidget *widget, gpointer data)
{
  show_properties_dialog ();
}

gint
expose_event (GtkWidget *widget, GdkEventExpose *event)
{

  int i;

  gdk_draw_drawable (widget->window,
                     widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
                     buffer_pixmap, event->area.x, event->area.y,
                     event->area.x, event->area.y,
                     event->area.width, event->area.height);
  
 
 for (i = 0; i < 7; i++) {
   gdk_draw_line (widget->window, widget->style->black_gc,
                  i * TILEWIDTH, 0, i * TILEWIDTH, 7 * TILEHEIGHT);
   gdk_draw_line (widget->window, widget->style->black_gc,
                  0, (i * TILEHEIGHT) , 7 * TILEWIDTH, (i * TILEHEIGHT));
  }

  return FALSE;
}

static gint
configure_event (GtkWidget *widget, GdkEventConfigure *event)
{

  guint i, j;
  
  if (buffer_pixmap)
    g_object_unref (buffer_pixmap);
  buffer_pixmap = gdk_pixmap_new (widget->window,
                                  widget->allocation.width,
                                  widget->allocation.height, -1);
  
  for (i = 0; i < 7; i++)
    for (j = 0; j < 7; j++)
      gui_draw_pixmap_buffer (pixmaps[i][j], i, j);

  return TRUE;
}

static gint
button_press_event (GtkWidget *widget, GdkEventButton *event)
{
  guint x, y;
  
  /*  if (!network_allow ())
      return (TRUE);*/
  
  if ((whose_turn == WHITE_TURN) && white_computer_level)
    return TRUE;
  
  if ((whose_turn == BLACK_TURN) && black_computer_level)
    return TRUE;
  
  if (event->button == 1) {
    x = event->x / TILEWIDTH;
    y = event->y / TILEHEIGHT;
    if (is_valid_piece (x, y, whose_turn)) {
      gui_draw_selected (selected_x, selected_y, FALSE);
      selected_x = x;
      selected_y = y;
      selected_piece = TRUE;
      gui_draw_selected (x, y, TRUE);
    }
    else if (selected_piece 
             && is_valid_move (x, y, selected_x, selected_y, whose_turn)) {
      selected_piece = FALSE;
      gui_draw_selected (selected_x, selected_y, FALSE);
      move (x, y, selected_x, selected_y, whose_turn);
    }
  } 
  return TRUE;
}


void
gui_draw_selected (gint x, gint y, gint on)
{

  GdkGC * gc;
  GdkGC * sgc;
  static GdkColor newc = { 12345, 30208, 41216, 33792};
	int test_x=0;
  int test_y=0;
	
	sgc = gdk_gc_new (window->window);
	gdk_gc_set_foreground (sgc, &newc);
	gdk_gc_set_background (sgc, &newc);

  if (on) 
    gc = window->style->white_gc;
  else 
    gc = window->style->black_gc;
	
  for (test_x=0; test_x<7; test_x++) {
    for (test_y=0; test_y<7; test_y++) {
      if (is_valid_move (test_x, test_y, x, y, on))
        gdk_draw_rectangle (drawing_area->window, gc, FALSE,
                            test_x * TILEWIDTH, test_y * TILEHEIGHT,
                            TILEWIDTH, TILEHEIGHT);
    }
  }
  if (on) 
    gdk_draw_rectangle (drawing_area->window, sgc, FALSE,
                        x * TILEWIDTH, y * TILEHEIGHT,
                        TILEWIDTH, TILEHEIGHT);
  else 
    gdk_draw_rectangle (drawing_area->window, gc, FALSE,
                        x * TILEWIDTH, y * TILEHEIGHT,
                        TILEWIDTH, TILEHEIGHT);
  
  g_object_unref (sgc);
}

void
gui_draw_pixmap (gint which, gint x, gint y)
{

  gdk_draw_drawable (drawing_area->window,
                     drawing_area->style->fg_gc[GTK_WIDGET_STATE (drawing_area)],
                     tiles_pixmap,
                     (which % 8) * TILEWIDTH, (which / 8) * TILEHEIGHT,
                     x * TILEWIDTH, y * TILEHEIGHT,
                     TILEWIDTH, TILEHEIGHT);

  gdk_draw_drawable (buffer_pixmap,
                     drawing_area->style->fg_gc[GTK_WIDGET_STATE (drawing_area)],
                     tiles_pixmap, (which % 8) * TILEWIDTH,
                     (which / 8) * TILEHEIGHT, x * TILEWIDTH,
                     y * TILEHEIGHT, TILEWIDTH, TILEHEIGHT);

  gdk_draw_line (drawing_area->window, window->style->black_gc,
                 x * TILEWIDTH, y * TILEHEIGHT,
                 (x) * TILEWIDTH, (y+1) * TILEHEIGHT);

  gdk_draw_line (drawing_area->window, window->style->black_gc,
                 x * TILEWIDTH, y * TILEHEIGHT,
                 (x+1) * TILEWIDTH, (y) * TILEHEIGHT);
  
  gdk_draw_line (drawing_area->window, window->style->black_gc,
                 (x+1) * TILEWIDTH, y * TILEHEIGHT,
                 (x+1) * TILEWIDTH, (y+1) * TILEHEIGHT);
  gdk_draw_line (drawing_area->window, window->style->black_gc,
                 x * TILEWIDTH, (y+1) * TILEHEIGHT,
                 (x+1) * TILEWIDTH, (y+1) * TILEHEIGHT);
}

void
gui_draw_pixmap_buffer (gint which, gint x, gint y)
{
  gdk_draw_drawable (buffer_pixmap,
                     drawing_area->style->fg_gc[GTK_WIDGET_STATE(drawing_area)],
                     tiles_pixmap,
                     (which % 8) * TILEWIDTH, (which / 8) * TILEHEIGHT,
                     x * TILEWIDTH, y * TILEHEIGHT, TILEWIDTH, TILEHEIGHT);
}

void
load_pixmaps (void)
{
  gchar *tmp;
  gchar *fname;
  GdkPixbuf *image;

  /* We share pixmaps with iagno. */
  tmp = g_build_filename ("iagno", tile_set, NULL);
  fname = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_APP_PIXMAP,
                                     (tmp), FALSE, NULL);
  g_free (tmp);
  
  if (! g_file_test (fname, G_FILE_TEST_EXISTS)) {
    g_print (_("Could not find \'%s\' pixmap file for gataxx\n"), fname);
    exit (1);
  }

  image = gdk_pixbuf_new_from_file (fname, NULL);
  gdk_pixbuf_render_pixmap_and_mask (image, &tiles_pixmap, &tiles_mask, 127);
  
  gdk_pixbuf_unref (image);
  g_free (fname);
}

gint
flip_pixmaps (gpointer data)
{

  guint i, j;
  guint flipped_tiles = 0;
  
  if (!tiles_to_flip)
    return(TRUE);
  
  for(i = 0; i < 7; i++)
    for(j = 0; j < 7; j++) {
      if (pixmaps[i][j] == 100) {
	pixmaps[i][j] = 101;
	gui_draw_pixmap(0, i, j);
	flipped_tiles = 1;
      } else if (pixmaps[i][j] < board[i][j]) { /* Black -> White */
	if (animate == 0) {
	  if (pixmaps[i][j] == BLACK_TURN)
	    pixmaps[i][j] = board[i][j];
	  else
	    pixmaps[i][j]++;
	} else if (animate == 1) {
          pixmaps[i][j] += 8;
          if (pixmaps[i][j] > 31)
            pixmaps[i][j] = 31;
	} else if (animate == 2)
	  pixmaps[i][j]++;
	if (pixmaps[i][j] > 0)
	  gui_draw_pixmap(pixmaps[i][j], i, j);
	flipped_tiles = 1;
      } else if (pixmaps[i][j] > board[i][j] && pixmaps[i][j] != 101) { /* White -> Black */
	if (animate == 0) {
	  if (pixmaps[i][j] == WHITE_TURN)
	    pixmaps[i][j] = board[i][j];
	  else
	    pixmaps[i][j]--;
	} else if (animate == 1) {
          pixmaps[i][j] -= 8;
          if (pixmaps[i][j] < 1)
            pixmaps[i][j] = 1;
	} else if (animate == 2)
	  pixmaps[i][j]--;
	if (pixmaps[i][j] < 32)
	  gui_draw_pixmap(pixmaps[i][j], i, j);
	flipped_tiles = 1;
      }
    }
  
  if (!flipped_tiles)
    tiles_to_flip = 0;
  
  return(TRUE);
}

void
init_new_game (void)
{
  guint i, j;
  MoveHistory * temp;
  
  if (flip_final_id) {
    g_source_remove (flip_final_id);
    flip_final_id = 0;
  }

  game_in_progress = 1;
  while (game) {
    temp = game;
    game = game->prev;
    g_free (temp);
  }
  game = g_malloc (sizeof(MoveHistory));
  game->prev = NULL;
  undo_set_sensitive (FALSE);
  
  for(i = 0; i < 7; i++)
    for(j = 0; j < 7; j++)
      board[i][j] = 0;
  
  board[0][0] = BLACK_TURN;
  board[0][6] = WHITE_TURN;
  board[6][0] = WHITE_TURN;
  board[6][6] = BLACK_TURN;
  
  bcount = 2;
  wcount = 2;
  
  gui_status ();
  
  memcpy (pixmaps, board, sizeof (gint8) * 7 * 7);
  memcpy (game->board, board, sizeof (gint8) * 7 * 7);

  for(i = 0; i < 7; i++)
    for(j = 0; j < 7; j++)
      gui_draw_pixmap_buffer (pixmaps[i][j], i, j);
  
  gdk_draw_drawable (drawing_area->window,
                     drawing_area->style->fg_gc[GTK_WIDGET_STATE(drawing_area)],
                     buffer_pixmap, 0, 0, 0, 0, BOARDWIDTH, BOARDHEIGHT);
  for (i = 0; i < 7; i++) {
    gdk_draw_line (drawing_area->window,
                   drawing_area->style->black_gc,
                   i * TILEWIDTH, 0, i * TILEWIDTH, 7 * TILEHEIGHT);
    gdk_draw_line (drawing_area->window,
                   drawing_area->style->black_gc,
                   0, (i * TILEHEIGHT) , 7 * TILEWIDTH, (i * TILEHEIGHT));
  }

  whose_turn = BLACK_TURN;
  gui_message (_("Dark's move"));
  
  games_clock_stop (GAMES_CLOCK (time_display));
  games_clock_set_seconds (GAMES_CLOCK (time_display), 0);
  
  if (black_computer_level ^ white_computer_level) {
    if (! black_computer_level)
      games_clock_start (GAMES_CLOCK (time_display));
    gtk_widget_set_sensitive (time_display, TRUE);
    timer_valid = 1;
  } else {
    gtk_widget_set_sensitive (time_display, FALSE);
    timer_valid = 0;
  }

  check_computer_players ();
}

void
create_window (void)
{

  GtkWidget *table;
  
  window = gnome_app_new ("gataxx", _("Ataxx"));
  
  gtk_widget_realize (window);
  gtk_window_set_resizable (GTK_WINDOW(window), FALSE);
  g_signal_connect (G_OBJECT (window), "delete_event",
                    G_CALLBACK (quit_game_cb), NULL);
  
  gnome_app_create_menus (GNOME_APP (window), mainmenu);
  
  drawing_area = gtk_drawing_area_new ();

  gnome_app_set_contents (GNOME_APP (window), drawing_area);

  gtk_widget_set_size_request (GTK_WIDGET (drawing_area),
                               BOARDWIDTH, BOARDHEIGHT);
  g_signal_connect (G_OBJECT (drawing_area), "expose_event",
                    G_CALLBACK (expose_event), NULL);
  g_signal_connect (G_OBJECT (drawing_area), "configure_event",
                    G_CALLBACK (configure_event), NULL);
  g_signal_connect (G_OBJECT (drawing_area), "button_press_event",
                    G_CALLBACK (button_press_event), NULL);
  gtk_widget_set_events (drawing_area,
                         GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK);
  gtk_widget_show (drawing_area);

  appbar = GNOME_APPBAR (gnome_appbar_new (FALSE, TRUE, FALSE));
  gnome_app_set_statusbar (GNOME_APP (window), GTK_WIDGET (appbar));
  gnome_app_install_menu_hints (GNOME_APP (window), mainmenu);

  table = gtk_table_new (1, 8, FALSE);
  
  black_score = gtk_label_new (_("Dark:"));
  gtk_table_attach(GTK_TABLE(table), black_score, 1, 2, 0, 1, 0, 0, 3, 1);

  black_score = gtk_label_new("00");
  gtk_table_attach(GTK_TABLE(table), black_score, 2, 3, 0, 1, 0, 0, 3, 1);
  
  white_score = gtk_label_new(_("Light:"));
  gtk_table_attach(GTK_TABLE(table), white_score, 4, 5, 0, 1, 0, 0, 3, 1);
  
  white_score = gtk_label_new("00");
  gtk_table_attach(GTK_TABLE(table), white_score, 5, 6, 0, 1, 0, 0, 3, 1);
  
  time_display = games_clock_new ();
  gtk_widget_set_sensitive (time_display, FALSE);
  gtk_table_attach (GTK_TABLE (table), time_display, 7, 8, 0, 1, 0, 0, 3, 1);

  undo_set_sensitive (FALSE);
  
  gtk_widget_show_all (table);
  
  gtk_box_pack_start (GTK_BOX (appbar), table, FALSE, TRUE, 0);
  
  gnome_appbar_set_status (GNOME_APPBAR (appbar), _("Welcome to gataxx!"));
}

void
gui_status (void)
{
  gchar message[3];
  
  sprintf (message, _("%.2d"), bcount);
  gtk_label_set_text (GTK_LABEL (black_score), message);
  sprintf (message, _("%.2d"), wcount);
  gtk_label_set_text (GTK_LABEL (white_score), message);
  undo_set_sensitive (game && game->prev);
}

void
gui_message (gchar *message)
{
  gnome_appbar_pop (GNOME_APPBAR (appbar));
  gnome_appbar_push (GNOME_APPBAR (appbar), message);
}

guint
check_computer_players (void)
{
  
  if (black_computer_level && whose_turn == BLACK_TURN) {
    switch (black_computer_level) {
    case 1:
      black_computer_id = g_timeout_add (computer_speed,
                                         (GtkFunction)computer_move_1,
                                         (gpointer) BLACK_TURN);
      break;
    case 2:
      black_computer_id = g_timeout_add (computer_speed,
                                         (GtkFunction)computer_move_2,
                                         (gpointer) BLACK_TURN);
      break;
    case 3:
      black_computer_id = g_timeout_add (computer_speed,
                                         (GtkFunction)computer_move_3,
                                         (gpointer) BLACK_TURN);
      break;
    }
  }
  
  if (white_computer_level && whose_turn == WHITE_TURN) {
    switch (white_computer_level) {
    case 1:
      white_computer_id = g_timeout_add (computer_speed,
                                         (GtkFunction)computer_move_1,
                                         (gpointer) WHITE_TURN);
      break;
    case 2:
      white_computer_id = g_timeout_add (computer_speed,
                                         (GtkFunction)computer_move_2,
                                         (gpointer) WHITE_TURN);
      break;
    case 3:
      white_computer_id = g_timeout_add (computer_speed,
                                         (GtkFunction)computer_move_3,
                                         (gpointer) WHITE_TURN);
      break;
    }
  }
  
  return TRUE;
}

static void
set_bg_color (void)
{
  GdkImage *tmpimage;
  GdkColor bgcolor;
  
  tmpimage = gdk_drawable_get_image (tiles_pixmap, 0, 0, 1, 1);
  bgcolor.pixel = gdk_image_get_pixel (tmpimage, 0, 0);
  gdk_window_set_background (drawing_area->window, &bgcolor);
  g_object_unref (tmpimage);
}

static int 
save_state (GnomeClient *client, gint phase, GnomeRestartStyle save_style,
            gint shutdown, GnomeInteractStyle interact_style, gint fast,
            gpointer client_data)
{
  char *argv[20];
  int i;
  int xpos, ypos;
  
  gdk_window_get_origin (window->window, &xpos, &ypos);
  
  i = 0;
  argv[i++] = (char *)client_data;
  argv[i++] = "-x";
  argv[i++] = g_strdup_printf ("%d",xpos);
  argv[i++] = "-y";
  argv[i++] = g_strdup_printf ("%d",ypos);
  
  gnome_client_set_restart_command (client, i, argv);
  gnome_client_set_clone_command (client, 0, NULL);
  
  g_free (argv[2]);
  g_free (argv[4]);
  
  return TRUE;
}

static void
key_changed_cb (GConfClient *tmp_client, guint cnx_id, GConfEntry *tmp_entry,
                gpointer tmp_data)
{
  reload_properties ();
}

int 
main (int argc, char **argv)
{
  GnomeClient *client;

  /*  CORBA_def (CORBA_Environment ev;) */
  struct timeval tv;
    
  gnome_score_init ("gataxx");
  
  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
    
  gettimeofday (&tv, NULL);
  srand (tv.tv_usec);
    
#ifdef HAVE_ORBIT
  CORBA_exception_init (&ev);
  orb = gnome_CORBA_init_with_popt_table ("gataxx", VERSION,
                                          &argc, argv, options, 0, NULL,
                                          GNORBA_INIT_SERVER_FUNC|GNORBA_INIT_DISABLE_COOKIES,
                                          &ev);
#else
  gnome_program_init ("gataxx", VERSION,
                      LIBGNOMEUI_MODULE, 
                      argc, argv,
                      GNOME_PARAM_POPT_TABLE, options,
                      GNOME_PARAM_APP_DATADIR, DATADIR, NULL);
#endif

  gnome_window_icon_set_default_from_file (GNOME_ICONDIR"/gataxx.png");
  client= gnome_master_client ();
    
  g_signal_connect (G_OBJECT (client), "save_yourself",
                    G_CALLBACK (save_state), argv[0]);
  g_signal_connect (G_OBJECT (client), "die",
                    G_CALLBACK (quit_game_cb), argv[0]);

  gconf_init (argc, argv, NULL);
  gataxx_gconf_client = gconf_client_get_default ();
  if (! games_gconf_sanity_check_string (gataxx_gconf_client,
                                         "/apps/gataxx/tileset")) {
    return 1;
  }
  gconf_client_add_dir (gataxx_gconf_client, "/apps/gataxx",
                        GCONF_CLIENT_PRELOAD_NONE, NULL);
  gconf_client_notify_add (gataxx_gconf_client, "/apps/gataxx",
                           key_changed_cb, NULL, NULL, NULL);

  create_window ();

  load_properties ();
    
  load_pixmaps ();
    
  if (session_xpos >= 0 && session_ypos >= 0) {
    gtk_window_move (GTK_WINDOW (window), session_xpos, session_ypos);
  }
    
  gtk_widget_show (window);
    
  set_bg_color ();
    
  gdk_window_clear_area (drawing_area->window, 0, 0,
                         BOARDWIDTH, BOARDHEIGHT);
    
  /*	network_init (); */
    
  gtk_main ();
    
  g_object_unref (G_OBJECT (gataxx_gconf_client));

  return 0;
}
