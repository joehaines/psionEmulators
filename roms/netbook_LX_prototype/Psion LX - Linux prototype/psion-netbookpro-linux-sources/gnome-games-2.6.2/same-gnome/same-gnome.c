/* -*- mode:C; indent-tabs-mode:t; tab-width:8; c-basic-offset:8; -*- */

/*
 * Same-Gnome: the game.
 * (C) 1997 the Free Software Foundation
 *
 * Author: Miguel de Icaza.
 *         Federico Mena.
 *         Horacio Peña.
 *
 * The idea is originally from KDE's same game program.
 *
 */

#include <stdlib.h>
#include <time.h>
#include <string.h>

#include <config.h>
#include <gnome.h>
#include <libgnomeui/gnome-window-icon.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gconf/gconf-client.h>
#include <games-gconf.h>
#include <games-frame.h>
#include <games-files.h>

/* Define a sensible alternative to ngettext if we don't have it. Note that
 * this is only sensible in the context of same-gnome. */
#ifndef HAVE_NGETTEXT
#define ngettext(one,lots,n) gettext(lots)
#endif

#define STONE_SIZE 40
#define STONE_COLS  15
#define STONE_LINES 10
#define GAME_EVENTS (GDK_EXPOSURE_MASK              |\
		     GDK_BUTTON_PRESS_MASK          |\
		     GDK_ENTER_NOTIFY_MASK          |\
		     GDK_LEAVE_NOTIFY_MASK          |\
		     GDK_POINTER_MOTION_MASK)

#define KEY_DIR "/apps/same-gnome"
#define KEY_TILESET "/apps/same-gnome/tileset"
#define KEY_SCORE "/apps/same-gnome/score"
#define KEY_FIELD "/apps/same-gnome/field"

static GtkWidget *pref_dialog, *scorew;
static GtkWidget *app, *draw_area, *vb, *appbar;
static GdkPixbuf *image;

static int tagged_count = 0;
static int ball_timeout_id = -1;
static int old_x = -1, old_y = -1;
static int score;
static gchar *scenario;
static gint restarted;
static gboolean game_over = FALSE;

static void update_score_state (void);

/* Prefs */
GConfClient *conf_client = NULL;

static struct ball {
	int color;
	int tag;
	int frame;
} field [STONE_COLS][STONE_LINES];

static int nstones;
static int ncolors;
static int sync_stones = 0;

#define mapx(x) (x)
#define mapy(y) (STONE_LINES-1-(y))

static void
draw_ball (int x, int y)
{
	int bx, by;

	if (x >= STONE_COLS || x < 0)
		return;
	if (y >= STONE_LINES || y < 0)
		return;

	if (field [x][y].color) {
		by = STONE_SIZE * (field [x][y].color - 1);
		bx = STONE_SIZE * (field [x][y].frame);

		gdk_draw_pixbuf (draw_area->window,
				 draw_area->style->black_gc, image,
				 bx, by,
				 x * STONE_SIZE, y * STONE_SIZE,
				 STONE_SIZE, STONE_SIZE,
				 GDK_RGB_DITHER_NORMAL, 0, 0);
	} else {
		gdk_window_clear_area (draw_area->window,
				       x * STONE_SIZE, y * STONE_SIZE,
				       STONE_SIZE, STONE_SIZE);
	}
}

static void
paint (GdkRectangle *area)
{
	int x1, y1, x2, y2, x, y;
	
	x1 = area->x / STONE_SIZE;
	y1 = area->y / STONE_SIZE;
	x2 = (area->x + area->width) / STONE_SIZE;
	y2 = (area->y + area->height) / STONE_SIZE;

	for (x = x1; x <= x2; x++) {
		for (y = y1; y <= y2; y++) {
			draw_ball (x, y);
		}
	}
}

static void
untag_all ()
{
	int x, y;

	for (x = 0; x < STONE_COLS; x++)
		for (y = 0; y < STONE_LINES; y++) {
			field [x][y].tag   = 0;
			if (sync_stones) {
				field [x][y].frame = 0;
				draw_ball (x, y);
			}
		}
}

static int
flood_fill (int x, int y, int color)
{
	int c = 0;
	
	if (!color)
		return c;
	
	if (field [x][y].color != color)
		return c;
	
	if (field [x][y].tag)
		return c;

	c = 1;
	field [x][y].tag = 1;
	
	if (x+1 < STONE_COLS)
		c += flood_fill (x+1, y, color);
	if (x)
		c += flood_fill (x-1, y, color);
	if (y+1 < STONE_LINES)
		c += flood_fill (x, y+1, color);
	if (y)
		c += flood_fill (x, y-1, color);
	return c;
}

static int
move_tagged_balls (void *data)
{
	int x, y;
	
	for (x = 0; x < STONE_COLS; x++)
		for (y = 0; y < STONE_LINES; y++) {
			if (!field [x][y].tag)
				continue;
			field [x][y].frame = (field [x][y].frame + 1) % nstones;
			draw_ball (x, y);
		}
	return 1;
}

static void
disable_timeout ()
{
	if (ball_timeout_id != -1) {
		g_source_remove (ball_timeout_id);
		ball_timeout_id = -1;
	}
}

static void
mark_balls (int x, int y)
{
	if (x == old_x && y == old_y)
		return;
	old_x = x;
	old_y = y;

	untag_all ();
	disable_timeout ();
	
	tagged_count = flood_fill (x, y, field [x][y].color);
	
	if (tagged_count > 1) {
		gchar *b;
		ball_timeout_id = g_timeout_add (100, move_tagged_balls, 0);
		b = g_strdup_printf (ngettext ("%d stone selected", "%d stones selected", tagged_count), tagged_count);
                gnome_appbar_set_status (GNOME_APPBAR (appbar), b);
		g_free (b);
	} else
                gnome_appbar_set_status (GNOME_APPBAR (appbar),
					 _("No stones selected"));
}

static void
compress_column (int x)
{
	int y, ym;
	
	for (y = STONE_LINES - 1; y >= 0; y--) {
		if (!field [mapx (x)][mapy (y)].tag)
			continue;
		for (ym = y; ym < STONE_LINES - 1; ym++)
			field [mapx (x)][mapy (ym)] 
				= field [mapx (x)][mapy (ym+1)];
		field [mapx (x)][mapy (ym)].color = 0;
		field [mapx (x)][mapy (ym)].tag   = 0;
	}
}

static void
compress_y ()
{
	int x;

	for (x = 0; x < STONE_COLS; x++)
		compress_column (x);
}

static void
copy_col (int dest, int src)
{
	int y;
	
	for (y = 0; y < STONE_LINES; y++)
		field [mapx (dest)][mapy (y)] = field [mapx (src)][mapy (y)];
}

static void
clean_last_col ()
{
	int y;

	for (y = 0; y < STONE_LINES; y++) {
		field [mapx (STONE_COLS-1)][mapy (y)].color = 0;
		field [mapx (STONE_COLS-1)][mapy (y)].tag   = 0;
	}
}

static void
compress_x ()
{
	int x, xm, l;

	for (x = 0; x < STONE_COLS; x++) {
		for (l = STONE_COLS; field [mapx(x)][mapy(0)].color == 0 && l; l--) {
			for (xm = x; xm < STONE_COLS-1; xm++)
				copy_col (xm, xm+1);
			clean_last_col ();
		} 
	}
}

static void
set_score (int new_score)
{
	char *b = NULL;
	
	score = new_score;
	b = g_strdup_printf ("%.5d", score);
	gtk_label_set_text (GTK_LABEL (scorew), b);
	g_free (b);
}

static void
show_scores (guint pos)
{
	GtkWidget *dialog;

	dialog = gnome_scores_display (_("The Same GNOME"),
				       "same-gnome", NULL, pos);
	if (dialog != NULL) {
		gtk_window_set_transient_for (GTK_WINDOW (dialog),
					      GTK_WINDOW (app));
		gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	}
}

static void
game_top_ten_callback (GtkWidget *widget, gpointer data)
{
	show_scores (0);
}

static void
check_game_over (void)
{
	int cleared=1;
	int x,y;
	int pos;
	
	for (x = 0; x < STONE_COLS; x++)
		for (y = 0 ; y < STONE_LINES; y++) {
			if (!field [x][y].color)
				continue;
			cleared = 0;
			if (x+1 < STONE_COLS) 
				if(field[x][y].color == field[x+1][y].color)
					return;
			if (y+1 < STONE_LINES) 
				if (field[x][y].color == field[x][y+1].color)
					return;
		}

	if (cleared)
		set_score (score+1000);

	pos = gnome_score_log (score, NULL, TRUE);
	update_score_state ();
	show_scores (pos);
	game_over = TRUE;
}

static void
kill_balls (int x, int y)
{
	if (! field [x][y].color)
		return;
	
	if (tagged_count < 2)
		return;

	set_score (score + (tagged_count - 2) * (tagged_count - 2));
	compress_y ();
	compress_x ();
	gtk_widget_queue_draw (GTK_WIDGET (draw_area));
	check_game_over ();
}

static gint
area_event (GtkWidget *widget, GdkEvent *event, void *d)
{
	switch (event->type) {
	case GDK_EXPOSE: {
		GdkEventExpose *e = (GdkEventExpose *) event;
		paint (&e->area);
		return TRUE;
	}
	
	case GDK_BUTTON_PRESS: {
		int x, y;
		gtk_widget_get_pointer (widget, &x, &y);
		kill_balls (x / STONE_SIZE, y / STONE_SIZE);
		old_x = -1;
		old_y = -1;
	}

	case GDK_ENTER_NOTIFY:
	case GDK_MOTION_NOTIFY: {
		int x, y;
		
		gtk_widget_get_pointer (widget, &x, &y);
		mark_balls (x / STONE_SIZE, y / STONE_SIZE);
		return TRUE;
	}
	
	case GDK_LEAVE_NOTIFY:
		old_x = -1;
		old_y = -1;
		disable_timeout ();
		untag_all ();
                gnome_appbar_set_status (GNOME_APPBAR(appbar), "");
		return TRUE;

	default:
		return FALSE;
	}
}

static void
fill_board (void)
{
	int x, y;

	for (x = 0; x < STONE_COLS; x++)
		for (y = 0; y < STONE_LINES; y++) {
			field [x][y].color = 1 + (rand () % ncolors);
			field [x][y].tag   = 0;
			field [x][y].frame = sync_stones ? 0 : (rand () % nstones);
		}
}

static void
new_game (void)
{
	game_over = FALSE;
	fill_board ();
	set_score (0);
	gtk_widget_queue_draw (GTK_WIDGET (draw_area));
}

static void
configure_sync (char *fname)
{
	if (g_strrstr (fname, "-sync.png"))
		sync_stones = 1;
	else
		sync_stones = 0;
}

static void
load_scenario (char *fname)
{
	gchar *fn;
        GdkColor background_color;
	gint i, j;
	GdkColormap *colormap;
	gchar *color_name;
    
	g_return_if_fail (fname != NULL);

	if (g_path_is_absolute (fname))
		fn = g_strdup (fname);
	else
		fn = g_build_filename (PIXMAPDIR, fname, NULL);

	if (! g_file_test (fn, G_FILE_TEST_EXISTS)) {
		GtkWidget *box = gtk_message_dialog_new 
			(GTK_WINDOW (app),
			 GTK_DIALOG_DESTROY_WITH_PARENT,
			 GTK_MESSAGE_ERROR,
			 GTK_BUTTONS_OK,
			 _("Could not find the theme:\n%s\n\n"
			   "Please check your Same GNOME installation."), fn);

		gtk_dialog_set_default_response (GTK_DIALOG (box), GTK_RESPONSE_OK);
		gtk_dialog_run (GTK_DIALOG (box));
		gtk_widget_destroy (box);
                /* We may not have called gtk_main yet, but if we do we want
                 * to exit nicely. */
                if (gtk_main_level() > 0)
                  gtk_main_quit ();
                else
                  exit (1);
	}

	g_free (scenario);
	scenario = g_strdup (fname);

	configure_sync (fname);

	if (image)
		g_object_unref (image);

	image = gdk_pixbuf_new_from_file (fn, NULL);

	if (image == NULL) {
		GtkWidget *box = gtk_message_dialog_new 
			(GTK_WINDOW (app),
			 GTK_DIALOG_DESTROY_WITH_PARENT,
			 GTK_MESSAGE_ERROR,
			 GTK_BUTTONS_OK,
			 _("Same GNOME can't load the image file:\n%s\n\n"
			   "Please check your Same GNOME installation"), fn);
		
		gtk_dialog_set_default_response (GTK_DIALOG (box),
						 GTK_RESPONSE_OK);
		gtk_dialog_run (GTK_DIALOG (box));
		gtk_widget_destroy (box);
                /* We may not have called gtk_main yet, but if we do we want
                 * to exit nicely. */
                if (gtk_main_level() > 0)
                  gtk_main_quit ();
                else
                  exit(1);
	}

	/* FIXME: this should be a preference */
	color_name = g_strdup ("#000000");
	gdk_color_parse (color_name, &background_color);
	g_free (color_name);

	colormap = gtk_widget_get_colormap (draw_area);
	gdk_colormap_alloc_color (colormap, &background_color, FALSE, TRUE);

        gdk_window_set_background (draw_area->window, &background_color);
  
	g_free( fn );

	nstones = gdk_pixbuf_get_width (image) / STONE_SIZE;
	for (i = 0; i < STONE_COLS; i++)
	{
		for (j = 0; j < STONE_LINES; j++)
		{
			if (sync_stones) {
				field[i][j].frame = 0;
			} else {
				field[i][j].frame %= nstones;
			}
		}
	}
/*	ncolors = image->rgb_height / STONE_SIZE; */
	ncolors = 3;


	gtk_widget_queue_draw (GTK_WIDGET (draw_area));
}

static void
create_same_board (char *fname)
{
	draw_area = gtk_drawing_area_new ();

	gtk_widget_set_events (draw_area, gtk_widget_get_events (draw_area) | GAME_EVENTS);

	gtk_box_pack_start_defaults (GTK_BOX (vb), draw_area);
	gtk_widget_realize (draw_area);
  
	gtk_widget_show (draw_area);

	load_scenario (fname);
	gtk_widget_set_size_request (GTK_WIDGET (draw_area),
				     STONE_COLS  * STONE_SIZE,
				     STONE_LINES * STONE_SIZE);
	g_signal_connect (G_OBJECT (draw_area), "event",
			  G_CALLBACK (area_event), 0);
}

static void
game_new_callback (GtkWidget *widget, void *data)
{
	new_game ();
}

static void
set_selection (GtkTreeSelection *selection, gpointer data)
{
        gchar * filename;
        GtkTreeModel * model;
        GtkTreeIter iter;

        gtk_tree_selection_get_selected (selection, &model, &iter);
        gtk_tree_model_get (model, &iter, 1, &filename, -1);

	load_scenario (filename);
	gconf_client_set_string (conf_client, KEY_TILESET, filename, NULL);
}

static void fill_list_foreach (gchar * string, GtkListStore * list)
{
        GtkTreeIter iter;
	gchar *name;
	gchar *suffix;
	
	/* We strip any trailing suffix, any -sync suffix
	 * and convert '_' to ' '. This is brutal code. */
	name = g_strdup (string);
	suffix = g_strrstr (name,".");
	if (suffix) *suffix = '\0';
	suffix = g_strrstr (name,"-sync");
	if (suffix) *suffix = '\0';
	suffix = name;
	while (*suffix) {
		if (*suffix == '_') *suffix = ' ';
		suffix++;
	}
	
	gtk_list_store_append (list, &iter);
	gtk_list_store_set (list, &iter, 0, name, 1, g_strdup (string), -1);
} 

static void
fill_list (GtkListStore *list)
{
	GamesFileList * filelist;
	
	filelist = games_file_list_new_images (PIXMAPDIR, NULL);
	games_file_list_transform_basename (filelist);
	
	games_file_list_for_each (filelist, (GFunc) fill_list_foreach, list);

	g_object_unref (filelist);
}

static void
pref_dialog_response (GtkDialog *dialog, gint response, gpointer data)
{
	gtk_widget_destroy (pref_dialog);
	pref_dialog = NULL;
}

static void
game_preferences_callback (GtkWidget *widget, void *data)
{
	GtkWidget *listview, *frame;
	GtkWidget *scroll;
        GtkTreeViewColumn *column;
        GtkTreeSelection * select;
        GtkListStore *list;

	if (pref_dialog) {
		gtk_window_present (GTK_WINDOW (pref_dialog));
		return;
	}

	pref_dialog = gtk_dialog_new_with_buttons (_("Same GNOME Preferences"),
			GTK_WINDOW (app),
			GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
			GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
			NULL);

	gtk_dialog_set_default_response (GTK_DIALOG (pref_dialog),
					 GTK_RESPONSE_OK);
        gtk_dialog_set_has_separator (GTK_DIALOG (pref_dialog), FALSE);
	g_signal_connect (G_OBJECT (pref_dialog), "response",
			  G_CALLBACK(pref_dialog_response), NULL);

        list = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
	fill_list (list);

        listview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (list));
        column = gtk_tree_view_column_new_with_attributes (_("Theme"),
                                                           gtk_cell_renderer_text_new (),
                                                           "text", 0,
                                                           NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (listview),
                                     GTK_TREE_VIEW_COLUMN (column));
        gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (listview), FALSE);
        
        select = gtk_tree_view_get_selection (GTK_TREE_VIEW (listview));
        gtk_tree_selection_set_mode (select, GTK_SELECTION_BROWSE);
        gtk_tree_selection_unselect_all (select);

	{
		gboolean valid;
		GtkTreeIter iter;
		GtkTreeModel *model;
		gint index;

		/* Get an iter associated with the view */
		
		model = gtk_tree_view_get_model (GTK_TREE_VIEW (listview));
		valid = gtk_tree_model_get_iter_first (model, &iter);
		index = 0;
		while (valid) {
			/* Walk through list, testing each row */
			gchar *filename;
			gtk_tree_model_get (model, &iter,
					    1, &filename, -1);

			if (strcmp (filename, scenario) == 0) {
				gtk_tree_view_set_cursor (GTK_TREE_VIEW (listview),
							  gtk_tree_path_new_from_indices (index, -1),
							  NULL, FALSE);
				/*gtk_tree_selection_select_iter (select,
				  &iter);*/
			}
			valid = gtk_tree_model_iter_next (model, &iter);
			index++;
		}
	}

        g_signal_connect (G_OBJECT (select), "changed",
                          G_CALLBACK (set_selection), NULL);
        
        scroll = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);
        gtk_widget_set_size_request (scroll, 250, 200);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll),
                                             GTK_SHADOW_ETCHED_IN);
        gtk_container_add (GTK_CONTAINER (scroll), listview);
        
        frame = games_frame_new (_("Theme"));
        gtk_container_add (GTK_CONTAINER (frame), scroll);

        gtk_box_pack_start (GTK_BOX (GTK_DIALOG (pref_dialog)->vbox),
                            frame, TRUE, TRUE, GNOME_PAD_SMALL);

        gtk_widget_show_all (pref_dialog);
}

static void
game_about_callback (GtkWidget *widget, void *data)
{
	GdkPixbuf *pixbuf = NULL;
	static GtkWidget *about = NULL;
	const gchar *authors[] = {
		"Miguel de Icaza",
		"Federico Mena",
		"Horacio J. Pe\xc3\xb1""a",		
		NULL
	};
	gchar *documenters[] = {
                NULL
        };
        /* Translator credits */
        gchar *translator_credits = _("translator-credits");

	if (about) {
		gtk_window_present (GTK_WINDOW (about));
		return;
	}

	{
		char *filename = NULL;

		filename = gnome_program_locate_file (NULL,
				GNOME_FILE_DOMAIN_APP_PIXMAP,  ("gnome-gsame.png"),
				TRUE, NULL);
		if (filename != NULL)
		{
			pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
			g_free (filename);
		}
	}
	
	about = gnome_about_new (_("Same GNOME"), VERSION,
				 "Copyright \xc2\xa9 1997-2004 Free Software "
				 "Foundation, Inc.",
				 _("Original idea from KDE's same game "
				   "program."),				 (const char **)authors,
				 (const char **)documenters,
				 strcmp (translator_credits, "translator-credits") != 0 ? translator_credits : NULL,
                                 pixbuf);
	if (pixbuf != NULL)
		g_object_unref (pixbuf);
	
	gtk_window_set_transient_for (GTK_WINDOW (about), GTK_WINDOW (app));
	gtk_widget_show (about);

	g_signal_connect (G_OBJECT (about), "destroy",
			  G_CALLBACK (gtk_widget_destroyed), &about);
}

static int
game_quit_callback (GtkWidget *widget, void *data)
{
	gtk_main_quit ();
	return FALSE;
}

GnomeUIInfo gamemenu[] = {

        GNOMEUIINFO_MENU_NEW_GAME_ITEM(game_new_callback, NULL),

	GNOMEUIINFO_SEPARATOR,

	GNOMEUIINFO_MENU_SCORES_ITEM(game_top_ten_callback, NULL),

	GNOMEUIINFO_SEPARATOR,

        GNOMEUIINFO_MENU_QUIT_ITEM(game_quit_callback, NULL),

	GNOMEUIINFO_END
};

GnomeUIInfo settingsmenu[] = {
        GNOMEUIINFO_MENU_PREFERENCES_ITEM(game_preferences_callback, NULL),

	GNOMEUIINFO_END
};

GnomeUIInfo helpmenu[] = {
        GNOMEUIINFO_HELP("same-gnome"),

	GNOMEUIINFO_MENU_ABOUT_ITEM(game_about_callback, NULL),

	GNOMEUIINFO_END
};

GnomeUIInfo mainmenu[] = {
	GNOMEUIINFO_MENU_GAME_TREE(gamemenu),
	GNOMEUIINFO_MENU_SETTINGS_TREE(settingsmenu),
	GNOMEUIINFO_MENU_HELP_TREE(helpmenu),
	GNOMEUIINFO_END
};

static void 
update_score_state (void)
{
        gchar **names = NULL;
        gfloat *scores = NULL;
        time_t *scoretimes = NULL;
	gint top;

	top = gnome_score_get_notable("same-gnome", NULL, &names, &scores, &scoretimes);
	if (top > 0) {
		gtk_widget_set_sensitive (gamemenu[2].widget, TRUE);
		g_strfreev(names);
		g_free(scores);
		g_free(scoretimes);
	} else {
		gtk_widget_set_sensitive (gamemenu[2].widget, FALSE);
	}
}

static int
save_state (GnomeClient *client,
	    gint phase,
	    GnomeSaveStyle save_style,
	    gint shutdown, 
	    GnomeInteractStyle interact_style,
	    gint fast,
	    gpointer client_data)
{
	gchar *buf;
	struct ball *f = (struct ball*) field;
	int i;  
	
	gconf_client_set_int (conf_client, KEY_SCORE, score, NULL);
	
	buf= g_malloc (STONE_COLS*STONE_LINES+1);

	for (i = 0 ; i < (STONE_COLS*STONE_LINES); i++) {
		buf [i]= f [i].color + 'a';
	}
	buf [STONE_COLS*STONE_LINES]= '\0';
	gconf_client_set_string (conf_client, KEY_FIELD, buf, NULL);
	g_free (buf);

	return TRUE;
}


static void
restart (void)
{
	gchar *buf;
	struct ball *f = (struct ball*) field;
	int i;

	/* It's too late to fix this for 2.4, but setting the score
	 * from something the player can fiddle with is *not* a
	 * good idea. */
	score = gconf_client_get_int (conf_client, KEY_SCORE, NULL);
	if (score < 0)
		score = 0;
	
	buf = gconf_client_get_string (conf_client, KEY_FIELD, NULL);

	if (buf) {
		for (i= 0; i < (STONE_COLS*STONE_LINES); i++) 
		{
			f[i].color= buf[i] - 'a';
			if (f[i].color < 1)
				f[i].color = 1;
			if (f[i].color > ncolors)
				f[i].color = ncolors;
			f[i].tag  = 0;
			f[i].frame= rand () % nstones;
		}
		g_free (buf);
	}
}

static gint
client_die (GnomeClient *client, gpointer client_data)
{
        gtk_main_quit ();

	return FALSE;
}

#ifndef GNOME_CLIENT_RESTARTED
#define GNOME_CLIENT_RESTARTED(client) \
(GNOME_CLIENT_CONNECTED (client) && \
 (gnome_client_get_previous_id (client) != NULL) && \
 (strcmp (gnome_client_get_id (client), \
  gnome_client_get_previous_id (client)) == 0))
#endif /* GNOME_CLIENT_RESTARTED */

int
main (int argc, char *argv [])
{
	static char *fname;
	static const struct poptOption options[] = {
		{ "scenario", 's', POPT_ARG_STRING, &fname, 0, N_("Set game scenario"), N_("NAME") },
		{ NULL, '\0', 0, NULL, 0 }
	};
        GtkWidget * label;
	GnomeClient *client;

	gnome_score_init("same-gnome");

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

        gnome_program_init ("same-gnome", VERSION,
                             LIBGNOMEUI_MODULE,
                             argc, argv,
                             GNOME_PARAM_POPT_TABLE, options,
                             GNOME_PARAM_APP_DATADIR, DATADIR, NULL);

	gnome_window_icon_set_default_from_file (GNOME_ICONDIR"/gnome-gsame.png");
	client= gnome_master_client ();

	g_signal_connect (G_OBJECT (client), "save_yourself",
			  G_CALLBACK (save_state), argv[0]);
	g_signal_connect (G_OBJECT (client), "die",
			  G_CALLBACK (client_die), NULL);

	if (GNOME_CLIENT_RESTARTED (client)) {
		restart ();
		restarted = 1;
	}

	/* Get the default GConfClient */
	conf_client = gconf_client_get_default ();
	if (!games_gconf_sanity_check_string (conf_client, KEY_TILESET)) {
		return 1;
	}

	srand (time (NULL));

	app = gnome_app_new ("same-gnome", _("Same GNOME"));

        gtk_window_set_resizable (GTK_WINDOW (app), FALSE);
	g_signal_connect (G_OBJECT (app), "delete_event",
			  G_CALLBACK (game_quit_callback), NULL);

	appbar = gnome_appbar_new (FALSE, TRUE, GNOME_PREFERENCES_USER);
	gnome_app_set_statusbar (GNOME_APP (app), GTK_WIDGET (appbar));

	gnome_appbar_set_status (GNOME_APPBAR (appbar),
				_("Welcome to Same GNOME!"));

	gnome_app_create_menus (GNOME_APP (app), mainmenu);

	gnome_app_install_menu_hints (GNOME_APP (app), mainmenu);
  
        vb = gtk_vbox_new (FALSE, 0);
	gnome_app_set_contents (GNOME_APP (app), vb);

	if (! fname) {
		fname = gconf_client_get_string
			(conf_client, KEY_TILESET, NULL);
	}

	if (!fname)
		fname = g_strdup ("stones.png");

	create_same_board (fname);

	update_score_state ();
	
	label = gtk_label_new (_("Score:"));
	scorew = gtk_label_new ("");
	set_score (score);

	gtk_box_pack_start(GTK_BOX(appbar), label, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(appbar), scorew, FALSE, TRUE, 0);
	
	if (!restarted)
		new_game ();
	
	g_free (fname);

        gtk_widget_show_all (app);

	gtk_main ();
	return 0;
}
