/* LinNeighborhood
 * Copyright (c) 1999-2002 Richard Stemmer and Hans Schmid
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#include <stdio.h>
#include <gdk/gdkkeysyms.h>

#include "gtk_sharewindow.h"
#include "gtk_gui.h"
#include "utility.h"
#include "browsewrap.h"
#include "gtk_tree.h"
#include "gtk_dialog.h"

#include "picture/icon_toolbar_up.xpm"
#include "picture/icon_toolbar_cancel.xpm"
#include "picture/icon_toolbar_delete.xpm"
#include "picture/icon_toolbar_copy.xpm"
#include "picture/icon_toolbar_insert.xpm"
#include "picture/icon_toolbar_prefs.xpm"

#include "picture/ShareWindow.xpm"

#include "picture/i_directory.xpm"
#include "picture/i_default.xpm"
#include "picture/i_hidden.xpm"
#include "picture/i_txt.xpm"
#include "picture/i_cpp.xpm"
#include "picture/i_c.xpm"
#include "picture/i_h.xpm"
#include "picture/i_zip.xpm"
#include "picture/i_exe.xpm"
#include "picture/i_dll.xpm"
#include "picture/i_html.xpm"
#include "picture/i_gif.xpm"
#include "picture/i_java.xpm"
#include "picture/i_hlp.xpm"


/* default window constraints */
#define SHARE_WINDOW_DEF_WIDTH       520
#define SHARE_WINDOW_DEF_HEIGHT      400 

#define WINDOW_DATA                    "window_data"
#define SMB_FILESELECT_DATA            "smb_fileselect_data"

#define SHARE_CLIST_COLUMNS            5

#define NAME_COLUMN                    0
#define ATTR_COLUMN                    4

#define SHARE_WINDOW_MAX_TITLE_LEN        ( MAXGROUPNAMEL + MAXMACHNAMEL + 3 )
#define SHARE_WINDOW_MAX_EDIT_LEN         ( MAXSHRNAMEL + MAXSHAREPATHL + 3 )


typedef struct sharewin_struct_ {
        GtkWidget *window;
        GtkCList *clist;
        char group[MAXGROUPNAMEL+1];
        char machine[MAXMACHNAMEL+1];
        char share[MAXSHRNAMEL+1];
        char path[MAXSHAREPATHL+1];
} sharewin_struct;

static unsigned char share_window_initialized = 0;    /* flag: global data initialized */


static void share_window_add_window (GtkWidget *window, GtkCList *clist, share_window_struct *ws);
static void share_window_remove_by_window (GtkWidget *window);
static sharewin_struct *share_window_get_struct_by_window (GtkWidget *window);


typedef struct _share_window_data {
        GtkWidget *window;
        GtkWidget *edit;
        GtkWidget *clist;
        GtkWidget *object_status;
        GtkWidget *size_status;
        int object_context_id;
        int size_context_id;
        int name_width;
        int size_width;
        int date_width;
        int time_width;
        int attr_width;
        unsigned char keyStrg;
        share_window_struct share;
} share_window_data;


static void window_data_initialize (share_window_data *data)
{
  data->window = NULL;
  data->edit = NULL;
  data->clist = NULL;
  data->object_status = NULL;
  data->size_status = NULL;
  data->name_width = 230;
  data->size_width = 80;
  data->date_width = 70;
  data->time_width = 35;
  data->attr_width = 35;
  data->share.group[0] = 0;
  data->share.machine[0] = 0;
  data->share.share[0] = 0;
  data->share.path[0] = 0;
  data->keyStrg = 0;
}

static void window_data_delete (share_window_data *data)
{
  if ( data != NULL )
    g_free(data);
}

static share_window_data *window_data_get (GtkWidget *window)
{
  share_window_data *data;

  data = NULL;
  if ( window != NULL )
    data = (share_window_data*)gtk_object_get_data(GTK_OBJECT(window), WINDOW_DATA);

  return data;
}

/* ------------------------------------------------------------------------- */

/*
 * icon factory
 */

typedef enum {
        ICON_TOOLBAR_UP,
        ICON_TOOLBAR_CANCEL,
        ICON_TOOLBAR_DELETE,
        ICON_TOOLBAR_COPY,
        ICON_TOOLBAR_INSERT,
        ICON_TOOLBAR_PREFS,
        /* file icons */
        ICON_FILE_DIRECTORY,
        ICON_FILE_DEFAULT,
        ICON_FILE_HIDDEN,
        ICON_FILE_TXT,
        ICON_FILE_CPP,
        ICON_FILE_C,
        ICON_FILE_H,
        ICON_FILE_ZIP,
        ICON_FILE_EXE,
        ICON_FILE_DLL,
        ICON_FILE_HTML,
        ICON_FILE_GIF,
        ICON_FILE_JAVA,
        ICON_FILE_HLP
} ICON_TYPE;


static GdkPixmap *pixmap_window_icon = NULL;

static GdkPixmap *pixmap_toolbar_up = NULL;
static GdkPixmap *pixmap_toolbar_cancel = NULL;
static GdkPixmap *pixmap_toolbar_delete = NULL;
static GdkPixmap *pixmap_toolbar_copy = NULL;
static GdkPixmap *pixmap_toolbar_insert = NULL;
static GdkPixmap *pixmap_toolbar_prefs = NULL;

static GdkBitmap *mask_window_icon;

static GdkBitmap *mask_toolbar_up;
static GdkBitmap *mask_toolbar_cancel;
static GdkBitmap *mask_toolbar_delete;
static GdkBitmap *mask_toolbar_copy;
static GdkBitmap *mask_toolbar_insert;
static GdkBitmap *mask_toolbar_prefs;


static GtkWidget *get_icon (ICON_TYPE type)
{
  GtkWidget *icon = NULL;
  
  switch ( type )
  {
    case ICON_TOOLBAR_UP:
        if ( !pixmap_toolbar_up ) {
          pixmap_toolbar_up = gdk_pixmap_create_from_xpm_d(main_window->window,
                &mask_toolbar_up, 0, (gchar **)icon_toolbar_up_xpm);
        }
        icon = gtk_pixmap_new(pixmap_toolbar_up, mask_toolbar_up);
        break;
    
    case ICON_TOOLBAR_CANCEL:
        if ( !pixmap_toolbar_cancel ) {
          pixmap_toolbar_cancel = gdk_pixmap_create_from_xpm_d(main_window->window,
                &mask_toolbar_cancel, 0, (gchar **)icon_toolbar_cancel_xpm);
        }
        icon = gtk_pixmap_new(pixmap_toolbar_cancel, mask_toolbar_cancel);
        break;
    
    case ICON_TOOLBAR_DELETE:
        if ( !pixmap_toolbar_delete ) {
          pixmap_toolbar_delete = gdk_pixmap_create_from_xpm_d(main_window->window,
                &mask_toolbar_delete, 0, (gchar **)icon_toolbar_delete_xpm);
        }
        icon = gtk_pixmap_new(pixmap_toolbar_delete, mask_toolbar_delete);
        break;
    
    case ICON_TOOLBAR_COPY:
        if ( !pixmap_toolbar_copy ) {
          pixmap_toolbar_copy = gdk_pixmap_create_from_xpm_d(main_window->window,
                &mask_toolbar_copy, 0, (gchar **)icon_toolbar_copy_xpm);
        }
        icon = gtk_pixmap_new(pixmap_toolbar_copy, mask_toolbar_copy);
        break;
    
    case ICON_TOOLBAR_INSERT:
        if ( !pixmap_toolbar_insert ) {
          pixmap_toolbar_insert = gdk_pixmap_create_from_xpm_d(main_window->window,
                &mask_toolbar_insert, 0, (gchar **)icon_toolbar_insert_xpm);
        }
        icon = gtk_pixmap_new(pixmap_toolbar_insert, mask_toolbar_insert);
        break;
    
    case ICON_TOOLBAR_PREFS:
        if ( !pixmap_toolbar_prefs ) {
          pixmap_toolbar_prefs = gdk_pixmap_create_from_xpm_d(main_window->window,
                &mask_toolbar_prefs, 0, (gchar **)icon_toolbar_prefs_xpm);
        }
        icon = gtk_pixmap_new(pixmap_toolbar_prefs, mask_toolbar_prefs);
        break;
    
    default:
        break;;
  }
  
  return icon;
}

/* pixmaps for file icons */

static GdkPixmap *pixmap_file_directory = NULL;
static GdkPixmap *pixmap_file_default = NULL;
static GdkPixmap *pixmap_file_hidden = NULL;
static GdkPixmap *pixmap_file_txt = NULL;
static GdkPixmap *pixmap_file_cpp = NULL;
static GdkPixmap *pixmap_file_c = NULL;
static GdkPixmap *pixmap_file_h = NULL;
static GdkPixmap *pixmap_file_zip = NULL;
static GdkPixmap *pixmap_file_exe = NULL;
static GdkPixmap *pixmap_file_dll = NULL;
static GdkPixmap *pixmap_file_html = NULL;
static GdkPixmap *pixmap_file_gif = NULL;
static GdkPixmap *pixmap_file_java = NULL;
static GdkPixmap *pixmap_file_hlp = NULL;

static GdkBitmap *mask_file_directory;
static GdkBitmap *mask_file_default;
static GdkBitmap *mask_file_hidden;
static GdkBitmap *mask_file_txt;
static GdkBitmap *mask_file_cpp;
static GdkBitmap *mask_file_c;
static GdkBitmap *mask_file_h;
static GdkBitmap *mask_file_zip;
static GdkBitmap *mask_file_exe;
static GdkBitmap *mask_file_dll;
static GdkBitmap *mask_file_html;
static GdkBitmap *mask_file_gif;
static GdkBitmap *mask_file_java;
static GdkBitmap *mask_file_hlp;

static void create_pixmap (ICON_TYPE type)
{
  switch ( type )
  {
    case ICON_FILE_DIRECTORY:
        if ( !pixmap_file_directory ) {
          pixmap_file_directory = gdk_pixmap_create_from_xpm_d(main_window->window,
                &mask_file_directory, 0, (gchar **)i_directory_xpm);
        }
        break;
    
    case ICON_FILE_HIDDEN:
        if ( !pixmap_file_hidden ) {
          pixmap_file_hidden = gdk_pixmap_create_from_xpm_d(main_window->window,
                &mask_file_hidden, 0, (gchar **)i_hidden_xpm);
        }
        break;
    
    case ICON_FILE_TXT:
        if ( !pixmap_file_txt ) {
          pixmap_file_txt = gdk_pixmap_create_from_xpm_d(main_window->window,
                &mask_file_txt, 0, (gchar **)i_txt_xpm);
        }
        break;
    
    case ICON_FILE_CPP:
        if ( !pixmap_file_cpp ) {
          pixmap_file_cpp = gdk_pixmap_create_from_xpm_d(main_window->window,
                &mask_file_cpp, 0, (gchar **)i_cpp_xpm);
        }
        break;
    
    case ICON_FILE_C:
        if ( !pixmap_file_c ) {
          pixmap_file_c = gdk_pixmap_create_from_xpm_d(main_window->window,
                &mask_file_c, 0, (gchar **)i_c_xpm);
        }
        break;
    
    case ICON_FILE_H:
        if ( !pixmap_file_h ) {
          pixmap_file_h = gdk_pixmap_create_from_xpm_d(main_window->window,
                &mask_file_h, 0, (gchar **)i_h_xpm);
        }
        break;
    
    case ICON_FILE_ZIP:
        if ( !pixmap_file_zip ) {
          pixmap_file_zip = gdk_pixmap_create_from_xpm_d(main_window->window,
                &mask_file_zip, 0, (gchar **)i_zip_xpm);
        }
        break;
    
    case ICON_FILE_EXE:
        if ( !pixmap_file_exe ) {
          pixmap_file_exe = gdk_pixmap_create_from_xpm_d(main_window->window,
                &mask_file_exe, 0, (gchar **)i_exe_xpm);
        }
        break;
    
    case ICON_FILE_DLL:
        if ( !pixmap_file_dll ) {
          pixmap_file_dll = gdk_pixmap_create_from_xpm_d(main_window->window,
                &mask_file_dll, 0, (gchar **)i_dll_xpm);
        }
        break;
    
    case ICON_FILE_HTML:
        if ( !pixmap_file_html ) {
          pixmap_file_html = gdk_pixmap_create_from_xpm_d(main_window->window,
                &mask_file_html, 0, (gchar **)i_html_xpm);
        }
        break;
    
    case ICON_FILE_GIF:
        if ( !pixmap_file_gif ) {
          pixmap_file_gif = gdk_pixmap_create_from_xpm_d(main_window->window,
                &mask_file_gif, 0, (gchar **)i_gif_xpm);
        }
        break;
    
    case ICON_FILE_JAVA:
        if ( !pixmap_file_java ) {
          pixmap_file_java = gdk_pixmap_create_from_xpm_d(main_window->window,
                &mask_file_java, 0, (gchar **)i_java_xpm);
        }
        break;
    
    case ICON_FILE_HLP:
        if ( !pixmap_file_hlp ) {
          pixmap_file_hlp = gdk_pixmap_create_from_xpm_d(main_window->window,
                &mask_file_hlp, 0, (gchar **)i_hlp_xpm);
        }
        break;
    
    default:
        if ( !pixmap_file_default ) {
          pixmap_file_default = gdk_pixmap_create_from_xpm_d(main_window->window,
                &mask_file_default, 0, (gchar **)i_default_xpm);
        }
        break;
  }
}

static void fetch_file_icon (ICON_TYPE type, GdkPixmap **pixmap, GdkBitmap **mask)
{
  create_pixmap(type);
  
  switch ( type )
  {
    case ICON_FILE_DIRECTORY:
        *pixmap = pixmap_file_directory;
        *mask = mask_file_directory;
        break;
    case ICON_FILE_HIDDEN:
        *pixmap = pixmap_file_hidden;
        *mask = mask_file_hidden;
        break;
    case ICON_FILE_TXT:
        *pixmap = pixmap_file_txt;
        *mask = mask_file_txt;
        break;
    case ICON_FILE_CPP:
        *pixmap = pixmap_file_cpp;
        *mask = mask_file_cpp;
        break;
    case ICON_FILE_C:
        *pixmap = pixmap_file_c;
        *mask = mask_file_c;
        break;
    case ICON_FILE_H:
        *pixmap = pixmap_file_h;
        *mask = mask_file_h;
        break;
    case ICON_FILE_ZIP:
        *pixmap = pixmap_file_zip;
        *mask = mask_file_zip;
        break;
    case ICON_FILE_EXE:
        *pixmap = pixmap_file_exe;
        *mask = mask_file_exe;
        break;
    case ICON_FILE_DLL:
        *pixmap = pixmap_file_dll;
        *mask = mask_file_dll;
        break;
    case ICON_FILE_HTML:
        *pixmap = pixmap_file_html;
        *mask = mask_file_html;
        break;
    case ICON_FILE_GIF:
        *pixmap = pixmap_file_gif;
        *mask = mask_file_gif;
        break;
    case ICON_FILE_JAVA:
        *pixmap = pixmap_file_java;
        *mask = mask_file_java;
        break;
    case ICON_FILE_HLP:
        *pixmap = pixmap_file_hlp;
        *mask = mask_file_hlp;
        break;
    default:
        *pixmap = pixmap_file_default;
        *mask = mask_file_default;
        break;
  }
}

static void fetch_file_icon_by_name (char *filename, GdkPixmap **pixmap, GdkBitmap **mask)
{
  gchar *dot;
  ICON_TYPE type;
  
  dot = strrchr(filename, '.');
  if ( dot != NULL )
  {
    dot++;
    
    if ( !strcasecmp(dot, "txt") )
      type = ICON_FILE_TXT;
    else if ( !strcasecmp(dot, "cpp") )
      type = ICON_FILE_CPP;
    else if ( !strcasecmp(dot, "cc") )
      type = ICON_FILE_CPP;
    else if ( !strcasecmp(dot, "c++") )
      type = ICON_FILE_CPP;
    else if ( !strcasecmp(dot, "c") )
      type = ICON_FILE_C;
    else if ( !strcasecmp(dot, "h") )
      type = ICON_FILE_H;
    else if ( !strcasecmp(dot, "hpp") )
      type = ICON_FILE_H;
    else if ( !strcasecmp(dot, "zip") )
      type = ICON_FILE_ZIP;
    else if ( !strcasecmp(dot, "gz") )
      type = ICON_FILE_ZIP;
    else if ( !strcasecmp(dot, "tgz") )
      type = ICON_FILE_ZIP;
    else if ( !strcasecmp(dot, "exe") )
      type = ICON_FILE_EXE;
    else if ( !strcasecmp(dot, "dll") )
      type = ICON_FILE_DLL;
    else if ( !strcasecmp(dot, "html") )
      type = ICON_FILE_HTML;
    else if ( !strcasecmp(dot, "htm") )
      type = ICON_FILE_HTML;
    else if ( !strcasecmp(dot, "gif") )
      type = ICON_FILE_GIF;
    else if ( !strcasecmp(dot, "java") )
      type = ICON_FILE_JAVA;
    else if ( !strcasecmp(dot, "class") )
      type = ICON_FILE_JAVA;
    else if ( !strcasecmp(dot, "hlp") )
      type = ICON_FILE_HLP;
    else
      type = ICON_FILE_DEFAULT;
  }
  else
    type = ICON_FILE_DEFAULT;
  
  fetch_file_icon(type, pixmap, mask);
}

/* ---- utility ------------------------------------------------------------ */

static char *row_get_filename (GtkCList *clist, int row)
{
  char *name = NULL;
  guint8 spacing;
  GdkPixmap *pixmap;
  GdkBitmap *mask;
  
  gtk_clist_get_pixtext(clist, row, NAME_COLUMN, &name, &spacing, &pixmap, &mask);
  
  return name;
}

static unsigned char row_is_directory (GtkCList* clist, int row)
{
  unsigned char ret = 0;
  char *attr_text = NULL;
  
  gtk_clist_get_text(clist, row, ATTR_COLUMN, &attr_text);
  if ( attr_text != NULL )
  {
    /* check directory attribute */
    if ( strchr(attr_text, 'D') != NULL )
    {
      ret = 1;
    }
    else
    {
      if ( strchr(attr_text, 'd') != NULL )
      {
        ret = 1;
      }
    }
  }
  return ret;
}

static void share_window_set_title (share_window_data *data)
{
  int title_len = SHARE_WINDOW_MAX_TITLE_LEN;
  char title[SHARE_WINDOW_MAX_TITLE_LEN + 1];

  strcpy(title, "//");
  string_ncat(title, data->share.group, title_len);
  string_ncat(title, "/", title_len);
  string_ncat(title, data->share.machine, title_len);

  gtk_window_set_title(GTK_WINDOW(data->window), title);
}

static void share_window_set_edit (share_window_data *data)
{
  int edit_len = SHARE_WINDOW_MAX_EDIT_LEN;
  char edit_text[SHARE_WINDOW_MAX_EDIT_LEN + 1];
  
  strcpy(edit_text, "./");
  string_ncat(edit_text, data->share.share, edit_len);
  if ( !is_empty_string(data->share.path) )
  {
    string_ncat(edit_text, "/", edit_len);
    string_ncat(edit_text, data->share.path, edit_len);
  }
  gtk_entry_set_text(GTK_ENTRY(data->edit), edit_text);
}

/*
 *  0 = use the same window
 *  1 = show in new window
 */
static unsigned char show_new_window (share_window_data *data)
{
  if ( data == NULL )
    return 1;
  
  if ( data->keyStrg == 0 )
    return 1;
  else
    return 0;
}

static void share_window_browse_path (GtkWidget *window, char *path)
{
  share_window_struct sw_struct;
  share_window_data *data;
  sharewin_struct *sharewin;
  GtkWidget *share_window;
  
  if ( window == NULL )
    return;
  if ( path == NULL )
    return;
  
  data = window_data_get(window);

  if ( data != NULL )
  {
    sw_struct = data->share;
    string_ncopy(sw_struct.path, path, MAXSHAREPATHL);

    if ( show_new_window(data) )
    {
      /* show browse in new window */
      share_window = share_window_new(&sw_struct);

      /* set the window cursor */
      if ( share_window != NULL )
      {
        change_win_cursor(share_window->window, GDK_WATCH);
      }
    }
    else
    {
      /* use the old window */
      gtk_clist_clear(GTK_CLIST(data->clist));

      string_ncopy(data->share.path, sw_struct.path, MAXSHAREPATHL);
      share_window_set_title(data);
      share_window_set_edit(data);

      /* change the entry in the window list */
      sharewin = share_window_get_struct_by_window(window);
      if ( sharewin != NULL )
      {
        string_ncopy(sharewin->path, sw_struct.path, MAXSHAREPATHL);
      }
      
      /* change the window cursor */
      change_win_cursor(data->window->window, GDK_WATCH);
    }
  }

  /* browse the share */
  browse_path(&sw_struct, sw_struct.mode);
}

static void share_window_browse_dir (GtkWidget *window, char *dir)
{
  share_window_struct sw_struct;
  share_window_data *data;
  char new_path[MAXSHAREPATHL+1];
  
  if ( window == NULL )
    return;
  if ( dir == NULL )
    return;
  
  data = window_data_get(window);
  if ( data != NULL )
  {
    sw_struct = data->share;
    string_ncopy(new_path, sw_struct.path, MAXSHAREPATHL);
    if ( !is_empty_string(new_path) )
      string_ncat(new_path, "/", MAXSHAREPATHL);
    string_ncat(new_path, dir, MAXSHAREPATHL);
    
    share_window_browse_path(window, new_path);
  }
}

/* ---- callbacks + menues ------------------------------------------------- */

static ITEMTYPE share_window_get_item_type (GtkCList *clist, int row)
{
  share_window_data *data;
  unsigned char is_local;
  ITEMTYPE type;
  
  /* extract the 'share_window_data' from the clist */
  data = (share_window_data*)gtk_clist_get_row_data(clist, row);
  
  if ( data != NULL )
  {
    is_local = share_window_is_local_file( &(data->share) );

    if ( row_is_directory(clist, row) )
    {
      /* item is a directory */
      if ( is_local )
        type = TYPE_LOCAL_DIRECTORY;
      else
        type = TYPE_SMB_DIRECTORY;
    }
    else
    {
      /* item is a file */
      if ( is_local )
        type = TYPE_LOCAL_FILE;
      else
        type = TYPE_SMB_FILE;
    }
  }
  else
  {
    /* no data pointer found */
    type = TYPE_NONE;
  }
  
  return type;
}

/*
 *  callbacks for copying a smb file to a local file
 *
 */

static void dialog_smb_file_exists_ok_callback (GtkWidget *widget, gpointer data)
{
  dialog_pointer_struct *d_struct;
  
  d_struct = (dialog_pointer_struct*)data;
  
  if ( d_struct != NULL )
  {
    /* ok, overwrite file */
    
    gtk_widget_destroy( GTK_WIDGET(d_struct->dialog) );
  }
}

static GtkWidget *dialog_filew;

static gboolean dialog_smb_window_destroy (GtkWidget *window)
{
  share_file_struct *param;

  if ( window != NULL )
  {
    /* destroy the 'share_dialog_struct' */
    param = (share_file_struct*)gtk_object_get_data(GTK_OBJECT(window), SMB_FILESELECT_DATA);
    
    if ( param != NULL )
    {
      g_free(param);
    }
  }
  
  return FALSE;
}

static void dialog_smb_file_copy_from_ok_callback (GtkWidget *window)
{
  share_file_struct *file;
  const char *filename;

  file = (share_file_struct*)gtk_object_get_data(GTK_OBJECT(window), SMB_FILESELECT_DATA);
  
  if ( file != NULL )
  {
    /* extract file name */
    filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(window));
    
    if ( filename != NULL )
    {
      /* checks whether the file exists on the local file system */
      if ( file_exist(filename) )
      {
        /* file does exist, overwrite it ? */
        dialog_file_overwrite(GTK_SIGNAL_FUNC(dialog_smb_file_exists_ok_callback), NULL, file, sizeof(share_file_struct));
      }
      else
      {
#ifndef NO_LIBSMB
        share_window_copy_smb_to_local(share_file_struct);
#endif
      }
    }
  }

  gtk_widget_destroy(window);
}

static void dialog_smb_file_copy_from_cancel_callback (GtkWidget *window)
{
  gtk_widget_destroy(window);
}

static void dialog_smb_file_copy_from (GtkWidget *widget, gpointer data)
{
  share_file_struct *file;
  
  /* extract file name */
  file = (share_file_struct*)data;
  
  if ( file == NULL )
    return;
  
  dialog_filew = gtk_file_selection_new(_("Copy SMB File To"));
  
  gtk_window_set_modal(GTK_WINDOW(dialog_filew), TRUE);
  gtk_file_selection_set_filename(GTK_FILE_SELECTION(dialog_filew), file->filename);

  gtk_object_set_data(GTK_OBJECT(dialog_filew), SMB_FILESELECT_DATA, file);

  gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(dialog_filew)->ok_button),
           "clicked", (GtkSignalFunc)dialog_smb_file_copy_from_ok_callback, GTK_OBJECT(dialog_filew));
  gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(dialog_filew)->cancel_button),
           "clicked", (GtkSignalFunc)dialog_smb_file_copy_from_cancel_callback, GTK_OBJECT(dialog_filew));
  gtk_signal_connect(GTK_OBJECT(dialog_filew), "destroy",
                          GTK_SIGNAL_FUNC(dialog_smb_window_destroy), dialog_filew);

  gtk_widget_show(dialog_filew);
}

/* ---------------------- */

static char c_smb_file_popup[1][64] = { "/" };

static GtkItemFactoryEntry smb_file_popup[] =
{
  {c_smb_file_popup[0], NULL, dialog_smb_file_copy_from, 0, NULL}
};

static void popup_menu_initialize (void)
{
  string_ncat(c_smb_file_popup[0], _("Copy SMB file to ..."), 63);    /* copy from smb -> local */
}

/* ---------------------- */

static gint callback_clist_button_press (GtkWidget *widget, GdkEventButton *event)
{
  static guint32 last_press_time = 0;
  int mouse_event;
  int row = 0;
  int column = 0;
  int x, y;
  char *dir;
  GtkWidget *window;
  
  mouse_event = scan_mouse_press(event, &last_press_time);
  
  switch ( mouse_event )
  {
    case MOUSE_LEFT_DOUBLE:
        
        /* retrieve selected row */
        x = event->x;
        y = event->y;
        if ( GTK_IS_CLIST(widget) )
        {
          if ( gtk_clist_get_selection_info(GTK_CLIST(widget), x, y, &row, &column) )
          {
            /* if selected row is 'directory row' then start new browse */
            if ( row_is_directory(GTK_CLIST(widget), row) )
            {
              dir = row_get_filename(GTK_CLIST(widget), row);
              if ( dir != NULL )
              {
                window = share_window_get_window_by_clist(GTK_CLIST(widget), 1);
                share_window_browse_dir(window, dir);
              }
            }
          }
        }
        
        break;
  }
  return TRUE;
}

static gint callback_clist_button_release (GtkWidget *widget, GdkEventButton *event)
{
  static guint32 last_press_time = 0;
  int mouse_event;
  int row = 0;
  int column = 0;
  int x, y;
  ITEMTYPE item_type;
  int pop_items;
  char *filename;
  GdkPixmap *pixmap;
  GdkBitmap *mask;
  share_file_struct *file;
  share_window_data *data;
  
  mouse_event = scan_mouse_press(event, &last_press_time);
  
  switch ( mouse_event )
  {
    case MOUSE_RIGHT_SINGLE:
        
        /* retrieve selected row */
        x = event->x;
        y = event->y;
        if ( GTK_IS_CLIST(widget) )
        {
          if ( gtk_clist_get_selection_info( GTK_CLIST(widget), x, y, &row, &column) )
          {
            /* extract the 'share_window_data' from the clist */
            data = (share_window_data*)gtk_clist_get_row_data(GTK_CLIST(widget), row);

            item_type = share_window_get_item_type(GTK_CLIST(widget), row);
            /* get the file-name/directory-name of the item */
            gtk_clist_get_pixtext(GTK_CLIST(widget), row, 0, &filename, 0, &pixmap, &mask);
            
            /* setup 'share_file_struct' */
            file = (share_file_struct*)g_malloc( sizeof(share_file_struct) );
            
            if ( data != NULL )
            {
              strncpy(file->group, data->share.group, MAXGROUPNAMEL);
              strncpy(file->machine, data->share.machine, MAXMACHNAMEL);
              strncpy(file->share, data->share.share, MAXSHRNAMEL);
              strncpy(file->path, data->share.path, MAXSHAREPATHL);
              strncpy(file->user, data->share.user, USER_LEN);
              strncpy(file->password, data->share.password, PASSWORD_LEN);
            }
            
            strncpy(file->filename, filename, MAXFILEL);
            
            /* show popup menues */
            
            switch ( item_type )
            {
              case TYPE_SMB_FILE:
                  pop_items = sizeof(smb_file_popup) / sizeof(smb_file_popup[0]);
                  
                  /* set filename for callback function */
                  smb_file_popup[0].callback_action = (guint)file;
                  
                  show_popup_menu(smb_file_popup, pop_items, event->x_root, event->y_root);
                  break;
              
              default:
                  g_free(file);
                  break;
            }
          }
        }
        
        break;
  }
  return TRUE;
}

static gint callback_clist_key_press (GtkWidget *widget, GdkEventKey *event)
{
  share_window_data *data;

  /* is source the share window ? */
  if ( GTK_IS_WINDOW(widget) )
  {
    /* get the window data */
    data = window_data_get(widget);
    if ( data != NULL )
    {
      if ( event->keyval == GDK_Control_L )
      {
        data->keyStrg = 1;
      }
    }
  }
  
  return 0;
}

static gint callback_clist_key_release (GtkWidget *widget, GdkEventKey *event)
{
  share_window_data *data;

  /* is source the share window ? */
  if ( GTK_IS_WINDOW(widget) )
  {
    /* get the window data */
    data = window_data_get(widget);
    if ( data != NULL )
    {
      if ( event->keyval == GDK_Control_L )
      {
        data->keyStrg = 0;
      }
    }
  }
  
  return 0;
}

static void callback_folder_up (GtkWidget *widget, gpointer data)
{
  sharewin_struct *win;
  GtkWidget *window;
  char *slash;
  char new_path[MAXSHAREPATHL+1];
  
  window = (GtkWidget*)data;
  if ( window != NULL )
  {
    win = share_window_get_struct_by_window(window);
    if ( win != NULL )
    {
      if ( !is_empty_string(win->path) )
      {
        string_ncopy(new_path, win->path, MAXSHAREPATHL);
        slash = strrchr(new_path, '/');
        if ( slash != NULL )
        {
          *slash = 0;
        }
        else
        {
          new_path[0] = 0;
        }
        
        share_window_browse_path(win->window, new_path);
      }
    }
  }
}

/*static void callback_folder_rescan (GtkWidget *widget, gpointer data)
{
}*/

static void callback_folder_cancel (GtkWidget *widget, gpointer data)
{
  /* stop all browsing action */
  callback_stop(NULL, NULL);
}

static void share_status_set_object_text (GtkWidget *status, int context_id, guint objects)
{
  char text[32];
  
  if ( status != NULL )
  {
    sprintf(text, " ");    
    sprintf(text+1, _("%i Object(s)"), objects);
    gtk_statusbar_pop(GTK_STATUSBAR(status), context_id);
    gtk_statusbar_push(GTK_STATUSBAR(status), context_id, text);
  }
}

static void share_status_set_k_size (GtkWidget *status, int context_id, guint64 sum, guint objects)
{
  char text[64];
  
  if ( status != NULL )
  {
    sprintf(text, " ");    
    sprintf(text+1, _("%Luk in %u file(s)"), sum, objects);
    gtk_statusbar_pop(GTK_STATUSBAR(status), context_id);
    gtk_statusbar_push(GTK_STATUSBAR(status), context_id, text);
  }
}

/* ------------------------------------------------------------------------- */

/*
 * creates the main menu of a share window
 */

static void create_menu (GtkWidget *window, GtkWidget *vbox)
{
  GtkWidget *menu_bar;
  GtkWidget *main_menu;
  GtkWidget *main_item;
  GtkWidget *menu_item;
  
  /* create menu bar */
  menu_bar = gtk_menu_bar_new();
  gtk_widget_set_usize(menu_bar, -1, 25);
  gtk_box_pack_start(GTK_BOX(vbox), menu_bar, FALSE, TRUE, 0);
  
  /* File menu */
  main_item = gtk_menu_item_new_with_label(_("File"));
  main_menu = gtk_menu_new();

  menu_item = gtk_menu_item_new_with_label(_("Exit"));
  gtk_menu_append( GTK_MENU(main_menu), menu_item);
  gtk_signal_connect_object( GTK_OBJECT(menu_item), "activate",
                             GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(window));

  gtk_menu_item_set_submenu( GTK_MENU_ITEM(main_item), main_menu);
  gtk_menu_bar_append( GTK_MENU_BAR(menu_bar), main_item);
}

/*
 * creates the toolbar of a share window
 */

static void create_toolbar (GtkWidget *vbox, share_window_data *data)
{
  GtkWidget *hbox;
  GtkWidget *toolbar;
  GtkWidget *icon;
  
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
  
  data->edit = gtk_entry_new_with_max_length(SHARE_WINDOW_MAX_EDIT_LEN);
  gtk_widget_set_usize(data->edit, 250, -1);
  gtk_entry_set_editable(GTK_ENTRY(data->edit), 0);
  gtk_box_pack_start(GTK_BOX(hbox), data->edit, FALSE, FALSE, 0);
  
  toolbar = gtk_toolbar_new();
  gtk_toolbar_set_orientation(GTK_TOOLBAR(toolbar), GTK_ORIENTATION_HORIZONTAL);
  gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH);

  gtk_box_pack_start(GTK_BOX(hbox), toolbar, FALSE, TRUE, 0);
  
  gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));
  
  icon = get_icon(ICON_TOOLBAR_UP);
  gtk_toolbar_append_item( GTK_TOOLBAR(toolbar), NULL, _("Folder Up"), "",
                      icon, GTK_SIGNAL_FUNC(callback_folder_up), (gpointer)(data->window));
  
  gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));
  
  icon = get_icon(ICON_TOOLBAR_CANCEL);
  gtk_toolbar_append_item( GTK_TOOLBAR(toolbar), NULL, _("Cancel"), "",
                      icon, GTK_SIGNAL_FUNC(callback_folder_cancel), (gpointer)(data->window));
  
  gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));
  
  icon = get_icon(ICON_TOOLBAR_DELETE);
  gtk_toolbar_append_item( GTK_TOOLBAR(toolbar), NULL, _("Delete Selected Files"), "",
                      icon, NULL, NULL);
  
  icon = get_icon(ICON_TOOLBAR_COPY);
  gtk_toolbar_append_item( GTK_TOOLBAR(toolbar), NULL, _("Copy Selected File To"), "",
                      icon, NULL, NULL);
  
  icon = get_icon(ICON_TOOLBAR_INSERT);
  gtk_toolbar_append_item( GTK_TOOLBAR(toolbar), NULL, _("Move Selected File To"), "",
                      icon, NULL, NULL);
  
  gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));
  
  icon = get_icon(ICON_TOOLBAR_PREFS);
  gtk_toolbar_append_item( GTK_TOOLBAR(toolbar), NULL, "Preferences", "",
                      icon, NULL, NULL);

  set_toolbar_tooltip_color(GTK_TOOLBAR(toolbar));
}

/*
 * create the status line of the share window
 */

static void create_status (GtkWidget *vbox, share_window_data *data)
{
  GtkWidget *hbox;
  
  data->object_status = gtk_statusbar_new();
  data->object_context_id = gtk_statusbar_get_context_id(GTK_STATUSBAR(data->object_status),
                                        "LinNeighborhood ShareStatus");
  data->size_status = gtk_statusbar_new();
  data->size_context_id = gtk_statusbar_get_context_id(GTK_STATUSBAR(data->size_status),
                                        "LinNeighborhood ShareStatus");
  /* size of the status' object field */
  gtk_widget_set_usize(data->object_status, 130, -1);
  
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
  
  gtk_box_pack_start(GTK_BOX(hbox), data->object_status, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), data->size_status, TRUE, TRUE, 0);
}

/*
 * creates the CList containing the file entries
 */

static void create_filelist (GtkWidget *vbox, share_window_data *data)
{
  GtkWidget *scrolled_win;
  int i;

  static char *titles[SHARE_CLIST_COLUMNS] =
  {
    "Name", "Size ", "Date", "Time", "Attr"
  };
  
  scrolled_win = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  
  data->clist = gtk_clist_new_with_titles(SHARE_CLIST_COLUMNS, titles);
  gtk_clist_column_titles_passive(GTK_CLIST(data->clist));
  /*gtk_clist_set_selection_mode(GTK_CLIST(data->clist), GTK_SELECTION_BROWSE);*/
  gtk_clist_set_column_justification(GTK_CLIST(data->clist), 1, GTK_JUSTIFY_RIGHT);
  
  gtk_clist_set_column_width(GTK_CLIST(data->clist), 0, data->name_width);
  gtk_clist_set_column_width(GTK_CLIST(data->clist), 1, data->size_width);
  gtk_clist_set_column_width(GTK_CLIST(data->clist), 2, data->date_width);
  gtk_clist_set_column_width(GTK_CLIST(data->clist), 3, data->time_width);
  gtk_clist_set_column_width(GTK_CLIST(data->clist), 4, data->attr_width);
  for ( i = 0; i < SHARE_CLIST_COLUMNS; i++ )
  {
    gtk_clist_set_column_min_width(GTK_CLIST(data->clist), i, 40);
  }
  /* set minimum row height */
  if ( GTK_CLIST(data->clist)->row_height < FILE_ICON_HEIGHT )
    gtk_clist_set_row_height(GTK_CLIST(data->clist), FILE_ICON_HEIGHT);
  /* set mouse handler */
  gtk_signal_connect( GTK_OBJECT(data->clist), "button_press_event",
                      GTK_SIGNAL_FUNC(callback_clist_button_press), NULL);
  gtk_signal_connect( GTK_OBJECT(data->clist), "button_release_event",
                      GTK_SIGNAL_FUNC(callback_clist_button_release), NULL);
  
  gtk_container_add(GTK_CONTAINER(scrolled_win), data->clist);
  
  gtk_box_pack_start(GTK_BOX(vbox), scrolled_win, TRUE, TRUE, 0);
}

/* ------------------------------------------------------------------------- */

/* "destructor", use for garbage collection */

static gboolean cb_share_window_destroy (GtkWidget *widget)
{
  share_window_data *data;
  
  /* remove window from list */
  share_window_remove_by_window(widget);
  
  /* free window data struct */
  data = window_data_get(widget);
  if ( data != NULL )
  {
    window_data_delete(data);
  }

  return FALSE;
}

GtkWidget *share_window_new (share_window_struct *sw_struct)
{
  share_window_data *data;
  GtkWidget *vbox;
  GtkWidget *sh_window;
  
  if ( !share_window_initialized )
  {
    popup_menu_initialize();
    share_window_initialized = 1;
  }
  
  /* does a window pointing to this path already exists ? */
  sh_window = share_window_already_exists(sw_struct);
  
  if ( sh_window == NULL )
  {
    /* create window data struct */
    data = (share_window_data*)g_malloc(sizeof(share_window_data));
    if ( !data )
      return NULL;

    window_data_initialize(data);
    if ( sw_struct != NULL )
    {
      memcpy(&(data->share), sw_struct, sizeof(share_window_struct));
    }

    /* create frame window */
    data->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    if ( data->window != NULL )
    {
      gtk_object_set_data(GTK_OBJECT(data->window), WINDOW_DATA, data);

      gtk_window_set_default_size(GTK_WINDOW(data->window),
                  SHARE_WINDOW_DEF_WIDTH, SHARE_WINDOW_DEF_HEIGHT);
      gtk_container_border_width (GTK_CONTAINER(data->window), 0);

      /* set frame title */
      share_window_set_title(data);

      /* connect signals */
      gtk_signal_connect(GTK_OBJECT(data->window), "destroy",
                          GTK_SIGNAL_FUNC(cb_share_window_destroy), data->window);

      vbox = gtk_vbox_new(FALSE, 0);
      gtk_container_add(GTK_CONTAINER(data->window), vbox);

      /* create window widgets */
      create_menu(data->window, vbox);
      create_toolbar(vbox, data);
      create_status(vbox, data);
      create_filelist(vbox, data);
      
      /* set edit field text */
      share_window_set_edit(data);

      /* set keyboard handler */
      gtk_signal_connect( GTK_OBJECT(data->window), "key_press_event",
                          GTK_SIGNAL_FUNC(callback_clist_key_press), NULL);
      gtk_signal_connect( GTK_OBJECT(data->window), "key_release_event",
                          GTK_SIGNAL_FUNC(callback_clist_key_release), NULL);

      /* show all window widgets */
      gtk_widget_show_all(data->window);

      /* set window icon */
      if ( !pixmap_window_icon )
      {
        pixmap_window_icon = gdk_pixmap_create_from_xpm_d (data->window->window, &mask_window_icon,
                     &(data->window)->style->bg[GTK_STATE_NORMAL], (gchar **)ShareWindow_xpm);
      }
      gdk_window_set_icon(data->window->window, NULL, pixmap_window_icon, mask_window_icon);

      /* add window to window list */
      share_window_add_window(data->window, GTK_CLIST(data->clist), sw_struct);

      return data->window;

    }
    else
    {
      /* error, free memory */
      g_free(data);
      return NULL;
    }
  }
  else
  {
    /* +++ window already exists +++ */
    
    /* put window to foreground */
    gdk_window_raise( sh_window->window );
    
    return sh_window;
  }
}

/* ---------------------------------------------------------------------------
 * checks whether the window is an local file system browser
 *
 * return value :  0 -> smb file system browser
 *                !0 -> local file system browser
 * ---------------------------------------------------------------------------
 */
 
unsigned char share_window_is_local_file (share_window_struct *sw_struct)
{
  unsigned char ret_value = 0;
  
  if ( sw_struct != NULL )
  {
    if ( sw_struct->mode & BROWSE_MODE_FILE_SYSTEM )
    {
      ret_value = 1;
    }
  }
  
  return ret_value;
}

/* ---------------------------------------------------------------------------
 * checks whether there's an existing window to the requested path
 *
 * return value :  if a window pointing to path 'xxx' already exists,
 *                 the function returns its pointer, otherwise NULL
 * ---------------------------------------------------------------------------
 */

GtkWidget *share_window_already_exists (share_window_struct *sw_struct)
{
  if ( sw_struct == NULL )
    return NULL;
  
  if ( !share_window_is_local_file(sw_struct) )
  {
    /* smb browser */
    return share_window_get_window( sw_struct->group,
                                    sw_struct->machine,
                                    sw_struct->share,
                                    sw_struct->path,
                                    1 );
  }
  else
  {
    /* local browser */
    return share_window_get_local_window( sw_struct->path, 1);
  }
}

/* ------------------------------------------------------------------------- */

/*
 * adds a list of files to the share window
 */

void share_window_add_files (GtkWidget *window, GSList *files)
{
  share_window_data *data;
  GtkCList *clist = NULL;
  char *text[5];
  char size[22];
  char date[11];
  char time[6];
  char attr[FILE_ATTR_NUMBER+1];
  guint object_num;
  guint file_num;
  guint64 sum, tail;
  GdkPixmap *pixmap = NULL;
  GdkBitmap *mask = NULL;
  GSList *templist;
  file_struct *file;
  gint row;
  
  if ( window == NULL )
    return;
  
  data = window_data_get(window);
  if ( data != NULL )
  {
    clist = GTK_CLIST(data->clist);
  }
  if ( clist == NULL )
    return;
  
  gtk_clist_clear(clist);
  
  /* speed up inserting of the rows */
  gtk_clist_freeze(clist);
  
  object_num = 0;
  file_num = 0;
  sum = 0;
  
  templist = files;
  
  while ( templist != NULL )
  {
    file = (file_struct*)(templist->data);
    
    if ( file != NULL )
    {
      sprintf(size, "%Lu", file->size);
      sprintf(date, "%02u:%02u:%4hu", file->day, file->month, file->year);
      sprintf(time, "%02u:%02u", file->minute, file->hour);
      attr[0] = 0;
      if ( file->attr & FILE_ATTR_DIRECTORY )
        strcat(attr, "D");
      if ( file->attr & FILE_ATTR_HIDDEN )
        strcat(attr, "H");
      if ( file->attr & FILE_ATTR_READONLY )
        strcat(attr, "R");
      if ( file->attr & FILE_ATTR_ARCHIVE )
        strcat(attr, "A");

      text[0] = "";
      text[1] = size;
      text[2] = date;
      text[3] = time;
      text[4] = attr;

      if ( file->attr & FILE_ATTR_DIRECTORY )
      {
        fetch_file_icon(ICON_FILE_DIRECTORY, &pixmap, &mask);
        text[1] = "";
      }
      else
      {
        fetch_file_icon_by_name(file->name, &pixmap, &mask);
        file_num++;
        sum += file->size;
      }
      row = gtk_clist_append(clist, text);
      /* set the pointer to 'share_window_data' as row data pointer, you need it later */
      gtk_clist_set_row_data(clist, row, data);
      
      gtk_clist_set_pixtext(clist, object_num, 0, file->name, 0, pixmap, mask);

      object_num++;
    }
    
    templist = templist->next;
  }

  /* draw the clist */
  gtk_clist_thaw(clist);
  
  share_status_set_object_text (data->object_status, data->object_context_id, object_num);
  tail = sum % 1024;
  sum /= 1024;
  if ( tail )
    sum++;
  share_status_set_k_size (data->size_status, data->size_context_id, sum, file_num);
  
  /* set the window cursor to normal */
  change_win_cursor(data->window->window, GDK_TOP_LEFT_ARROW);
}

void share_window_copy_smb_to_local(share_file_struct f_struct)
{
#ifndef NO_LIBSMB
  CopySMBToLocal (f_struct->group, f_struct->machine, f_struct->share, f_struct->path,
               f_struct->filename, f_struct->user, f_struct->password, copy_file_callback  callback);
#endif
}

/* ------------------------------------------------------------------------- */

/*
 *    there's a list where all share windows are insertet with
 *    a structure containing the share path of the convention
 *    '//group/machine/any/path/of/machine' and the matching
 *    widget pointer to the window where the share is viewed.
 *    We need this list to find the matching window of
 *    a network share.
 */

static GSList *sharewinlist = (GSList*)NULL;    /* list of share windows */

/* delete the linked list */

void share_window_clean (void)
{
  slist_free_with_data(&sharewinlist);
}

/* search the window widget of a share path */

GtkWidget *share_window_get_window (const char *group, const char *machine, const char *share,
                                    const char *path, unsigned char reset)
{
  static GSList *templist = NULL;
  sharewin_struct *sharewin;
  GtkWidget *win;
  
  win = NULL;
  if ( reset == 1 )
    templist = sharewinlist;
  
  while ( templist != NULL )
  {
    sharewin = (sharewin_struct*)(templist->data);
    if ( sharewin != NULL )
    {
      if ( (is_empty_string(group)) || (compare_smb_groupname(group, sharewin->group)) )
      {
        if ( compare_smb_machinename(machine, sharewin->machine) )
        {
          if ( compare_smb_sharename(share, sharewin->share) )
          {
            if ( compare_path(path, sharewin->path) )
            {
              /* entry found in list */
              win = sharewin->window;
              templist = templist->next;
              break;
            }
          }
        }
      }
    }
    templist = templist->next;
  }
  return win;
}

GtkWidget *share_window_get_window_by_clist (GtkCList *clist, unsigned char reset)
{
  static GSList *templist = NULL;
  sharewin_struct *sharewin;
  GtkWidget *win;
  
  win = NULL;
  if ( reset == 1 )
    templist = sharewinlist;
  
  while ( templist != NULL )
  {
    sharewin = (sharewin_struct*)(templist->data);
    if ( sharewin != NULL )
    {
      if ( clist == sharewin->clist )
      {
        /* entry found in list */
        win = sharewin->window;
        templist = templist->next;
        break;
      }
    }
    templist = templist->next;
  }
  return win;
}

/* local file system version */
GtkWidget *share_window_get_local_window (const char *path, unsigned char reset)
{
  static GSList *templist = NULL;
  sharewin_struct *sharewin;
  GtkWidget *win;
  
  win = NULL;
  if ( reset == 1 )
    templist = sharewinlist;
  
  while ( templist != NULL )
  {
    sharewin = (sharewin_struct*)(templist->data);
    if ( sharewin != NULL )
    {
      if ( compare_path(path, sharewin->path) )
      {
        /* entry found in list */
        win = sharewin->window;
        templist = templist->next;
        break;
      }
    }
    templist = templist->next;
  }
  return win;
}

static sharewin_struct *share_window_get_struct_by_window (GtkWidget *window)
{
  GSList *templist;
  sharewin_struct *win;
  sharewin_struct *ret = NULL;
  
  templist = sharewinlist;
  
  while ( templist != NULL )
  {
    win = (sharewin_struct*)(templist->data);
    if ( win != NULL )
    {
      if ( window == win->window )
        ret = win;
    }
    templist = templist->next;
  }
  return ret;
}

static void share_window_add_window (GtkWidget *window, GtkCList *clist, share_window_struct *ws)
{
  sharewin_struct *sharewin;
  
  if ( ws == NULL )
    return;
  
  sharewin = (sharewin_struct*)g_malloc(sizeof(sharewin_struct));
  if ( sharewin != NULL )
  {
    sharewin->window = window;
    sharewin->clist = clist;
    string_ncopy(sharewin->group, ws->group, MAXGROUPNAMEL);
    string_ncopy(sharewin->machine, ws->machine, MAXMACHNAMEL);
    string_ncopy(sharewin->share, ws->share, MAXSHRNAMEL);
    string_ncopy(sharewin->path, ws->path, MAXSHAREPATHL);
    sharewinlist = g_slist_append(sharewinlist, sharewin);
  }
}

static void share_window_remove_by_window (GtkWidget *window)
{
  GSList *templist;
  sharewin_struct *sharewin;
  gpointer data;
  
  templist = sharewinlist;
  
  while ( templist != NULL )
  {
    sharewin = (sharewin_struct*)(templist->data);
    if ( sharewin != NULL )
    {
      if ( window == sharewin->window )
      {
        data = templist->data;
        sharewinlist = g_slist_remove(sharewinlist, templist->data);
        g_free(data);
        break;
      }
    }
    templist = templist->next;
  }
}

share_window_struct *share_window_get_struct (GtkWidget *window)
{
  if ( GTK_IS_WINDOW(window) )
  {
  }
  
  return NULL;
}

/* ======================================== */
/*  change the cursor in all share windows  */
/* ======================================== */

void share_window_list_set_cursor (GdkCursorType cursortype)
{
  GSList *templist;
  sharewin_struct *sharewin;
  
  templist = sharewinlist;
  
  while ( templist != NULL )
  {
    sharewin = (sharewin_struct*)(templist->data);
    if ( sharewin != NULL )
    {
      change_win_cursor(sharewin->window->window, cursortype);
    }
    templist = templist->next;
  }
}

/* ------------------------------------------------------------------------- */
