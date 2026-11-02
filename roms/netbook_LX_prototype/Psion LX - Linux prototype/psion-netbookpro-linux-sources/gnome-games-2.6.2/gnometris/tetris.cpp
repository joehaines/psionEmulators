/* -*- mode:C++; tab-width:8; c-basic-offset:8; indent-tabs-mode:true -*- */

/*
 * written by J. Marcin Gorycki <marcin.gorycki@intel.com>
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

#include "tetris.h"
#include "field.h"
#include "blockops.h"
#include "blocks.h"
#include "preview.h"
#include "scoreframe.h"

#include <games-gconf.h>
#include <games-frame.h>

#include <gdk/gdkkeysyms.h>
#include <config.h>
#include <dirent.h>
#include <string.h>

GdkPixbuf **pic;

int LINES = 20;
int COLUMNS = 11;

int BLOCK_SIZE = 40;

int posx = COLUMNS / 2;
int posy = 0;

int blocknr = 0;
int rot = 0;
int color = 0;

int blocknr_next = -1;
int rot_next = -1;
int color_next = -1;

bool random_block_colors = false;
bool do_preview = true;
bool rotateCounterClockWise = true;

#define TETRIS_OBJECT "gnometris-tetris-object"

#define KEY_OPTIONS_DIR "/apps/gnometris/options"
#define KEY_BLOCK_PIXMAP "/apps/gnometris/options/block_pixmap"
#define KEY_BACKGROUND_PIXMAP "/apps/gnometris/options/background_pixmap"
#define KEY_STARTING_LEVEL "/apps/gnometris/options/starting_level"
#define KEY_DO_PREVIEW "/apps/gnometris/options/do_preview"
#define KEY_RANDOM_BLOCK_COLORS "/apps/gnometris/options/random_block_colors"
#define KEY_ROTATE_COUNTER_CLOCKWISE "/apps/gnometris/options/rotate_counter_clock_wise"
#define KEY_LINE_FILL_HEIGHT "/apps/gnometris/options/line_fill_height"
#define KEY_LINE_FILL_PROBABILITY "/apps/gnometris/options/line_fill_probability"

Tetris::Tetris(int cmdlLevel): 
	blockPixmap(0),
	bgPixmap(0),
	field(0),
	preview(0),
	paused(false), 
	timeoutId(0), 
	onePause(false), 
	inPlay(false),
	image(0),
	bgimage(0),
	setupdialog(0), 
	cmdlineLevel(cmdlLevel), 
	fastFall(false),
        dropBlock(false)
{
	double x1, y1, x2, y2;
	double width;
	double pts;

	pic = new GdkPixbuf*[tableSize];
	for (int i = 0; i < tableSize; ++i)
		pic[i] = 0;
	
	w = gnome_app_new("gnometris", _("Gnometris"));
	g_signal_connect (w, "delete_event", G_CALLBACK (gameQuit), this);
	gtk_window_set_resizable (GTK_WINDOW (w), FALSE);

	static GnomeUIInfo game_menu[] = 
	{
		GNOMEUIINFO_MENU_NEW_GAME_ITEM(gameNew, this),
		GNOMEUIINFO_MENU_PAUSE_GAME_ITEM(gamePause, this),
		GNOMEUIINFO_SEPARATOR,
		GNOMEUIINFO_MENU_SCORES_ITEM(gameTopTen, this),
		GNOMEUIINFO_MENU_END_GAME_ITEM(gameEnd, this),
		GNOMEUIINFO_SEPARATOR,
		GNOMEUIINFO_MENU_QUIT_ITEM(gameQuit, this),
		GNOMEUIINFO_END
	};

	gameMenuPtr = game_menu;
	
	static GnomeUIInfo settings_menu[] = 
	{
		GNOMEUIINFO_MENU_PREFERENCES_ITEM(gameProperties, this),
		GNOMEUIINFO_END
	};

	gameSettingsPtr = settings_menu;
	
	GnomeUIInfo help_menu[] = 
	{
		GNOMEUIINFO_HELP((gpointer)"gnometris"),
		GNOMEUIINFO_MENU_ABOUT_ITEM(gameAbout, this),
		GNOMEUIINFO_END
	};

	GnomeUIInfo mainmenu[] = 
	{
		GNOMEUIINFO_MENU_GAME_TREE(game_menu),
		GNOMEUIINFO_MENU_SETTINGS_TREE(settings_menu),
		GNOMEUIINFO_MENU_HELP_TREE(help_menu),
		GNOMEUIINFO_END
	};

	line_fill_height = 0;
	line_fill_prob = 5;

	/* init gconf */
	gconf_client = gconf_client_get_default ();
	games_gconf_sanity_check_string (gconf_client, KEY_BLOCK_PIXMAP);

	gconf_client_add_dir (gconf_client, KEY_OPTIONS_DIR, GCONF_CLIENT_PRELOAD_NONE, NULL);
	gconf_client_notify_add (gconf_client, KEY_OPTIONS_DIR, gconfNotify, this, NULL, NULL);

	initOptions ();

	gnome_app_create_menus(GNOME_APP(w), mainmenu);

	GtkWidget * hb = gtk_hbox_new(FALSE, 0);
	gnome_app_set_contents(GNOME_APP(w), hb);

	field = new Field(/*ops*/);
	ops = new BlockOps(field);

	gtk_widget_set_events(w, gtk_widget_get_events(w) | 
						  GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);
	
	GtkWidget *vb1 = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vb1), 10);
	gtk_box_pack_start_defaults(GTK_BOX(vb1), field->getWidget());
	gtk_box_pack_start(GTK_BOX(hb), vb1, 0, 0, 0);
	field->show();
	setupPixmap();

	g_signal_connect (w, "event", G_CALLBACK (eventHandler), this);
  
	GtkWidget *vb2 = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vb2), 10);
	gtk_box_pack_end(GTK_BOX(hb), vb2, 0, 0, 0);
	
	preview = new Preview();
	
	gtk_box_pack_start(GTK_BOX(vb2), preview->getWidget(), FALSE, FALSE, 0);
	
	scoreFrame = new ScoreFrame(cmdlineLevel);
	
	gtk_box_pack_end(GTK_BOX(vb2), scoreFrame->getWidget(), TRUE, FALSE, 0);

	setOptions ();
        setupScoreState ();

	themeList = NULL;
	bgThemeList = NULL;
	
	gtk_widget_show(hb);
	gtk_widget_show(vb1);
	gtk_widget_show(vb2);
	scoreFrame->show();
	gtk_widget_show(w);

	gtk_widget_set_sensitive(gameMenuPtr[1].widget, FALSE);
	gtk_widget_set_sensitive(gameMenuPtr[4].widget, FALSE);
	gtk_widget_set_sensitive(gameSettingsPtr[0].widget, TRUE);

        pauseMessage = gnome_canvas_item_new (gnome_canvas_root(GNOME_CANVAS(field->getWidget())),
                                              gnome_canvas_text_get_type(),
                                              "fill_color",
                                              "white",
                                              "x", COLUMNS*BLOCK_SIZE/2.0,
                                              "y", LINES*BLOCK_SIZE/2.0,
                                              "text", _("Paused"),
                                              "size_points", 36.0,
                                              0
                                              );

	/* Since gnome_canvas doesn't support setting the size of text in
	 * pixels (read the source where the "size" parameter gets set)
	 * and pango isn't forthcoming about how it scales things (see
	 * http://mail.gnome.org/archives/gtk-i18n-list/2003-August/msg00001.html
	 * and bug #119081). We guess at the size, see what size it is rendered
	 * to and then adjust the point size to fit. 36.0 points is pretty
	 * close for 96 dpi . */

	gnome_canvas_item_get_bounds (pauseMessage, &x1, &y1, &x2, &y2);
	width = x2 - x1;
	/* 0.8 is the fraction of the screen we want to use and 36.0 is
	 * the guess we use previously for the point size. */
	pts = 0.8*36.0*COLUMNS*BLOCK_SIZE/width;
	gnome_canvas_item_set (pauseMessage, "size_points", pts, 0);

        gnome_canvas_item_hide (pauseMessage);

        gameoverMessage = gnome_canvas_item_new (gnome_canvas_root(GNOME_CANVAS(field->getWidget())),
                                                 gnome_canvas_text_get_type(),
                                                 "fill_color",
                                                 "white",
                                                 "x", COLUMNS*BLOCK_SIZE/2.0,
                                                 "y", LINES*BLOCK_SIZE/2.0,
                                                 "text", _("Game Over"),
                                                 "size_points", 36.0,
                                                 0
                                                 );

	gnome_canvas_item_get_bounds (gameoverMessage, &x1, &y1, &x2, &y2);
	width = x2 - x1;
	/* 0.9 is the fraction of the screen we want to use and 36.0 is
	 * the guess we use previously for the point size. */
	pts = 0.9*36.0*COLUMNS*BLOCK_SIZE/width;
	gnome_canvas_item_set (gameoverMessage, "size_points", pts, 0);

        gnome_canvas_item_hide (gameoverMessage);

}

Tetris::~Tetris()
{
	delete ops;
	delete field;
	delete preview;
	delete scoreFrame;

	if (image)
		g_object_unref (G_OBJECT (image));
	if (bgimage)
		g_object_unref (G_OBJECT (bgimage));		

	if (blockPixmap)
		g_free(blockPixmap);
	if (bgPixmap)
		g_free(bgPixmap);

	delete[] pic;
}

void
Tetris::setupScoreState ()
{
        gchar **names = NULL;
        gfloat *scores = NULL;
        time_t *scoretimes = NULL;
	gint top = 0;

	top = gnome_score_get_notable ("gnometris", NULL, &names, &scores, &scoretimes);
	if (top > 0) {
		gtk_widget_set_sensitive (gameMenuPtr[3].widget, TRUE);
		g_strfreev (names);
		g_free (scores);
		g_free (scoretimes);
	} else {
		gtk_widget_set_sensitive (gameMenuPtr[3].widget, FALSE);
	}
}

void 
Tetris::setupdialogDestroy(GtkWidget *widget, void *d)
{
	Tetris *t = (Tetris*) d;
	if (t->setupdialog)
		gtk_widget_destroy(t->setupdialog);
	t->setupdialog = 0;
	gtk_widget_set_sensitive(t->gameMenuPtr[0].widget, TRUE);
}

void
Tetris::setupdialogResponse (GtkWidget *dialog, gint response_id, void *d)
{
	setupdialogDestroy (NULL, d);
}

void
Tetris::setupPixmap()
{
	gchar *pixname, *fullpixname;
        gchar * s;
	
	pixname = g_build_filename ("gnometris", blockPixmap, NULL);
	fullpixname = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_APP_PIXMAP, pixname, FALSE, NULL);
	g_free (pixname);

	if (!g_file_test (fullpixname, G_FILE_TEST_EXISTS))
	{
		GtkWidget *w = gtk_message_dialog_new (NULL,
						       GTK_DIALOG_DESTROY_WITH_PARENT,
						       GTK_MESSAGE_ERROR,
						       GTK_BUTTONS_OK,
						       _("Could not find the theme: \n%s\n\nPlease check your gnome-games installation"), fullpixname);
		gtk_dialog_run (GTK_DIALOG (w));
		exit(1);
	}

	if(image)
		g_object_unref (G_OBJECT (image));

	image = gdk_pixbuf_new_from_file(fullpixname, NULL);

	if (image == NULL) {
		GtkWidget *w = gtk_message_dialog_new (NULL,
						       GTK_DIALOG_DESTROY_WITH_PARENT,
						       GTK_MESSAGE_ERROR,
						       GTK_BUTTONS_OK,
						       _("Can't load the image: \n%s\n\nPlease check your gnome-games installation"), fullpixname);
		gtk_dialog_run (GTK_DIALOG (w));
		exit (1);
	}
	g_free (fullpixname);

	BLOCK_SIZE = gdk_pixbuf_get_height(image);
	nr_of_colors = gdk_pixbuf_get_width(image) / BLOCK_SIZE;

	for (int i = 0; i < tableSize; ++i)
	{
		if (pic[i])
			g_object_unref (G_OBJECT (pic[i]));

		pic[i] = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, BLOCK_SIZE, BLOCK_SIZE);

		gdk_pixbuf_copy_area (image, (i % nr_of_colors) * BLOCK_SIZE, 0,
					BLOCK_SIZE, BLOCK_SIZE, pic[i], 0, 0);

	}

	pixname = g_build_filename ("gnometris", "bg", bgPixmap, NULL);
	fullpixname = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_APP_PIXMAP, pixname, FALSE, NULL);
	g_free (pixname);

	if (bgimage)
		g_object_unref (G_OBJECT (bgimage));

	if (g_file_test (fullpixname, G_FILE_TEST_EXISTS)) 
		bgimage = gdk_pixbuf_new_from_file (fullpixname, NULL);
	else {
                /* If we can't find a .png look for a .jpg, this is
                 * especially pertinant since a lot of the original
                 * backgrounds have been converted for size reasons. */
                s = g_strrstr(fullpixname, ".png");
		if (s == NULL) { /* Not a .png. */
			bgimage = 0;
		} else {
			g_strlcpy(s, ".jpg", 5);
			if (g_file_test (fullpixname, G_FILE_TEST_EXISTS)) {
				bgimage = gdk_pixbuf_new_from_file (fullpixname, NULL);
				s = g_strrstr (bgPixmap, ".png");
				g_strlcpy (s, ".jpg", 5);
				gconf_client_set_string (gconf_client, KEY_BACKGROUND_PIXMAP, bgPixmap, NULL);
			} else
				bgimage = 0;
		}
        }
	g_free (fullpixname);

	if (field)
	{
		field->updateSize (bgimage);
		gtk_widget_queue_resize (field->getWidget());
	}
	
	if (preview)
	{
		preview->updateSize ();
		gtk_widget_queue_resize (field->getWidget());
	}
}

void
Tetris::gconfNotify (GConfClient *tmp_client, guint cnx_id, GConfEntry *tmp_entry, gpointer tmp_data)
{
	Tetris *t = (Tetris *)tmp_data;

	t->initOptions ();
	t->setOptions ();
}

void
Tetris::initOptions ()
{
	GError *error = NULL;

	if (blockPixmap)
		g_free (blockPixmap);

	blockPixmap = gconf_client_get_string (gconf_client, KEY_BLOCK_PIXMAP, &error);
	if (error) {
		g_warning ("gconf error: %s\n", error->message);
		g_error_free (error);
		error = NULL;
	}
        if (blockPixmap == NULL)
          blockPixmap = g_strdup ("7blocks-tig.png");

	if (bgPixmap)
		g_free (bgPixmap);

	bgPixmap = gconf_client_get_string (gconf_client, KEY_BACKGROUND_PIXMAP, &error);
	if (error) {
		g_warning ("gconf error: %s\n", error->message);
		g_error_free (error);
		error = NULL;
	}
        if (bgPixmap == NULL)
          bgPixmap = g_strdup ("gnometris-bg.jpg");

	startingLevel = gconf_client_get_int (gconf_client, KEY_STARTING_LEVEL, &error);
	if (error) {
		g_warning ("gconf error: %s\n", error->message);
		g_error_free (error);
		error = NULL;
	}
        if (startingLevel < 1) 
          startingLevel = 1;
        if (startingLevel > 10) 
          startingLevel = 10;

	do_preview = gconf_client_get_bool (gconf_client, KEY_DO_PREVIEW, &error);
	if (error) {
		g_warning ("gconf error: %s\n", error->message);
		g_error_free (error);
		error = NULL;
	}

	random_block_colors = gconf_client_get_bool (gconf_client, KEY_RANDOM_BLOCK_COLORS, &error);
	if (error) {
		g_warning ("gconf error: %s\n", error->message);
		g_error_free (error);
		error = NULL;
	}
	
	rotateCounterClockWise = gconf_client_get_bool (gconf_client, KEY_ROTATE_COUNTER_CLOCKWISE, &error);
	if (error) {
		g_warning ("gconf error: %s\n", error->message);
		g_error_free (error);
		error = NULL;
	}

	line_fill_height = gconf_client_get_int (gconf_client, KEY_LINE_FILL_HEIGHT, &error);
	if (error) {
		g_warning ("gconf error: %s\n", error->message);
		g_error_free (error);
		error = NULL;
	}
	if (line_fill_height < 0)
		line_fill_height = 0;
	if (line_fill_height > 19)
		line_fill_height = 19;
	
	line_fill_prob = gconf_client_get_int (gconf_client, KEY_LINE_FILL_PROBABILITY, &error);
	if (error) {
		g_warning ("gconf error: %s\n", error->message);
		g_error_free (error);
		error = NULL;
	}
	if (line_fill_prob < 0)
		line_fill_prob = 0;
	if (line_fill_prob > 10)
		line_fill_prob = 10;
}

void
Tetris::setOptions ()
{
	if (setupdialog) {
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (sentry), startingLevel);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (fill_prob_spinner), line_fill_prob);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (fill_height_spinner), line_fill_height);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (do_preview_toggle), do_preview);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (random_block_colors_toggle), random_block_colors);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rotate_counter_clock_wise_toggle), rotateCounterClockWise);
	}

	scoreFrame->setLevel (startingLevel);
	scoreFrame->setStartingLevel (startingLevel);
	setupPixmap ();
}

void 
Tetris::setSelectionPreview(GtkWidget *widget, void *d)
{
	Tetris *t = (Tetris*) d;
	gconf_client_set_bool (t->gconf_client, KEY_DO_PREVIEW,
			       GTK_TOGGLE_BUTTON (widget)->active, NULL);
}

void 
Tetris::setSelectionBlocks(GtkWidget *widget, void *d)
{
	Tetris *t = (Tetris*) d;
	gconf_client_set_bool (t->gconf_client, KEY_RANDOM_BLOCK_COLORS,
			       GTK_TOGGLE_BUTTON (widget)->active, NULL);
}

void 
Tetris::setRotateCounterClockWise(GtkWidget *widget, void *d)
{
	Tetris *t = (Tetris*) d;
	gconf_client_set_bool (t->gconf_client, KEY_ROTATE_COUNTER_CLOCKWISE,
			       GTK_TOGGLE_BUTTON (widget)->active, NULL);
}

void
Tetris::setSelection(GtkWidget *widget, void *data)
{
	Tetris *t;
	GList * item;

      	t = (Tetris *)g_object_get_data (G_OBJECT (widget), TETRIS_OBJECT);

	item = g_list_nth (t->themeList,
			   gtk_combo_box_get_active (GTK_COMBO_BOX (widget)));

	gconf_client_set_string (t->gconf_client, KEY_BLOCK_PIXMAP,
				 (char *)item->data, NULL);
}

void
Tetris::setBGSelection(GtkWidget *widget, void *data)
{
	Tetris *t;
	GList * item;
	
	t = (Tetris *)g_object_get_data (G_OBJECT (widget), TETRIS_OBJECT);

	item = g_list_nth (t->bgThemeList,
			   gtk_combo_box_get_active (GTK_COMBO_BOX (widget)));
			   
	gconf_client_set_string (t->gconf_client, KEY_BACKGROUND_PIXMAP,
				 (char *)item->data, NULL);
}

void
Tetris::lineFillHeightChanged (GtkWidget *spin, gpointer data)
{
	Tetris *t = (Tetris *)data;
	gint value = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin));
	gconf_client_set_int (t->gconf_client, KEY_LINE_FILL_HEIGHT,
			      value, NULL);
}

void
Tetris::lineFillProbChanged (GtkWidget *spin, gpointer data)
{
	Tetris *t = (Tetris *)data;
	gint value = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin));
	gconf_client_set_int (t->gconf_client, KEY_LINE_FILL_PROBABILITY,
			      value, NULL);
}

void
Tetris::startingLevelChanged (GtkWidget *spin, gpointer data)
{
	Tetris *t = (Tetris *)data;
	gint value = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin));
	gconf_client_set_int (t->gconf_client, KEY_STARTING_LEVEL,
			      value, NULL);
}

void
Tetris::fillMenu(GtkWidget *menu, char *pixname, char *dirname, 
		 GList ** listp, bool addnone /*= false*/)
{
	struct dirent *e;
	char *dname = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_APP_PIXMAP, dirname, FALSE, NULL);
	DIR *dir;
	int itemno = 0;
	GList * list;

	list = *listp;

	if (list) {
		g_list_foreach (list, (GFunc) g_free, NULL);
		g_list_free (list);
		*listp = NULL;
	}
	
	dir = opendir (dname);

	if (!dir)
		return;
	
	char *s;
	
	while ((e = readdir (dir)) != 0)
	{
		s = g_strdup(e->d_name);

		if (!(strstr (e->d_name, ".png") || strstr (e->d_name, ".jpg"))) 
		{
			free(s);
			continue;
		}

		gtk_combo_box_append_text (GTK_COMBO_BOX (menu), s);
		*listp = g_list_append (*listp, s);
			  
		if (!strcmp(pixname, s))
		{
		  gtk_combo_box_set_active(GTK_COMBO_BOX (menu), itemno);
		}
		itemno++;
	}
	
	if (addnone)
	{
		s = g_strdup(_("<none>"));
		gtk_combo_box_append_text (GTK_COMBO_BOX (menu), _("<none>"));
		*listp = g_list_append (*listp, s);
	}
	
	closedir(dir);
}

int 
Tetris::gameProperties(GtkWidget *widget, void *d)
{
	GtkWidget *label;
	GtkWidget *frame;
	GtkWidget *table;
	GtkWidget *fvbox;
	GtkObject *adj;
        
	Tetris *t = (Tetris*) d;
	
	if (t->setupdialog) {
		gtk_window_present (GTK_WINDOW(t->setupdialog));
		return FALSE;
	}

	/* create the dialog */
	t->setupdialog =
		gtk_dialog_new_with_buttons(_("Gnometris preferences"), 
					    GTK_WINDOW (t->w),
					    (GtkDialogFlags)0,
					    GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
					    NULL);
	gtk_dialog_set_has_separator (GTK_DIALOG (t->setupdialog), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (t->setupdialog), 12);
	g_signal_connect (t->setupdialog, "close",
			  G_CALLBACK (setupdialogDestroy), d);
	g_signal_connect (t->setupdialog, "response",
			  G_CALLBACK (setupdialogResponse), d);

	frame = games_frame_new (_("Setup"));
	table = gtk_table_new (3, 2, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (table), 0);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 6);

	/* pre-filled rows */
	label = gtk_label_new(_("Number of pre-filled rows:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 0, 1);

	adj = gtk_adjustment_new (t->line_fill_height, 0, LINES-1, 1, 5, 0);
	t->fill_height_spinner = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 10, 0);
	gtk_spin_button_set_update_policy
		(GTK_SPIN_BUTTON (t->fill_height_spinner), GTK_UPDATE_ALWAYS);
	gtk_spin_button_set_snap_to_ticks 
		(GTK_SPIN_BUTTON (t->fill_height_spinner), TRUE);
	g_signal_connect (t->fill_height_spinner, "value_changed",
			  G_CALLBACK (lineFillHeightChanged), t);
	gtk_table_attach_defaults (GTK_TABLE (table), t->fill_height_spinner, 1, 2, 0, 1);

	/* pre-filled rows density */
	label = gtk_label_new (_("Density of blocks in a pre-filled row:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 1, 2);

	adj = gtk_adjustment_new (t->line_fill_prob, 0, 10, 1, 5, 0);
	t->fill_prob_spinner = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 10, 0);
	gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (t->fill_prob_spinner),
					  GTK_UPDATE_ALWAYS);
	gtk_spin_button_set_snap_to_ticks
		(GTK_SPIN_BUTTON (t->fill_prob_spinner), TRUE);
	g_signal_connect (t->fill_prob_spinner, "value_changed",
		          G_CALLBACK (lineFillProbChanged), t);
	gtk_table_attach_defaults (GTK_TABLE (table), t->fill_prob_spinner, 1, 2, 1, 2);

	/* starting level */
	label = gtk_label_new (_("Starting Level:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 2, 3);

	adj = gtk_adjustment_new (t->startingLevel, 1, 10, 1, 5, 10);
	t->sentry = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 10, 0);
	gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (t->sentry),
					   GTK_UPDATE_ALWAYS);
	gtk_spin_button_set_snap_to_ticks (GTK_SPIN_BUTTON (t->sentry), TRUE);
	g_signal_connect (t->sentry, "value_changed",
			  G_CALLBACK (startingLevelChanged), t);
	gtk_table_attach_defaults (GTK_TABLE (table), t->sentry, 1, 2, 2, 3);

	gtk_container_add (GTK_CONTAINER (frame), table);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (t->setupdialog)->vbox), frame, 
			    FALSE, FALSE, 0);

	frame = games_frame_new (_("Operation"));
	fvbox = gtk_vbox_new (FALSE, FALSE);
	gtk_box_set_spacing (GTK_BOX (fvbox), 6);

	/* preview next block */
	t->do_preview_toggle =
		gtk_check_button_new_with_label (_("Preview next block"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (t->do_preview_toggle),
				     do_preview);
	g_signal_connect (t->do_preview_toggle, "clicked",
			  G_CALLBACK (setSelectionPreview), d);
	gtk_box_pack_start (GTK_BOX (fvbox), t->do_preview_toggle, 0, 0, 0);

	/* random blocks */
	t->random_block_colors_toggle =
		gtk_check_button_new_with_label(_("Use random block colors"));
	gtk_toggle_button_set_active
		(GTK_TOGGLE_BUTTON (t->random_block_colors_toggle),
		 random_block_colors);
	g_signal_connect (t->random_block_colors_toggle, "clicked",
			  G_CALLBACK (setSelectionBlocks), d);
	gtk_box_pack_start (GTK_BOX (fvbox), t->random_block_colors_toggle,
			    0, 0, 0);

	/* rotate counter clock wise */
 	t->rotate_counter_clock_wise_toggle =
		gtk_check_button_new_with_label (_("Rotate blocks counterclockwise"));
 	gtk_toggle_button_set_active
		(GTK_TOGGLE_BUTTON (t->rotate_counter_clock_wise_toggle),
		 rotateCounterClockWise);
	g_signal_connect (t->rotate_counter_clock_wise_toggle, "clicked",
			  G_CALLBACK (setRotateCounterClockWise), d);
 	gtk_box_pack_start (GTK_BOX (fvbox), t->rotate_counter_clock_wise_toggle,
			    0, 0, 0);

	gtk_container_add (GTK_CONTAINER (frame), fvbox);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (t->setupdialog)->vbox), frame, 
			    FALSE, FALSE, 0);

	frame = games_frame_new (_("Theme"));
	table = gtk_table_new (2, 2, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (table), 0);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 6);

	/* Block pixmap */
	label = gtk_label_new(_("Block image:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 0, 1);

	GtkWidget *omenu = gtk_combo_box_new_text ();
	g_object_set_data (G_OBJECT (omenu), TETRIS_OBJECT, t);	
	t->fillMenu (omenu, t->blockPixmap, "gnometris", &(t->themeList));
	g_signal_connect (omenu, "changed", G_CALLBACK (setSelection), NULL);
	gtk_table_attach_defaults (GTK_TABLE (table), omenu, 1, 2, 0, 1);

	/* background pixmap */
	label = gtk_label_new(_("Background image:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 1, 2);
	
	omenu = gtk_combo_box_new_text ();	
	g_object_set_data (G_OBJECT (omenu), TETRIS_OBJECT, t);	
	gchar *tmp = g_build_filename ("gnometris", "bg", NULL);
	t->fillMenu (omenu, t->bgPixmap, tmp, &(t->bgThemeList), true);
	g_free (tmp);
	g_signal_connect (omenu, "changed", G_CALLBACK (setBGSelection), NULL);
	gtk_table_attach_defaults (GTK_TABLE (table), omenu, 1, 2, 1, 2);

	gtk_container_add (GTK_CONTAINER (frame), table);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (t->setupdialog)->vbox), frame, 
			    FALSE, FALSE, 0);
	
	gtk_widget_show_all (t->setupdialog);
	gtk_widget_set_sensitive(t->gameMenuPtr[0].widget, FALSE);
	return TRUE;
}

int
Tetris::gamePause(GtkWidget *widget, void *d)
{
	Tetris *t = (Tetris*) d;
	t->togglePause();
	return TRUE;
}

int
Tetris::gameEnd(GtkWidget *widget, void *d)
{
	Tetris *t = (Tetris*) d;
	
	g_source_remove(t->timeoutId);
	t->timeoutId = 0;
	blocknr_next = -1;
	t->endOfGame();
	return TRUE;
}

int
Tetris::gameQuit(GtkWidget *widget, void *d)
{
	Tetris *t = (Tetris*) d;

	/* Record the score if the game isn't over. */
	if (t->inPlay && (t->scoreFrame->getScore() > 0))
		gnome_score_log(t->scoreFrame->getScore(), 0, TRUE);

	if (t->w)
		gtk_widget_destroy(t->w);
	gtk_main_quit();

	return TRUE;
}

void
Tetris::generateTimer(int level)
{
	g_source_remove(timeoutId);

	int intv = 1000 - 100 * (level - 1);
	if (intv <= 0)
		intv = 100;
		
	timeoutId = g_timeout_add_full(0, intv, timeoutHandler, this, NULL);
}

void
Tetris::manageFallen()
{
	ops->fallingToLaying();

	int levelBefore = scoreFrame->getLevel();
	ops->checkFullLines(scoreFrame);
	int levelAfter = scoreFrame->getLevel();
	if ((levelBefore != levelAfter) || fastFall)
		generateTimer(levelAfter);
	
	generate();
}

int
Tetris::timeoutHandler(void *d)
{
	Tetris *t = (Tetris*) d;
	
	if (t->paused)
		return TRUE;

 	if (t->onePause)
 	{
		t->onePause = false;
		gtk_widget_queue_draw(t->field->getWidget());
	}
 	else
	{
		bool res = t->ops->moveBlockDown();
		gtk_widget_queue_draw(t->field->getWidget());

		if (res)
		{
			t->manageFallen();
			if(t->fastFall) {
				t->scoreFrame->incScore(t->fastFallPoints);
				t->fastFall = false;
			}
		}
		else if(t->fastFall)
			++t->fastFallPoints;
	}

	return TRUE;	
}

gint
Tetris::eventHandler(GtkWidget *widget, GdkEvent *event, void *d)
{
	Tetris *t = (Tetris*) d;
        int bonus;
        
	if (t->timeoutId == 0)
		return FALSE;
	
	if (event->type == GDK_KEY_PRESS)
	{
		int keyval = ((GdkEventKey*)event)->keyval;

		if ((keyval == GDK_p) || (keyval == GDK_P))
		{
			t->togglePause();
			return TRUE;
		}
	}
	
	if (t->paused)
		return FALSE;

	bool res = false;
	bool keyEvent = false;

	switch (event->type)
	{
	case GDK_KEY_PRESS: 
	{
		GdkEventKey *e = (GdkEventKey*)event;
		keyEvent = true;
		
		switch(e->keyval)
		{
		case GDK_Left:
			res = t->ops->moveBlockLeft();
                        t->onePause = false;
			break;
		case GDK_Right:
			res = t->ops->moveBlockRight();
                        t->onePause = false;
			break;
		case GDK_Up:
			res = t->ops->rotateBlock(rotateCounterClockWise);
                        t->onePause = false;
			break;
		case GDK_Down:
			if (!t->fastFall && !t->onePause)
			{
				t->fastFall = true;
				t->fastFallPoints = 0;
				g_source_remove(t->timeoutId);
				t->timeoutId = g_timeout_add_full(0, 10, timeoutHandler, t, NULL);
				res = true;
			}
			break;
		case GDK_space:
                        if (!t->dropBlock)
                        {
                                t->dropBlock = true;
                                bonus = t->ops->dropBlock();
                                t->scoreFrame->incScore(bonus);
                                t->manageFallen();
                                res = TRUE;
                        }        
			break;
		default:
			return FALSE;
		}
		break;
	}
	case GDK_KEY_RELEASE:
	{
		GdkEventKey *e = (GdkEventKey*)event;

		switch (e->keyval)
		{
		case GDK_Down:
			keyEvent = true;
			if (t->fastFall)
			{
				t->fastFall = false;
 				t->generateTimer(t->scoreFrame->getLevel());
			}
			break;
		case GDK_space:
                        t->dropBlock = false;
			break;
		default:
			return FALSE;
		}
		break;
	}
	default:
		break;
	}

	if (res)
		gtk_widget_queue_draw(t->field->getWidget());

	return (keyEvent == true);
}

void
Tetris::togglePause()
{
	paused = !paused;
        if (paused) {
                gnome_canvas_item_show (pauseMessage);
                gnome_canvas_item_raise_to_top (pauseMessage);
        } else
                gnome_canvas_item_hide (pauseMessage);
}

void
Tetris::generate()
{
	if (ops->generateFallingBlock())
	{
		ops->putBlockInField(false);
		gtk_widget_queue_draw(preview->getWidget());
		onePause = true;
	}
	else
	{
		g_source_remove(timeoutId);
		timeoutId = 0;
		blocknr_next = -1;
		
		endOfGame();
	}
}

void
Tetris::endOfGame()
{
	gtk_widget_set_sensitive(gameMenuPtr[1].widget, FALSE);
	gtk_widget_set_sensitive(gameMenuPtr[4].widget, FALSE);
	gtk_widget_set_sensitive(gameSettingsPtr[0].widget, TRUE);
	color_next = -1;
	blocknr_next = -1;
	rot_next = -1;
        gnome_canvas_item_hide (pauseMessage);
        gnome_canvas_item_show (gameoverMessage);
        gnome_canvas_item_raise_to_top (gameoverMessage);
	inPlay = false;

	if (scoreFrame->getScore() > 0) 
	{
		int pos = gnome_score_log(scoreFrame->getScore(), 0, TRUE);
		showScores("Gnometris", pos);
	}
}

void
Tetris::showScores(gchar *title, guint pos)
{
	GtkWidget *dialog;

	dialog = gnome_scores_display(title, "gnometris", 0, pos);
	if (dialog != NULL) {
		gtk_window_set_transient_for (GTK_WINDOW(dialog), GTK_WINDOW(this->getWidget()));
		gtk_window_set_modal (GTK_WINDOW(dialog), TRUE);
	}
}

int
Tetris::gameNew(GtkWidget *widget, void *d)
{
	Tetris *t = (Tetris*) d;

	if (t->timeoutId) 
	{
		g_source_remove(t->timeoutId);
		t->timeoutId = 0;

		/* Catch the case where we started a new game without
		 * finishing the old one. */
		if ((t->scoreFrame->getScore() > 0) && t->inPlay)
			gnome_score_log(t->scoreFrame->getScore(), 0, TRUE);
	}

	t->inPlay = true;

	int level = t->cmdlineLevel ? t->cmdlineLevel : t->startingLevel;

	t->fastFall = false;
	
	t->scoreFrame->setLevel(level);
	t->scoreFrame->setStartingLevel(level);

	t->timeoutId = g_timeout_add_full(0, 1000 - 100 * (level - 1), timeoutHandler, t, NULL);
	t->ops->emptyField(t->line_fill_height,t->line_fill_prob);

	t->scoreFrame->resetLines();
	t->paused = false;
	
	t->ops->generateFallingBlock();
	gtk_widget_queue_draw(t->field->getWidget());
	gtk_widget_queue_draw(t->preview->getWidget());

	gtk_widget_set_sensitive(t->gameMenuPtr[1].widget, TRUE);
	gtk_widget_set_sensitive(t->gameMenuPtr[4].widget, TRUE);
	gtk_widget_set_sensitive(t->gameSettingsPtr[0].widget, FALSE);

        gnome_canvas_item_hide (t->pauseMessage);
        gnome_canvas_item_hide (t->gameoverMessage);
        
	return TRUE;
}

int
Tetris::gameAbout(GtkWidget *widget, void *d)
{
	Tetris *t = (Tetris*) d;
	GdkPixbuf *pixbuf = NULL;
	static GtkWidget *about = NULL;

	const gchar *authors[] = {"J. Marcin Gorycki", 0};

	const gchar *documenters[] = {
		NULL
	};

	const gchar *translator_credits = _("translator-credits");

	if (about != NULL) {
		gtk_window_present (GTK_WINDOW(about));
		return TRUE;
	}
        {
		char *filename = NULL;

		filename = gnome_program_locate_file (NULL,
				GNOME_FILE_DOMAIN_APP_PIXMAP, 
				"gnome-gtetris.png",
				TRUE, NULL);
		if (filename != NULL)
		{
			pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
			g_free (filename);
		}
	}

	about = gnome_about_new("Gnometris", 
				VERSION, 
				"(C) 2000 J. Marcin Gorycki",
				_("Written for my wife, Matylda\n"
				  "Send comments and bug reports to: "
				  "janusz.gorycki@intel.com"),
				(const char **)authors,
				(const char **)documenters,
				strcmp (translator_credits, "translator-credits") != 0 ? translator_credits : NULL,
				pixbuf);
	
	if (pixbuf != NULL)
		gdk_pixbuf_unref (pixbuf);
	
	gtk_window_set_transient_for (GTK_WINDOW (about), GTK_WINDOW (t->getWidget()));
	g_signal_connect (G_OBJECT (about), "destroy", G_CALLBACK (gtk_widget_destroyed), &about);
	gtk_widget_show(about);

	return TRUE;
}

int
Tetris::gameTopTen(GtkWidget *widget, void *d)
{
	Tetris *t = (Tetris*) d;
	t->showScores("Gnometris", 0);

	return TRUE;
}
