/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * properties.c - properties for gataxx
 * Written by Chris Rogers (gandalf@pobox.com)
 * Based on iagno code written by  Ian Peters (itp@gnu.org)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * For more details see the file COPYING.
 */

#include "config.h"
#include <gnome.h>
#include <string.h>
#include <dirent.h>
#include <games-clock.h>
#include <games-frame.h>
#include <gconf/gconf-client.h>

#include "properties.h"
#include "gataxx.h"
#include "ataxx.h"


static GtkWidget *propbox = NULL;

extern GtkWidget *window;
extern GtkWidget *time_display;
extern GConfClient *gataxx_gconf_client;
extern guint black_computer_level;
extern guint white_computer_level;
extern guint computer_speed;
extern gint timer_valid;
extern guint black_computer_id;
extern guint white_computer_id;
extern gchar *tile_set;
extern gint8 pixmaps[7][7];
extern gint animate;
extern gint flip_pixmaps_id;
extern gboolean flip_final;

gboolean quick_moves;
gint response;

int mapped = 0;

gchar * gataxx_gconf_get_string (gchar *key);
gint gataxx_gconf_get_int (gchar *key, gint default_int);
gboolean gataxx_gconf_get_bool (gchar *key, gint default_bool);

GList * theme_list = NULL;

static gint clamp_int (gint input, gint low, gint high)
{
	if (input < low)
		input = low;
	if (input > high)
		input = high;

	return input;
}

void 
load_properties (void)
{
	black_computer_level =
		gataxx_gconf_get_int ("/apps/gataxx/blacklevel", 0);
	black_computer_level = clamp_int (black_computer_level, 0, 3);

	white_computer_level =
		gataxx_gconf_get_int ("/apps/gataxx/whitelevel", 0);
	white_computer_level = clamp_int (white_computer_level, 0, 3);
	
	tile_set = gataxx_gconf_get_string ("/apps/gataxx/tileset");
	if (tile_set == NULL)
		tile_set = g_strdup("classic.png");

	animate = gataxx_gconf_get_int ("/apps/gataxx/animate", 2);
	animate = clamp_int (animate, 0, 2);

	if ((quick_moves =
	     gataxx_gconf_get_bool ("/apps/gataxx/quickmoves", FALSE)))
	{
		computer_speed = COMPUTER_MOVE_DELAY / 2;
	}
	else {
		computer_speed = COMPUTER_MOVE_DELAY;
	}
	flip_final = gataxx_gconf_get_bool ("/apps/gataxx/flipfinal", TRUE);

	if (flip_pixmaps_id)
		g_source_remove (flip_pixmaps_id);
	switch (animate) {
		case 0:
			flip_pixmaps_id = g_timeout_add (100, flip_pixmaps, NULL);
			break;
		case 1:
			flip_pixmaps_id = g_timeout_add (PIXMAP_FLIP_DELAY * 8, flip_pixmaps, NULL);
			break;
		case 2: flip_pixmaps_id = g_timeout_add (PIXMAP_FLIP_DELAY, flip_pixmaps, NULL);
			break;
	}
}

static void
save_properties (void)
{
	gconf_client_set_int (gataxx_gconf_client, "/apps/gataxx/blacklevel",
	                      black_computer_level, NULL);
	gconf_client_set_int (gataxx_gconf_client, "/apps/gataxx/whitelevel",
	                      white_computer_level, NULL);
	gconf_client_set_bool (gataxx_gconf_client, "/apps/gataxx/quickmoves",
	                       quick_moves, NULL);
	gconf_client_set_string (gataxx_gconf_client, "/apps/gataxx/tileset",
	                         tile_set, NULL);
	gconf_client_set_int (gataxx_gconf_client, "/apps/gataxx/animate",
	                      animate, NULL);
	gconf_client_set_bool (gataxx_gconf_client, "/apps/gataxx/flipfinal",
	                       flip_final, NULL);
}

static void
apply_changes (void)
{
	guint i, j;

	{	
		games_clock_stop (GAMES_CLOCK (time_display));
		gtk_widget_set_sensitive (time_display, FALSE);
		games_clock_set_seconds (GAMES_CLOCK (time_display), 0);
		timer_valid = 0;
	}

	if (black_computer_id) {
		g_source_remove (black_computer_id);
		black_computer_id = 0;
	}
	
	if (white_computer_id) {
		g_source_remove (white_computer_id);
		white_computer_id = 0;
	}
	
	if (quick_moves) {
		computer_speed = COMPUTER_MOVE_DELAY / 2;
	}
	else {
		computer_speed = COMPUTER_MOVE_DELAY;
	}
	
	{
		load_pixmaps ();
		for (i = 0; i < 7; i++)
			for (j = 0; j < 7; j++)
				if (pixmaps [i][j] >= BLACK_TURN &&
						pixmaps[i][j] <= WHITE_TURN) {
					gui_draw_pixmap (pixmaps[i][j], i, j);
				}
				else {
					gui_draw_pixmap (0, i, j);
				}
	}
	
	if (flip_pixmaps_id) {
		g_source_remove (flip_pixmaps_id);
		flip_pixmaps_id = 0;
	}
	
	switch (animate) {
		case 0:
			flip_pixmaps_id = g_timeout_add (100, flip_pixmaps,
					NULL);
			break;
		case 1:
			flip_pixmaps_id = g_timeout_add (PIXMAP_FLIP_DELAY *
					8, flip_pixmaps, NULL);
			break;
		case 2: flip_pixmaps_id = g_timeout_add (PIXMAP_FLIP_DELAY,
					flip_pixmaps, NULL);
			break;
	}
	
	check_computer_players ();
}

static void
black_computer_level_select (GtkWidget *widget, gpointer data)
{
	if (((guint) data != black_computer_level) 
	    && (GTK_TOGGLE_BUTTON (widget)->active)) {
		black_computer_level = (guint) data;
	}
	save_properties ();
	apply_changes ();
}

static void
white_computer_level_select (GtkWidget *widget, gpointer data)
{
	if (((guint) data != white_computer_level) 
	    && (GTK_TOGGLE_BUTTON (widget)->active)) {
		white_computer_level = (guint) data;
	}
	save_properties ();
	apply_changes ();
}

static void
quick_moves_select (GtkWidget *widget, gpointer data)
{
	if (GTK_TOGGLE_BUTTON (widget)->active) {
		quick_moves = TRUE;
	}
	else {
		quick_moves = FALSE;
	}

	save_properties ();
	apply_changes ();
}

static void
flip_final_select (GtkWidget *widget, gpointer data)
{
	if (GTK_TOGGLE_BUTTON (widget)->active) {
		flip_final = TRUE;
	}
	else {
		flip_final = FALSE;
	}
	save_properties ();
	apply_changes ();
}


static void
animate_select (GtkWidget *widget, gpointer data)
{
	if (GTK_TOGGLE_BUTTON (widget)->active) {
		animate = (gint) data;
	}

	save_properties ();
	apply_changes ();
}

void
set_selection (GtkWidget *widget, gpointer data)
{
	GList * entry;
	
	if (tile_set)
		g_free (tile_set);

	entry = g_list_nth (theme_list,
			    gtk_combo_box_get_active (GTK_COMBO_BOX (widget)));
	tile_set = g_strdup ((char *) entry->data);
        
	save_properties ();
	apply_changes();
}

void
free_str (GtkWidget *widget, void *data)
{
        free(data);
}

void
fill_menu (GtkWidget *menu)
{
        struct dirent *e;
	/* Note that we are sharing pixmaps with iagno here. */
	gchar *dname = gnome_program_locate_file (NULL,
						  GNOME_FILE_DOMAIN_APP_PIXMAP,
						  ("iagno"), FALSE, NULL);
        DIR *dir;
        int itemno = 0;
	gboolean found_default = FALSE;

	if (theme_list) {
		g_list_foreach (theme_list, (GFunc) g_free, NULL);
		g_list_free (theme_list);
	}
	theme_list = NULL;
	
        dir = opendir (dname);
	g_free (dname);
	
        if (! dir) {
                return;
	}

        while ((e = readdir (dir)) != NULL) {
                char *s = GINT_TO_POINTER (g_strdup (e->d_name));

                if(! g_strrstr (e->d_name, ".png")) {
                        g_free (s);
                        continue;
                }

		theme_list = g_list_append (theme_list, s);
		gtk_combo_box_append_text (GTK_COMBO_BOX (menu), s);
		
                if (! strcmp (tile_set, s)) {
			found_default = TRUE;
                        gtk_combo_box_set_active (GTK_COMBO_BOX (menu),
						  itemno);
                }

                itemno++;
        }
        closedir (dir);

	/* FIXME: Should popup an error dialog if no themes were found */
	if (itemno == 0) {
		gtk_widget_set_sensitive (menu, FALSE);
		return;
	} 

	if (! found_default) {
		/* Set the theme arbitrarily to be the first one. */
		gtk_combo_box_set_active(GTK_COMBO_BOX(menu), 0);
	}
}

void
show_properties_dialog (void)
{
	GtkWidget *notebook;
	GtkWidget *frame;
	GtkWidget *label;
	GtkWidget *label2;
	GtkWidget *table;
	GtkWidget *button;
	GtkWidget *vbox, *vbox2;
	GtkWidget *hbox;
	GtkWidget *option_menu;
	
	if (propbox != NULL) {
		gtk_window_present (GTK_WINDOW(propbox));
		return;
	}

        propbox = gtk_dialog_new_with_buttons (_("Gataxx Preferences"),
					       GTK_WINDOW (window),
					       GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
					       GTK_STOCK_CLOSE,
					       GTK_RESPONSE_CLOSE,
					       NULL);
	
	g_signal_connect (G_OBJECT (propbox), "response",
			  G_CALLBACK (gtk_widget_destroy), &propbox);

	g_signal_connect (G_OBJECT (propbox), "destroy",
			  G_CALLBACK (gtk_widget_destroyed), &propbox);
	
	notebook = gtk_notebook_new ();
	gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), GTK_POS_TOP);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (propbox)->vbox), notebook, TRUE, TRUE, 0);
	
	/* Players tab */
	
	label = gtk_label_new_with_mnemonic (_("_Players"));

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox, label);

	table = gtk_table_new (1, 2, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (table), 6);
        gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox2), 12);
        gtk_box_pack_start (GTK_BOX (vbox), vbox2, FALSE, FALSE, 0);
	button = gtk_check_button_new_with_label (_("Quick Moves (cut computer delay in half)"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
				     (computer_speed == COMPUTER_MOVE_DELAY / 2));
	g_signal_connect (G_OBJECT (button), "toggled",
			  G_CALLBACK (quick_moves_select), NULL);
	gtk_box_pack_start (GTK_BOX (vbox2), button, FALSE, FALSE, 0);
	
	
	frame = games_frame_new (_("Dark"));
	
	vbox2 = gtk_vbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (vbox2), 6);
	
	button = gtk_radio_button_new_with_label (NULL, _("Human"));
	if (black_computer_level == 0) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	}
	g_signal_connect (G_OBJECT (button), "toggled",
			  G_CALLBACK (black_computer_level_select), (gpointer) 0);

	gtk_box_pack_start (GTK_BOX (vbox2), button, FALSE, FALSE, 0);

	button = gtk_radio_button_new_with_label (gtk_radio_button_get_group
		      				 (GTK_RADIO_BUTTON (button)), _("Level one"));
	if (black_computer_level == 1) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	}
	g_signal_connect (G_OBJECT (button), "toggled",
			  G_CALLBACK (black_computer_level_select), (gpointer) 1);
	
	gtk_box_pack_start (GTK_BOX (vbox2), button, FALSE, FALSE, 0);
	button = gtk_radio_button_new_with_label (gtk_radio_button_get_group
						 (GTK_RADIO_BUTTON (button)), _("Level two"));
	if (black_computer_level == 2) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	}
	g_signal_connect (G_OBJECT (button), "toggled",
			  G_CALLBACK (black_computer_level_select), (gpointer) 2);

	gtk_box_pack_start (GTK_BOX (vbox2), button, FALSE, FALSE, 0);
	button = gtk_radio_button_new_with_label (gtk_radio_button_get_group
			                         (GTK_RADIO_BUTTON (button)), _("Level three"));
	if (black_computer_level == 3) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	}
	g_signal_connect (G_OBJECT (button), "toggled",
			  G_CALLBACK (black_computer_level_select), (gpointer) 3);

	gtk_box_pack_start (GTK_BOX (vbox2), button, FALSE, FALSE, 0);

	gtk_container_add (GTK_CONTAINER (frame), vbox2);
	
	gtk_table_attach (GTK_TABLE (table), frame, 0, 1, 0, 1,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
			  6, 6);
	
	frame = games_frame_new (_("Light"));
	
	vbox2 = gtk_vbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (vbox2), 6);
	
	button = gtk_radio_button_new_with_label (NULL, _("Human"));
	if (white_computer_level == 0) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	}
	g_signal_connect (G_OBJECT (button), "toggled",
			  G_CALLBACK (white_computer_level_select), (gpointer) 0);
	gtk_box_pack_start (GTK_BOX (vbox2), button, FALSE, FALSE, 0);
	button = gtk_radio_button_new_with_label (gtk_radio_button_get_group
			                         (GTK_RADIO_BUTTON (button)), _("Level one"));
	if (white_computer_level == 1) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	}
	g_signal_connect (G_OBJECT (button), "toggled",
			  G_CALLBACK (white_computer_level_select), (gpointer) 1);
	gtk_box_pack_start (GTK_BOX (vbox2), button, FALSE, FALSE, 0);
	button = gtk_radio_button_new_with_label (gtk_radio_button_get_group
			                         (GTK_RADIO_BUTTON (button)), _("Level two"));
	if (white_computer_level == 2) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	}
	g_signal_connect (G_OBJECT (button), "toggled",
			  G_CALLBACK (white_computer_level_select), (gpointer) 2);
	gtk_box_pack_start (GTK_BOX (vbox2), button, FALSE, FALSE, 0);
	button = gtk_radio_button_new_with_label (gtk_radio_button_get_group
			                         (GTK_RADIO_BUTTON (button)), _("Level three"));
	if (white_computer_level == 3) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	}
	g_signal_connect (G_OBJECT (button), "toggled",
			  G_CALLBACK (white_computer_level_select), (gpointer) 3);
	gtk_box_pack_start (GTK_BOX (vbox2), button, FALSE, FALSE, 0);
	
	gtk_container_add (GTK_CONTAINER (frame), (vbox2));

	gtk_table_attach (GTK_TABLE (table), frame, 1, 2, 0, 1,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
			  6, 6);

	/* Appearance tab */
	
	label = gtk_label_new_with_mnemonic (_("_Appearance"));

	table = gtk_table_new (1, 2, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (table), 6);
	
	frame = games_frame_new ("Animation");
	
	vbox = gtk_vbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
	
	button = gtk_radio_button_new_with_label (NULL, _("None"));

	if (animate == 0) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	}
	g_signal_connect (G_OBJECT (button), "toggled",
			  G_CALLBACK (animate_select), (gpointer) 0);

	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
	button = gtk_radio_button_new_with_label (gtk_radio_button_get_group
			(GTK_RADIO_BUTTON (button)), _("Partial"));
	if (animate == 1) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	}
	g_signal_connect (G_OBJECT (button), "toggled",
			  G_CALLBACK (animate_select), (gpointer) 1);

	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
	button = gtk_radio_button_new_with_label (gtk_radio_button_get_group
			                         (GTK_RADIO_BUTTON (button)), _("Complete"));
	if (animate == 2) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	}
	g_signal_connect (G_OBJECT (button), "toggled",
			  G_CALLBACK (animate_select), (gpointer) 2);
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
	
	gtk_container_add (GTK_CONTAINER (frame), vbox);

	gtk_table_attach_defaults (GTK_TABLE (table), frame, 0, 1, 0, 1);

        frame = games_frame_new (_("Options"));
	vbox = gtk_vbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	
	button = gtk_check_button_new_with_label (_("Flip final results"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), flip_final);
	g_signal_connect (G_OBJECT (button), "toggled",
			  G_CALLBACK (flip_final_select), NULL);

	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
	
	hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	
	label2 = gtk_label_new (_("Tile set:"));
	
	gtk_box_pack_start (GTK_BOX (hbox), label2, FALSE, FALSE, 0);
	
	option_menu = gtk_combo_box_new_text ();
	fill_menu (option_menu);
	g_signal_connect (G_OBJECT (option_menu), "changed",
			  G_CALLBACK (set_selection), NULL);
	gtk_box_pack_start (GTK_BOX (hbox), option_menu, TRUE, TRUE, 0);
	
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	
	gtk_table_attach_defaults (GTK_TABLE (table), frame, 1, 2, 0, 1);

	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), table, label);
	
	gtk_widget_show_all (propbox);

}	

gint
gataxx_gconf_get_int (gchar *key, gint default_int)
{
	GError *error = NULL;
	gint retval;

	retval = gconf_client_get_int (gataxx_gconf_client, key, &error);
	if (error != NULL) {
		g_error_free (error);
		return default_int;
	}

	return retval;
}

gboolean
gataxx_gconf_get_bool (gchar *key, gboolean default_bool)
{
	GError *error = NULL;
	gboolean retval;

	retval = gconf_client_get_bool (gataxx_gconf_client, key, &error);
	if (error != NULL) {
		g_error_free (error);
		return default_bool;
	}

	return retval;
}

/* Returns gchar* you should free with g_free(). Returns NULL on not-found
 * or other error. */
gchar *
gataxx_gconf_get_string (gchar *key)
{
	GError *error = NULL;
	gchar *retval;

	retval = gconf_client_get_string (gataxx_gconf_client, key, &error);
	if (error != NULL) {
		g_error_free (error);
		return NULL;
	}

	return retval;
}

void
reload_properties (void)
{
	g_free (tile_set);
	tile_set = NULL;
	load_properties ();
	apply_changes ();
}

