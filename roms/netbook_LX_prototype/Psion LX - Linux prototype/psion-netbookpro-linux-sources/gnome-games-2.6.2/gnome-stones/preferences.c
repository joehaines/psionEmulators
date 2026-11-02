/* -*- mode:C; indent-tabs-mode:t; tab-width:8; c-basic-offset:2; -*- */

/* gnome-stones - preferences.h
 *
 * Time-stamp: <2003-08-02 04:29:58 callum>
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

#include "preferences.h"
#include <games-frame.h>
#include "status.h"
#include "main.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>

#include "sound.h"

#define KEY_SCROLL_METHOD "/apps/gnome-stones/preferences/scroll_method"
#define KEY_GAME_NAME "/apps/gnome-stones/preferences/game_name"
#define KEY_START_CAVE "/apps/gnome-stones/preferences/start_cave"
#define KEY_JOYSTICK_DEVICE "/apps/gnome-stones/preferences/joystick_device"
#define KEY_JOYSTICK_SWITCH "/apps/gnome-stones/preferences/joystick_switch_level"
#define KEY_SOUND_ENABLED "/apps/gnome-stones/preferences/sound_enabled"


/*****************************************************************************/
/* Global Variables */

/* Enum for preferences dialog */
enum {
     GAME_STRING,
     CAVES_INT,
     FILENAME_STRING,
     INDEX_INT,
     N_COLS
};

/* The default game, that should be loaded, if no client state is to
   be restored.  If this variables value is NULL, than 'default.cave'
   will be used instead.  */

gchar *default_game= NULL;

/* This variable specifies the currently played game.  If 'game' is
   equal to 'NULL', than no game is loaded.  */

GStonesGame *game= NULL;

/* The currently played cave.  This cave must be a cave, that belongs
   to the game 'game'.  */

GStonesCave *cave= NULL;

/* The data about the player.  */

GStonesPlayer *player= NULL;

/* You may start a game in different cavs.  'start_cave' decides, in
   which cave the player will start.  */

guint start_cave= 0;

/* If you use a joystick as input device, this variable holds the
   device's id.  Setting it to GDK_CORE_POINTER disables the Joystick
   support.  */
#if 0
  guint32  joystick_deviceid    = GDK_CORE_POINTER;
#else
  guint32 joystick_deviceid = 0;
#endif
gfloat   joystick_switch_level= 0.5;


/* This is default scroll method for view function */

void (*view_scroll_method) (GStonesView *view, GStonesCave *cave)= atari_scroll;


/* The game can be in different states.  These state decides, how to
   react if some events occur.  */

GameState state= STATE_TITLE;


/* The preferences dialog.  */
GtkWidget *preferences_dialog= NULL;



/****************************************************************************/
/* Stuff for managing the list of avaiable games.  */


typedef struct _GameFile GameFile;

struct _GameFile
{
  gchar *filename;
  gchar *gametitle;
  guint  caves;
};


GList *games= NULL;


static gint
compare_game_names (const GameFile *file, const char *name)
{
  return strcmp (file->filename, name);
}


static GameFile *
add_game (const char *filename)
{
  char     *prefix= NULL;
  GList    *tmp;
  GameFile *file;

  if (!filename || (*filename == '\0'))
    return NULL;
  
  /* Maybe this game is already in the games list.  */
  tmp= g_list_find_custom (games, (gpointer) filename, 
			   (GCompareFunc) compare_game_names);
  if (tmp) return (GameFile *) tmp->data;
  
  /* FIXME: add some checks, if it's realy a Gnome-Stones game.  */
  file= g_malloc (sizeof (GameFile));

  if (file)
    {
      file->filename = g_strdup (filename);

      prefix= g_strconcat ("=", filename, "=/", NULL);
      gnome_config_push_prefix (prefix);
      g_free (prefix);
      
      file->gametitle= gnome_config_get_translated_string_with_default ("General/Title", NULL);
      if (file->gametitle == NULL) {
        gnome_config_pop_prefix ();
        g_free (file);
        return NULL;
      }
      
      file->caves    = gnome_config_get_int               ("Caves/Number");

      gnome_config_pop_prefix ();
      
      games= g_list_append (games, file);
    }

  return file;
}


static gboolean
caves_suffix (gchar *name)
{
 int l = strlen (name);

 if (l < 7) 
   return FALSE;

 return strcmp (name + l - 6, ".caves") == 0;

}

void
game_directory_scan (const char *directory)
{
  DIR *dir;
  
  dir= opendir (directory);
  if (dir)
    {
      struct dirent *entry;
      
      while ((entry = readdir (dir)) != NULL)
	{
	  gchar *filename = g_build_filename (directory, entry->d_name, NULL);
	  struct stat sbuf;
	  
	  if ( caves_suffix (filename) 
	       && (stat (filename, &sbuf)== 0) && S_ISREG (sbuf.st_mode))
	    add_game (filename);

	  g_free (filename);
	}
      
      closedir (dir);
    }
}


static void
load_game_by_number (guint n)
{
  GList *tmp= g_list_nth (games, n);

  g_return_if_fail (tmp != NULL);
  
  load_game (((GameFile *) tmp->data)->filename, 0);
}


static gboolean
load_game_by_name (const char *filename, guint cave)
{
  GameFile *file= add_game (filename);

  if (file)
    {
      return load_game (file->filename, cave);
    }
  
  return FALSE;
}



/*****************************************************************************/
/* string<-->scroll_method conversion  */


static gchar *
scroll_method_name (void)
{
  if (view_scroll_method==atari_scroll)
    return "atari_scroll";
  if (view_scroll_method==smooth_scroll)
    return "smooth_scroll";
  if (view_scroll_method==center_scroll)
    return "center_scroll";
 
  return "default";

}

static void
set_scroll_method (gchar *name)
{

  if (!strcmp (name,"atari_scroll"))
    view_scroll_method = atari_scroll;
  if (!strcmp (name,"smooth_scroll"))
    view_scroll_method = smooth_scroll;
  if (!strcmp (name,"center_scroll"))
    view_scroll_method = center_scroll;

}




/*****************************************************************************/
/* Save preferences.  */

void
gconf_set_scroll_method (gchar *value)
{
  gconf_client_set_string (get_gconf_client (), KEY_SCROLL_METHOD,
                           value, NULL);
}

void
gconf_set_game_name (gchar *value)
{
  gconf_client_set_string (get_gconf_client (), KEY_GAME_NAME,
                           value, NULL);
}

void
gconf_set_start_cave (gint value)
{
  gconf_client_set_int (get_gconf_client (), KEY_START_CAVE,
                        value, NULL);
}

void
gconf_set_sound_enabled (gboolean value)
{
  gconf_client_set_bool (get_gconf_client (), KEY_SOUND_ENABLED,
                         value, NULL);
}

void
gconf_set_joystick_device (gchar *value)
{
  gconf_client_set_string (get_gconf_client (), KEY_JOYSTICK_DEVICE,
                           value, NULL);
}


void
gconf_set_joystick_switch_level (gfloat value)
{
  gconf_client_set_float (get_gconf_client (), KEY_JOYSTICK_SWITCH,
                          value, NULL);
}

void 
preferences_save (gboolean global)
{
#if 0
  gchar *devicename = NULL;
  GList *devices;

  for (devices = gdk_devices_list (); devices; devices = devices->next)
    {
      GdkDevice *info = devices->data;
      
      if (joystick_deviceid == info->deviceid)
	{
	  devicename = info->name;
	  break;
	}
    }
  if (devicename) 
    gconf_set_joystick_device (devicename);

  gconf_set_joystick_switch_level (joystick_switch_level);
#endif  

  gconf_set_scroll_method (scroll_method_name ());

  if (game)
    {
      gconf_set_game_name (game->filename);

      gconf_set_start_cave (start_cave);
    }

  gconf_set_sound_enabled (get_sound_enabled ());
}


void
preferences_save_global (void)
{
  preferences_save (TRUE);
}


static gint
preferences_save_local (GnomeClient        *client,
			gint                phase,
			GnomeSaveStyle      save_style,
			gint                shutdown,
			GnomeInteractStyle  interact_style,   
			gpointer            client_data)
{
  preferences_save (FALSE);

  return TRUE;
}



/*****************************************************************************/
/* Restoring the preferences from disc.  */

gboolean
pref_get_sound_enabled ()
{
  return gconf_client_get_bool (get_gconf_client (),
                                KEY_SOUND_ENABLED, NULL);
}

gboolean
preferences_restore (void)
{
  char        *filename;
  gboolean     def;
  guint        cave;
  gchar       *scroll_method;
  GtkWidget * dialog;
  
#if 0
  gchar       *devicename;

  devicename= gnome_config_get_string ("Preferences/Joystick device=");
  if (devicename)
    {
      GList *devices;
      
      for (devices= gdk_input_list_devices (); devices; devices= devices->next)
	{
	  GdkDeviceInfo *info = (GdkDeviceInfo *) devices->data;
	  
	  if (strcmp (info->name, devicename) == 0)
	    {
	      joystick_deviceid= info->deviceid;
	      break;
	    }
	}      
      g_free (devicename);
    }
  if (joystick_deviceid != GDK_CORE_POINTER)
    gdk_input_set_mode (joystick_deviceid, GDK_MODE_SCREEN);
#endif

  joystick_switch_level = gconf_client_get_float (get_gconf_client (),
                                                  KEY_JOYSTICK_SWITCH,
                                                  NULL);

  if (joystick_switch_level > 1.0)
    joystick_switch_level = 0.5;
  if (joystick_switch_level <= 0.0)
    joystick_switch_level = 0.5;
  
  set_sound_enabled (pref_get_sound_enabled ());

  scroll_method = gconf_client_get_string (get_gconf_client (),
                                           KEY_SCROLL_METHOD, NULL);
  if (scroll_method == NULL)
    scroll_method = g_strdup ("atari_scroll");
  set_scroll_method (scroll_method);
  g_free (scroll_method);

  filename = gconf_client_get_string (get_gconf_client (),
                                      KEY_GAME_NAME, NULL);
  def = (filename == NULL);

  cave = gconf_client_get_int (get_gconf_client (),
                               KEY_START_CAVE, NULL);

  if (!default_game || !load_game_by_name (default_game, 0))
    {
      if (def || !load_game_by_name (filename, cave))
	{
	  if (!load_game_by_name (CAVESDIR"/default.caves", 0)) {
            dialog = gtk_message_dialog_new (GTK_WINDOW (app),
                                             GTK_DIALOG_DESTROY_WITH_PARENT,
                                             GTK_MESSAGE_ERROR,
                                             GTK_BUTTONS_OK,
                                             "<b>%s</b>\n\n%s\n\n%s",
                                             _("Neither your chosen game nor the default game could be loaded."),
                                             _("Please make sure that GNOME Stones is correctly installed."),
                                             _("You may be able to select a different game from the preferences dialog."));
            gtk_label_set_use_markup (GTK_LABEL (GTK_MESSAGE_DIALOG (dialog)->label),
                                      TRUE);
            gtk_dialog_run (GTK_DIALOG (dialog));
            gtk_widget_destroy (dialog);
          }
	}
    }

  g_free (filename);

  return TRUE;
}



/****************************************************************************/
/* Preferences dialog stuff.  */

typedef struct _PreferencesData PreferencesData;

struct _PreferencesData
{
  /* Page one.  */
  GtkWidget        *game_list;
  gint              selected_game;

  /* Page two. */
  GtkWidget        *level_frame;
  
  guint32           joystick_deviceid;
  gfloat            joystick_switch_level;


  /* Page three */

  /* Page four */
  gchar            *scroll_method_name;

};


static void
preferences_response_cb (GtkWidget *w, gint response_id, gpointer data)
{
  PreferencesData *prdata= (PreferencesData *) data;

  g_return_if_fail (prdata != NULL);

  switch(response_id)
  {
  default:
       joystick_set_properties (prdata->joystick_deviceid,
                                prdata->joystick_switch_level);
       gtk_widget_destroy (preferences_dialog);
       preferences_dialog = NULL;
       break;

  }
}

static void 
game_selector_select_row (GtkTreeSelection * selection,
                          gpointer data)
{
  PreferencesData *prdata= (PreferencesData *) data;
  GtkTreeView * view;
  GtkTreeIter iter;
  GtkTreeModel *model;

  g_return_if_fail (prdata != NULL);

  view = gtk_tree_selection_get_tree_view(selection);

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      int index;
      gtk_tree_model_get (model, &iter, 
                          INDEX_INT, &index,
                          -1);
      prdata->selected_game= index;
      load_game_by_number (prdata->selected_game);
      if (game)
        gconf_set_game_name (game->filename);
  }
}


#if 0
/* The joystick callbacks.  */

static void
preferences_set_joystick_device (GtkWidget *widget, gpointer data)
{
  guint32 deviceid = GPOINTER_TO_UINT (data);
    (PreferencesData *) gtk_object_get_user_data (GTK_OBJECT (widget));

  prdata->joystick_deviceid= deviceid;

  if (deviceid == GDK_CORE_POINTER)
    {
      gtk_widget_set_sensitive (prdata->level_frame, FALSE);
    }
  else
    {
      gtk_widget_set_sensitive (prdata->level_frame, TRUE);
    }

}
#endif

static void
preferences_set_joystick_switch_level (GtkAdjustment *adjust, gpointer data)
{
  /*gconf_set_joystick_switch_level (adjust->value);*/
}



/* The scroll method callbacks.  */

static void
preferences_set_scroll_method (GtkWidget *widget, gpointer data)
{
  gchar * names[3] = { "atari_scroll", "smooth_scroll", "center_scroll" };
  
  set_scroll_method (names[gtk_combo_box_get_active (GTK_COMBO_BOX (widget))]);
  gconf_set_scroll_method (scroll_method_name ());
}


static void
enable_sound_callback (GtkWidget *widget, gpointer *data)
{
  gboolean is_on = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  gconf_client_set_bool (get_gconf_client (),
                         KEY_SOUND_ENABLED,
                         is_on, NULL);
  set_sound_enabled (is_on);
  gconf_set_sound_enabled (is_on);
}


static GtkWidget *
preferences_dialog_new (void)
{
  GtkWidget *properties;
  GtkWidget *box;
  GtkWidget *notebook;
  GtkWidget *list_view;
  GtkWidget *label;
  GtkWidget *scrolled;
  GtkWidget *frame;
  GtkWidget *fv;
  GtkWidget *enable_sound_toggle_button;
  GtkListStore *list;
  GtkTreeIter iter;
  GtkTreeSelection *selection;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;

  PreferencesData *prdata;
  
  prdata= g_malloc (sizeof (PreferencesData));
  
  properties = gtk_dialog_new_with_buttons (_("GNOME Stones Preferences"),
                                            GTK_WINDOW (app),
                                            GTK_DIALOG_DESTROY_WITH_PARENT,
                                            GTK_STOCK_CLOSE,
                                            GTK_RESPONSE_ACCEPT,
                                            NULL);
  gtk_dialog_set_has_separator (GTK_DIALOG (properties), FALSE);  
  notebook = gtk_notebook_new ();
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (properties)->vbox), notebook, 
                      TRUE, TRUE, 0);
  
  /* The first page of our preferences dialog. */
  box= gtk_vbox_new (FALSE, GNOME_PAD);
  gtk_container_set_border_width (GTK_CONTAINER (box), GNOME_PAD_SMALL);

  /* The list of game names.  */
  list= gtk_list_store_new (N_COLS, 
                            G_TYPE_STRING, /* Game title */ 
                            G_TYPE_INT, /* Number of caves */
                            G_TYPE_STRING, /* Filename */
                            G_TYPE_INT); /* Index - remains hidden */

  /* Populate list */
  {
    GList *tmp= games;
    int index = 0;
    while (tmp)
      {
	GameFile *file = (GameFile *)tmp->data;
	
        gtk_list_store_append (list, &iter);
        gtk_list_store_set (list, &iter,
                            GAME_STRING, file->gametitle,
                            CAVES_INT, file->caves,
                            FILENAME_STRING, file->filename,
                            INDEX_INT, index,
                            -1);
        index++;
	tmp= tmp->next;
      }
  }

  /* Create view */
  list_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (list));

  g_object_unref (list);

  prdata->game_list= list_view;

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("Game title"), 
                                                     renderer, 
                                                     "text", 0, 
                                                     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (list_view), column);
  
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("Caves"), renderer, 
                                                     "text", 1, 
                                                     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (list_view), column);

  /*
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("Filename"), renderer, 
                                                 "text", 2, 
                                                 NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (list_view), column);
  */

  scrolled = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                  GTK_POLICY_ALWAYS,
                                  GTK_POLICY_AUTOMATIC);

  gtk_container_add (GTK_CONTAINER (scrolled), list_view);
  gtk_box_pack_start (GTK_BOX (box), scrolled, TRUE, TRUE, GNOME_PAD_SMALL);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (list_view));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

  {
    gboolean valid;
    GtkTreeModel *model;
    /* Get an iter associated with the view */
    
    model = gtk_tree_view_get_model (GTK_TREE_VIEW (list_view));
    valid = gtk_tree_model_get_iter_first (model, &iter);
    while (valid)
      {
        /* Walk through list, testing each row */
        gchar *filename;
        int index;
        gtk_tree_model_get (model, &iter,
                            FILENAME_STRING, &filename,
                            INDEX_INT, &index, -1);
        if (game && strcmp (filename, game->filename) == 0)
          {
            gtk_tree_view_set_cursor (GTK_TREE_VIEW (list_view),
                                      gtk_tree_path_new_from_indices (index, -1),
                                      NULL, FALSE);
            prdata->selected_game = index;
          }
        valid = gtk_tree_model_iter_next (model, &iter);
      }
  }

  g_signal_connect ( selection, 
                     "changed", 
                     GTK_SIGNAL_FUNC (game_selector_select_row),
                     prdata);

  label= gtk_label_new_with_mnemonic (_("_Game"));
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), 
			    box, label);

  /* The second page of our preferences dialog. */

  prdata->joystick_deviceid    = joystick_deviceid;
  prdata->joystick_switch_level= joystick_switch_level;

  box= gtk_vbox_new (FALSE, GNOME_PAD);
  gtk_container_set_border_width (GTK_CONTAINER (box), GNOME_PAD_SMALL);
  
  {
    GtkObject *adjust;
    GtkWidget *frame;
    GtkWidget *scale;
    GtkWidget *hbox;
    GtkWidget *vbox;
    GtkWidget *optionmenu;
#if 0
    GList     *devices;
#endif
    frame= games_frame_new (_("Device"));
    gtk_box_pack_start (GTK_BOX (box), frame, FALSE, FALSE, GNOME_PAD_SMALL);

    vbox= gtk_vbox_new (FALSE, GNOME_PAD);
    gtk_container_add (GTK_CONTAINER (frame), vbox);

    hbox= gtk_hbox_new (FALSE, GNOME_PAD);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, GNOME_PAD_SMALL);

    label= gtk_label_new (_("Joystick device:"));
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, GNOME_PAD_SMALL);

    optionmenu= gtk_combo_box_new_text ();    

    /* We definatly have a "disable" entry.  */
    gtk_combo_box_append_text (GTK_COMBO_BOX (optionmenu), _("disabled"));

#if 0
    g_signal_connect (GTK_OBJECT (menuitem), "activate",
			(GtkSignalFunc) preferences_set_joystick_device,
			GUINT_TO_POINTER (GDK_CORE_POINTER));
#endif

#if 0    
    for (devices = gdk_input_list_devices (), i= 1; devices; 
	 devices= devices->next, i++)
      {
	GdkDeviceInfo *info = (GdkDeviceInfo *) devices->data;

	if (info->deviceid != GDK_CORE_POINTER)
	  {
	    menuitem= gtk_menu_item_new_with_label (info->name);

            g_signal_connect (GTK_OBJECT (menuitem), "activate",
                                (GtkSignalFunc) preferences_set_joystick_device,
                                GUINT_TO_POINTER (info->deviceid));

	    gtk_menu_shell_append (GTK_MENU_SHELL (device_menu), menuitem);
	  }

	if (info->deviceid == prdata->joystick_deviceid)
	  gtk_menu_set_active (GTK_MENU (device_menu), i);
      }
#endif    
    gtk_box_pack_start (GTK_BOX (hbox), optionmenu, FALSE, FALSE, 2);

    frame= games_frame_new (_("Digital joystick emulation"));
    gtk_box_pack_start (GTK_BOX (box), frame, FALSE, FALSE, GNOME_PAD_SMALL);

    vbox= gtk_vbox_new (FALSE, GNOME_PAD);
    gtk_container_add (GTK_CONTAINER (frame), vbox);

    hbox= gtk_hbox_new (FALSE, GNOME_PAD);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, GNOME_PAD_SMALL);

    label= gtk_label_new (_("Switch level:"));
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, GNOME_PAD_SMALL);

    adjust= gtk_adjustment_new (prdata->joystick_switch_level,
				0.0, 1.0, 0.02, 0.1, 0.0);

    g_signal_connect (adjust, "value_changed",
			(GtkSignalFunc) preferences_set_joystick_switch_level,
			NULL);
    
    scale= gtk_hscale_new (GTK_ADJUSTMENT (adjust));
    gtk_scale_set_digits (GTK_SCALE (scale), 2);
    gtk_box_pack_start (GTK_BOX (hbox), scale, TRUE, TRUE, GNOME_PAD_SMALL);
#if 0    
    if (prdata->joystick_deviceid == GDK_CORE_POINTER)
      {
	gtk_widget_set_sensitive (GTK_WIDGET (frame), FALSE);
      }
#endif
    prdata->level_frame= frame;
  }
  

  label= gtk_label_new_with_mnemonic (_("_Joystick"));
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), 
			    box, label);

  /* The third page of our preferences dialog. */

  box = gtk_vbox_new (FALSE, GNOME_PAD);
  gtk_container_set_border_width (GTK_CONTAINER (box), GNOME_PAD_SMALL);
  

  frame = games_frame_new (_("Sound"));
  fv = gtk_vbox_new (FALSE, FALSE);
  gtk_box_set_spacing (GTK_BOX (fv), 6);
  gtk_container_add (GTK_CONTAINER(frame), fv);
  gtk_box_pack_start (GTK_BOX(box), frame, 
                      FALSE, FALSE, 0);
  
  enable_sound_toggle_button = 
    gtk_check_button_new_with_label ( _("Enable sound") );
  if (get_sound_enabled ()) 
    {
      gtk_toggle_button_set_active
        (GTK_TOGGLE_BUTTON (enable_sound_toggle_button),
         TRUE);
    }
  g_signal_connect (G_OBJECT(enable_sound_toggle_button), "clicked", 
                    G_CALLBACK (enable_sound_callback), NULL);
  
  gtk_container_add (GTK_CONTAINER(fv), enable_sound_toggle_button);
  
  label = gtk_label_new_with_mnemonic (_("_Sound"));
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
			    box, label);


  /* Fourth page is miscellaneous stuff */
  
  box= gtk_vbox_new (FALSE, GNOME_PAD);
  gtk_container_set_border_width (GTK_CONTAINER (box), GNOME_PAD_SMALL);
  

  
  {
    GtkWidget *frame;
    GtkWidget *hbox;
    GtkWidget *vbox;
    GtkWidget *optionmenu;


    frame= games_frame_new (_("Scroll method"));
    gtk_box_pack_start (GTK_BOX (box), frame, FALSE, FALSE, GNOME_PAD_SMALL);

    vbox= gtk_vbox_new (FALSE, GNOME_PAD);
    gtk_container_add (GTK_CONTAINER (frame), vbox);

    hbox= gtk_hbox_new (FALSE, GNOME_PAD);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, GNOME_PAD_SMALL);

    /*
    label= gtk_label_new (_("Scroll method:"));
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, GNOME_PAD_SMALL);
    */

    optionmenu= gtk_combo_box_new_text ();
    

    prdata->scroll_method_name = scroll_method_name();

    gtk_combo_box_append_text (GTK_COMBO_BOX (optionmenu),
                               _("Atari like scrolling"));
    if (!strcmp(prdata->scroll_method_name,"atari_scroll"))
      gtk_combo_box_set_active (GTK_COMBO_BOX (optionmenu), 0);
    gtk_combo_box_append_text (GTK_COMBO_BOX (optionmenu),
                               _("Smooth scrolling"));
    if (!strcmp(prdata->scroll_method_name,"smooth_scroll"))
      gtk_combo_box_set_active (GTK_COMBO_BOX (optionmenu), 1);
    gtk_combo_box_append_text (GTK_COMBO_BOX (optionmenu),
                               _("Always in the center"));
    if (!strcmp(prdata->scroll_method_name,"center_scroll"))
      gtk_combo_box_set_active (GTK_COMBO_BOX (optionmenu), 2);
    
    
    g_signal_connect (GTK_OBJECT (optionmenu), "changed",
			(GtkSignalFunc) preferences_set_scroll_method,
			NULL);
   
    gtk_box_pack_start (GTK_BOX (hbox), optionmenu, FALSE, FALSE, 2);
  }

  label = gtk_label_new_with_mnemonic (_("_Misc."));
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
			    box, label);

  gtk_widget_show_all (notebook);

  g_signal_connect (GTK_OBJECT (properties), "response",
		      GTK_SIGNAL_FUNC (preferences_response_cb), prdata);

  return properties;
}


void
preferences_dialog_show (void)
{
  if (!preferences_dialog)
    {
      preferences_dialog = preferences_dialog_new ();
    }

  gtk_window_present (GTK_WINDOW (preferences_dialog));
}



/****************************************************************************/
/* Initialize the session management stuff.  */



void
session_management_init (void)
{
  GnomeClient *client= gnome_master_client ();
  
  g_signal_connect (GTK_OBJECT (client), "save_yourself",
		    GTK_SIGNAL_FUNC (preferences_save_local), 
		    GINT_TO_POINTER (FALSE));
  g_signal_connect (GTK_OBJECT (client), "die",
		    GTK_SIGNAL_FUNC (quit_cb), NULL);
}



/* Local Variables: */
/* mode:c */
/* eval:(load-library "time-stamp") */
/* eval:(make-local-variable 'write-file-hooks) */
/* eval:(add-hook 'write-file-hooks 'time-stamp) */
/* eval:(setq time-stamp-format '(time-stamp-yyyy/mm/dd time-stamp-hh:mm:ss user-login-name)) */
/* End: */
