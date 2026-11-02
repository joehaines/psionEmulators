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

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/wait.h>
#include <gtk/gtk.h>
#include "gtk_tree.h"
#include "gtk_gui.h"
#include "gtk_dialog.h"
#include "preferences.h"
#include "data.h"
#include "browsewrap.h"
#include "utility.h"
#include "guiwrap.h"
#include "smbif.h"
#include "gtk_sharewindow.h"
#include "libsmb.h"

/* -- tree symbols -- */
#include "picture/root.xpm"
#include "picture/group.xpm"
#include "picture/machine.xpm"
#include "picture/folder.xpm"
#include "picture/open_folder.xpm"
#include "picture/printer.xpm"


/* -- Pixmaps for tree icons -- */
static GdkPixmap *root_pixmap = NULL;
static GdkPixmap *group_pixmap = NULL;
static GdkPixmap *machine_pixmap = NULL;
static GdkPixmap *folder_pixmap = NULL;
static GdkPixmap *open_folder_pixmap = NULL;
static GdkPixmap *printer_pixmap = NULL;

static GdkBitmap *root_mask;
static GdkBitmap *group_mask;
static GdkBitmap *machine_mask;
static GdkBitmap *folder_mask;
static GdkBitmap *open_folder_mask;
static GdkBitmap *printer_mask;

#define HOST_LEN           128

#define MIN_TREE_SIZE      100     /* minimum height of tree */
#define USIZE_SIZE         5       /* inset of an column of the tree */

/* left of the name label */
#define GROUP_INSET            49
#define MACHINE_INSET          58
#define PREF_MACHINE_INSET     40
#define SHARE_INSET            67

/* minimum allowed width of a column */
#define COLUMN_0_MIN_WIDTH 120
#define COLUMN_1_MIN_WIDTH 120
/* maximum ( from the right ) width of a column */
#define COLUMN_0_MAX_WIDTH 30
#define COLUMN_1_MAX_WIDTH 30

#define COLUMN_INSET       3

/* -- global recommended variables -- */

unsigned char is_password_once = 0;
GtkTreeItem *clicked_item = NULL;          /* currently selected tree item */

/* local variables */
static GtkWidget *root_tree;               /* neihborhood root tree */
static GtkWidget *root_item = NULL;        /* global available neighborhood root item */
static GtkWidget *vpaned;                  /* tailed window for resizing */
static GtkWidget *scrolled_box;
static GtkWidget *scrolled_win;
static GtkWidget *clist_win;               /* scrolled window for clist */
static GtkWidget *mount_clist;             /* mount-list */
static GtkWidget *scroller;

/* ------------------------------------- */
/* -- declaration of static functions -- */
/* ------------------------------------- */

/* workgroup */
static void group_retrigger ();
/* machine */
static void machine_retrigger (data_notify_struct *notify);
static void machine_get_name_and_group (GtkTreeItem *item, char **group, char **machine);
static machine_struct *machine_get_data_by_item (GtkTreeItem *item);
/* share */
static GtkTreeItem *share_create_tree_item (char *name, char *comment, SHARE_TYPE st, char *mpoint);
#ifndef NO_SMBMOUNT
static void share_get_mountpoint (GtkTreeItem *item, char **mountpoint);
static share_struct *share_get_data_by_item (GtkTreeItem *item);
#endif
/* host */
static void host_get_name (GtkTreeItem *item, char **name);
static host_struct *host_get_data_by_item (GtkTreeItem *item);
static GtkTreeItem *host_get_by_name (const char *host);
/* mount */
#ifndef NO_SMBMOUNT
static void mount_retrigger ();
#endif
/* tree */
static GtkTreeItem *tree_item_get_parent_item (GtkTreeItem *item);
static unsigned char tree_check_domain_browse (void);
static void tree_browse_action (GtkTreeItem *item, BROWSE_MODE mode);
#ifndef NO_SMBMOUNT
static void tree_mount_action (GtkTreeItem *item, unsigned char show_gui);
#endif
static ITEMTYPE tree_get_item_type ( GtkTreeItem * item );
static unsigned int tree_item_count (GtkTreeItem *item);
static GtkTreeItem *tree_iterate (void);
static void tree_reset_iteration (void);
static void tree_move_scrollwindow (GtkTreeItem *item);

/* ------------------------------------------------------------------------ */

/* ----------------------------- */
/* -- popup menus + callbacks -- */
/* ----------------------------- */

static void callback_rescan (GtkWidget *widget, gpointer data)
{
  BROWSE_MODE mode;

  mode = (BROWSE_MODE)data;

  if ( GTK_IS_TREE_ITEM(clicked_item) )
  {
    if ( mode & BROWSE_MODE_USER )
    {
      if ( tree_check_domain_browse() )
      {
        mode |= BROWSE_MODE_DOMAIN;
      }
      user_browse_dialog( ((mode & BROWSE_MODE_DOMAIN) ? 1 : 0) );
    }
    else
    {
      tree_scan_item(mode);
    }
  }
}

static void callback_host_information (GtkWidget *widget, gpointer data)
{
  MACHINEINFO_STRUCT machineinfo;
  host_struct *h_struct;
  
  h_struct = host_get_data_by_item(clicked_item);

  if ( h_struct != NULL )
  {
    string_ncopy(machineinfo.ipaddr, h_struct->ipaddr, MAXIPL);
    string_ncopy(machineinfo.Domain, h_struct->group, MAXGROUPNAMEL);
    string_ncopy(machineinfo.OS, h_struct->os, MAXCOMMENTL);
    string_ncopy(machineinfo.Server, h_struct->server, MAXCOMMENTL);

    machine_info_dialog(h_struct->name, &machineinfo);
  }
}

static void callback_machine_information (GtkWidget *widget, gpointer data)
{
  MACHINEINFO_STRUCT machineinfo;
  machine_struct *m_struct;
  group_struct *g_struct;
  char *domain;
  
  domain = cempty;
  m_struct = machine_get_data_by_item(clicked_item);
  
  if ( m_struct != NULL )
  {
    string_ncopy(machineinfo.ipaddr, m_struct->ipaddr, MAXIPL);
    g_struct = m_struct->group;
    if ( g_struct != NULL )
      domain = g_struct->name;
    string_ncopy(machineinfo.Domain, domain, MAXGROUPNAMEL);
    string_ncopy(machineinfo.OS, m_struct->os, MAXCOMMENTL);
    string_ncopy(machineinfo.Server, m_struct->server, MAXCOMMENTL);

    machine_info_dialog(m_struct->name, &machineinfo);
  }
}

static void callback_edit_fav_machine(GtkWidget *widget, gpointer data)
{
  host_struct *h_struct;
  
  h_struct = host_get_data_by_item(clicked_item);

  if ( h_struct != NULL )
  {
    edit_pref_machine(clicked_item, h_struct->name,
                      h_struct->group,
                      h_struct->ipaddr);
  }
}

static void callback_remove_host(GtkWidget *widget, gpointer data)
{
  host_struct *h_struct;
  
  h_struct = host_get_data_by_item(clicked_item);

  if ( h_struct != NULL )
  {
    data_hosts_delete (h_struct->name);
  }
}

void callback_stop (GtkWidget *widget, gpointer data)
{
  /* abort browsing and mounting */
  status_set_status_scan_text(_(" Stopped..."));
  /* clear browse jobs + waiting queue */
  browse_init();
  /* stop all pending actions */
  stop_all_mount();
  /* restore cursor */
  change_win_cursor(main_window->window, GDK_TOP_LEFT_ARROW);
  share_window_list_set_cursor(GDK_TOP_LEFT_ARROW);
}

#ifndef NO_LIBSMB

static void callback_browse (GtkWidget *widget, gpointer data)
{
  ITEMTYPE item_type;
  BROWSE_MODE mode;

  mode = (BROWSE_MODE)data;

  if ( clicked_item == NULL )
    return;
  
  item_type = tree_get_item_type(clicked_item);
  if ( (item_type == TYPE_FOLDER) || (item_type == TYPE_OPEN_FOLDER) )
  {
    if ( mode & BROWSE_MODE_USER )
    {
      user_browse_dialog( 0 );
    }
    else
    {
      tree_browse_action(clicked_item, mode);
    }
  }
}

#endif

#ifndef NO_SMBMOUNT

void callback_mount(GtkWidget *widget, gpointer data)
{
  ITEMTYPE item_type;

  if ( clicked_item == NULL )
    return;
  item_type = tree_get_item_type(clicked_item);
  if ( (item_type == TYPE_FOLDER) || (item_type == TYPE_OPEN_FOLDER) )
  {
    tree_mount_action(clicked_item, 1);
  }
}

void callback_umount(GtkWidget *widget, gpointer data)
{
  char *mountpoint = NULL;

  share_get_mountpoint(clicked_item, &mountpoint);
  if ( !is_empty_string(mountpoint) )
  {
    gui_umount_dialog(mountpoint);
  }
}

static void callback_filemanager (GtkWidget *widget, gpointer data)
{
  char *mountpoint = NULL;

  share_get_mountpoint(clicked_item, &mountpoint);
  if ( !is_empty_string(mountpoint) )
  {
    postmount_execute(mountpoint);
  }
}

static void callback_clist_umount (GtkWidget *widget, gpointer data)
{
  char *mountpoint;
  
  mountpoint = (char*)data;
  gui_umount_dialog(mountpoint);
}

static void callback_clist_filemanager (GtkWidget *widget, gpointer data)
{
  char *mountpoint;
  
  mountpoint = (char*)data;
  postmount_execute(mountpoint);
}

#endif

static char c_root_popup[3][64] = { "/", "/", "/"};

static GtkItemFactoryEntry root_popup[] =
{
  {c_root_popup[0], NULL, callback_rescan, (guint)BROWSE_MODE_NONE, NULL},
  {c_root_popup[1], NULL, callback_rescan, (guint)BROWSE_MODE_USER, NULL},
  {"/sep1", NULL, NULL, 0, "<Separator>"},
  {c_root_popup[2], "<item>", callback_rescan, (guint)BROWSE_MODE_QUICK, NULL}
};

static char c_group_popup[2][64] = { "/", "/"};

static GtkItemFactoryEntry group_popup[] =
{
  {c_group_popup[0], NULL, callback_rescan, (guint)BROWSE_MODE_NONE, NULL},
  {c_group_popup[1], NULL, callback_rescan, (guint)BROWSE_MODE_USER, NULL}
};

static char c_machine_popup[3][64] = { "/", "/", "/"};

static GtkItemFactoryEntry machine_popup[] =
{
  {c_machine_popup[0], NULL, callback_rescan, (guint)BROWSE_MODE_NONE, NULL},
  {c_machine_popup[1], NULL, callback_rescan, (guint)BROWSE_MODE_USER, NULL},
  {"/sep1", NULL, NULL, 0, "<Separator>"},
  {c_machine_popup[2], NULL, callback_machine_information, 0, NULL}
};

static char c_pref_machine_popup[5][64] = { "/", "/", "/", "/", "/"};

static GtkItemFactoryEntry pref_machine_popup[] =
{
  {c_pref_machine_popup[0], NULL, callback_rescan, (guint)BROWSE_MODE_NONE, NULL},
  {c_pref_machine_popup[1], NULL, callback_rescan, (guint)BROWSE_MODE_USER, NULL},
  {"/sep1", NULL, NULL, 0, "<Separator>"},
  {c_pref_machine_popup[2], NULL, callback_edit_fav_machine, 0, NULL},
  {c_pref_machine_popup[3], NULL, callback_remove_host, 0, NULL},
  {"/sep2", NULL, NULL, 0, "<Separator>"},
  {c_pref_machine_popup[4], NULL, callback_host_information, 0, NULL}
};

#ifndef NO_LIBSMB
static char c_folder_popup[3][64] = { "/", "/", "/"};
#else
static char c_folder_popup[1][64] = { "/"};
#endif

static GtkItemFactoryEntry folder_popup[] =
{
#ifndef NO_LIBSMB
  {c_folder_popup[0], NULL, callback_browse, (guint)BROWSE_MODE_NONE, NULL},
  {c_folder_popup[1], NULL, callback_browse, (guint)BROWSE_MODE_USER, NULL},
  {"/sep1", NULL, NULL, 0, "<Separator>"},
  {c_folder_popup[2], NULL, callback_mount, 0, NULL}
#else
  {c_folder_popup[0], NULL, callback_mount, 0, NULL}
#endif
};

#ifndef NO_LIBSMB
static char c_open_folder_popup[5][64] = { "/", "/", "/", "/", "/"};
#else
static char c_open_folder_popup[3][64] = { "/", "/", "/"};
#endif

static GtkItemFactoryEntry open_folder_popup[] =
{
#ifndef NO_LIBSMB
  {c_open_folder_popup[0], NULL, callback_browse, (guint)BROWSE_MODE_NONE, NULL},
  {c_open_folder_popup[1], NULL, callback_browse, (guint)BROWSE_MODE_USER, NULL},
  {"/sep1", NULL, NULL, 0, "<Separator>"},
  {c_open_folder_popup[2], NULL, callback_umount, 0, NULL},
  {c_open_folder_popup[3], NULL, callback_mount, 0, NULL},
  {"/sep1", NULL, NULL, 0, "<Separator>"},
  {c_open_folder_popup[4], NULL, callback_filemanager, 0, NULL}
#else
  {c_open_folder_popup[0], NULL, callback_umount, 0, NULL},
  {c_open_folder_popup[1], NULL, callback_mount, 0, NULL},
  {"/sep1", NULL, NULL, 0, "<Separator>"},
  {c_open_folder_popup[2], NULL, callback_filemanager, 0, NULL}
#endif
};

static char c_clist_popup[3][64] = { "/", "/", "/"};

#ifndef NO_SMBMOUNT

static GtkItemFactoryEntry clist_popup[] =
{
  {c_clist_popup[0], NULL, callback_clist_umount, 0, NULL},
  {"/sep1", NULL, NULL, 0, "<Separator>"},
  {c_clist_popup[1], NULL, callback_clist_filemanager, 0, NULL}
};

#endif

static void popup_menu_initialize (void)
{
  string_ncat(c_root_popup[0], _("rescan groups"), 63);
  string_ncat(c_root_popup[1], _("scan groups as user"), 63);
  string_ncat(c_root_popup[2], _("quick browse"), 63);

  string_ncat(c_group_popup[0], _("rescan group"), 63);
  string_ncat(c_group_popup[1], _("scan group as user"), 63);

  string_ncat(c_machine_popup[0], _("rescan machine"), 63);
  string_ncat(c_machine_popup[1], _("scan as user"), 63);
  string_ncat(c_machine_popup[2], _("show information"), 63);

  string_ncat(c_pref_machine_popup[0], _("rescan machine"), 63);
  string_ncat(c_pref_machine_popup[1], _("scan as user"), 63);
  string_ncat(c_pref_machine_popup[2], _("edit machine"), 63);
  string_ncat(c_pref_machine_popup[3], _("remove machine"), 63);
  string_ncat(c_pref_machine_popup[4], _("show information"), 63);

#ifndef NO_LIBSMB
  string_ncat(c_folder_popup[0], _("browse"), 63);
  string_ncat(c_folder_popup[1], _("browse as user"), 63);
  string_ncat(c_folder_popup[2], _("mount"), 63);
#else
  string_ncat(c_folder_popup[0], _("mount"), 63);
#endif

#ifndef NO_LIBSMB
  string_ncat(c_open_folder_popup[0], _("browse"), 63);
  string_ncat(c_open_folder_popup[1], _("browse as user"), 63);
  string_ncat(c_open_folder_popup[2], _("unmount"), 63);
  string_ncat(c_open_folder_popup[3], _("additional mount"), 63);
  string_ncat(c_open_folder_popup[4], _("file manager"), 63);
#else
  string_ncat(c_open_folder_popup[0], _("unmount"), 63);
  string_ncat(c_open_folder_popup[1], _("additional mount"), 63);
  string_ncat(c_open_folder_popup[2], _("file manager"), 63);
#endif

#ifndef NO_SMBMOUNT
  string_ncat(c_clist_popup[0], _("unmount"), 63);
  string_ncat(c_clist_popup[1], _("file manager"), 63);
#endif
}

/* ------------------------------------------------------------------------ */

/* ---------------------- */
/* -- signal callbacks -- */
/* ---------------------- */

/* -- eliminates old click behaviour of the tree -- */

static gint item_dummy (GtkWidget *widget, GdkEvent *event)
{
  return TRUE;
}

/* -- implements new click behaviour of the tree -- */

/* -- mouse down events -- */

void do_password_scan ()
{
  if ( pref.v.password_once )
  {
    if ( is_password_once )
    {
      /* password once, username + password already known */
      tree_scan_item(BROWSE_MODE_USER);
    }
    else
    {
      /* username + password not known until now */
      user_browse_dialog( tree_check_domain_browse() );
    }
  }
  else
  {
    user_browse_dialog( tree_check_domain_browse() );
  }
}

static gint item_press_event (GtkWidget *widget, GdkEventButton *event,
                                  	gpointer data)
{
  static guint32 last_press_time = 0;
  int mouse_event;
  ITEMTYPE type;
  
  g_return_val_if_fail (GTK_IS_TREE_ITEM(widget), FALSE);
  clicked_item = GTK_TREE_ITEM(widget);
  mouse_event = scan_mouse_press(event, &last_press_time);
  
  switch ( mouse_event )
  {
    case MOUSE_LEFT_DOUBLE:
        
        type = tree_get_item_type(GTK_TREE_ITEM(widget));

        switch ( type )
        {
          case TYPE_ROOT:
              if ( !pref.v.shares_scan_user || !pref.v.groups_scan_user )
              {
                tree_scan_item(BROWSE_MODE_NONE);
              }
              else
              {
                do_password_scan();
              }
              break;
            
          case TYPE_GROUP:
              if ( !pref.v.shares_scan_user || !pref.v.machines_scan_user )
              {
                tree_scan_item(BROWSE_MODE_NONE);
              }
              else
              {
                do_password_scan();
              }
              break;
            
          case TYPE_MACHINE:
          case TYPE_HOST:
              if ( !pref.v.shares_scan_user )
              {
                tree_scan_item(BROWSE_MODE_NONE);
              }
              else
              {
                do_password_scan();
              }
              break;
            
          default:
              if ( !pref.v.shares_scan_user )
              {
                tree_scan_item(BROWSE_MODE_NONE);
              }
              else
              {
                do_password_scan();
              }
              break;
        }
        break;
    
    default:
        if ( GTK_IS_TREE_ITEM(clicked_item) )
        {
          status_set_object_text(tree_item_count(clicked_item));
        }
        break;
  }
  
  return TRUE;
}

/* -- mouse up events -- */

static gint item_release_event (GtkWidget *widget, GdkEventButton *event,
                                    	gpointer data)
{
  static guint32 last_press_time = 0;
  int mouse_event;
  ITEMTYPE item_type;
  int pop_items;
  
  g_return_val_if_fail (GTK_IS_TREE_ITEM(widget), FALSE);
  clicked_item = GTK_TREE_ITEM(widget);
  
  if ( GTK_IS_TREE_ITEM(clicked_item) )
  {
    item_type = tree_get_item_type(clicked_item);
  }
  else
    return TRUE;
  
  mouse_event = scan_mouse_press(event, &last_press_time);
  
  switch ( mouse_event )
  {
    /* -- context sensitive menus -- */
    case MOUSE_RIGHT_SINGLE:
        switch ( item_type )
        {
          case TYPE_ROOT:
              pop_items = sizeof(root_popup) / sizeof(root_popup[0]);
              show_popup_menu(root_popup, pop_items, event->x_root, event->y_root);
              break;
          case TYPE_GROUP:
              pop_items = sizeof(group_popup) / sizeof(GtkMenuEntry);
              show_popup_menu(group_popup, pop_items, event->x_root, event->y_root);
              break;
          case TYPE_MACHINE:
              pop_items = sizeof(machine_popup) / sizeof(GtkMenuEntry);
              show_popup_menu(machine_popup, pop_items, event->x_root, event->y_root);
              break;
          case TYPE_HOST:
              pop_items = sizeof(pref_machine_popup) / sizeof(GtkMenuEntry);
              show_popup_menu(pref_machine_popup, pop_items, event->x_root, event->y_root);
              break;
          case TYPE_FOLDER:
#ifndef NO_SMBMOUNT
              pop_items = sizeof(folder_popup) / sizeof(GtkMenuEntry);
              show_popup_menu(folder_popup, pop_items, event->x_root, event->y_root);
#endif
              break;
          case TYPE_OPEN_FOLDER:
#ifndef NO_SMBMOUNT
              pop_items = sizeof(open_folder_popup) / sizeof(GtkMenuEntry);
              show_popup_menu(open_folder_popup, pop_items, event->x_root, event->y_root);
#endif
              break;
          default:
              break;
        }
        break;
  }
  
  return TRUE;
}

/* ------- context menu of the clist ------------------------------------- */

static void mlist_column_resize (GtkCList *list, gint column, gint width)
{
  if ( column == 0 )
  {
    size.m_mpoint_pos = width;
  }
}

static gint new_clist_button_release (GtkWidget *widget, GdkEventButton *event)
{
  int row = 0;
  int column = 0;
  int x, y;
#ifndef NO_SMBMOUNT
  int pop_items;
  char *cell_text;
#endif

  /* right mouse click ? */
  if ( event->button == 3 )
  {
    x = event->x;
    y = event->y;
    if ( mount_clist && GTK_IS_CLIST(mount_clist) )
    {
      if ( gtk_clist_get_selection_info( GTK_CLIST(mount_clist), x, y, &row, &column) )
      {
#ifndef NO_SMBMOUNT
        pop_items = sizeof(clist_popup) / sizeof(GtkMenuEntry);
        gtk_clist_get_text( GTK_CLIST(mount_clist), row, 1, &cell_text);
        clist_popup[0].callback_action = (guint)cell_text;
        clist_popup[2].callback_action = (guint)cell_text;
        show_popup_menu(clist_popup, pop_items, event->x_root, event->y_root);
#endif
      }
    }
  }
  return TRUE;
}

/* cheader was moved */

static void scroller_column_resize (GtkCList *list, gint column, gint width)
{
  GList *step_list;
  GtkWidget *box;
  GtkBoxChild *child;
  GtkWidget *name;
  GtkWidget *comment;
  GtkWidget *mountpoint;
  int type;
  GtkTreeItem *item;
  gint xname = 0;

  switch ( column )
  {
    case 0:
        size.t_comment_pos = width + ( 2 * COLUMN_INSET );
        break;
    case 1:
        size.t_mountpoint_pos = width + ( 4 * COLUMN_INSET );
        break;
    default:
        return;
  }

  tree_reset_iteration();
  item = tree_iterate();
  
  while ( item )
  {
    type = tree_get_item_type(item);
    
    if ( type != TYPE_NONE )
    {
      box = GTK_BIN(item)->child;
      step_list = GTK_BOX(box)->children->next; /* type entry */
      step_list = step_list->next;              /* name entry */
      child = step_list->data;
      name = child->widget;
      
      xname = name->allocation.x;
      if ( column == 0 )
        gtk_widget_set_usize(name, size.t_comment_pos - USIZE_SIZE - xname, -1);
    
      if ( (type == TYPE_MACHINE) ||
           (type == TYPE_HOST) ||
           (type == TYPE_FOLDER) || 
           (type == TYPE_PRINTER) ||
           (type == TYPE_OPEN_FOLDER) )
      {
        step_list = step_list->next;       /* comment entry */
        child = step_list->data;
        comment = child->widget;
        
        gtk_widget_set_uposition(comment, size.t_comment_pos, -1);
        if ( column == 1 )
          gtk_widget_set_usize(comment, size.t_mountpoint_pos - USIZE_SIZE - COLUMN_INSET, -1);

        if ( type == TYPE_OPEN_FOLDER )
        {
          step_list = step_list->next;     /* mountpoint entry */
          child = step_list->data;
          mountpoint = child->widget;
          gtk_widget_set_uposition(mountpoint, size.t_comment_pos + size.t_mountpoint_pos, -1);
        }
      }
    }
    
    item = tree_iterate();
  }
  
  /* repaint tree */
  gtk_widget_queue_draw(scrolled_win);
}

/* track new tailor position */

static int new_vpaned_button_release (GtkWidget * widget, GdkEventButton * event)
{
  GtkPaned *paned;
  
  paned = GTK_PANED(vpaned);
  
  if ( vpaned )
  {
    size.m_list_h = vpaned->allocation.height - paned->child1_size;
  }
  return FALSE;
}

/* ------------------------------------------------------------------------ */

/* ------------------------ */
/* -- Tree Item Managing -- */
/* ------------------------ */

static GtkWidget * get_item_pixmap (ITEMTYPE type)
{
  GdkPixmap *gdkpixmap;
  GdkBitmap *mask;
  GtkWidget *pixmap;

  switch ( type )
  {
    case TYPE_ROOT:
        if ( !root_pixmap )
        {
          root_pixmap = gdk_pixmap_create_from_xpm_d(main_window->window,
                              &root_mask, 0, (gchar **)root_xpm);
        }
        gdkpixmap = root_pixmap;
        mask = root_mask;
        break;

    case TYPE_GROUP:
        if ( !group_pixmap )
        {
          group_pixmap = gdk_pixmap_create_from_xpm_d(main_window->window,
                              &group_mask, 0, (gchar **)group_xpm);
        }
        gdkpixmap = group_pixmap;
        mask = group_mask;
        break;

    case TYPE_MACHINE:
    case TYPE_HOST:
        if ( !machine_pixmap )
        {
          machine_pixmap = gdk_pixmap_create_from_xpm_d(main_window->window,
                               &machine_mask, 0, (gchar **)machine_xpm);
        }
        gdkpixmap = machine_pixmap;
        mask = machine_mask;
        break;

    case TYPE_FOLDER:
        if ( !folder_pixmap )
        {
          folder_pixmap = gdk_pixmap_create_from_xpm_d(main_window->window,
                               &folder_mask, 0, (gchar **)folder_xpm);
        }
        gdkpixmap = folder_pixmap;
        mask = folder_mask;
        break;

    case TYPE_OPEN_FOLDER:
        if ( !open_folder_pixmap )
        {
          open_folder_pixmap = gdk_pixmap_create_from_xpm_d(main_window->window,
                               &open_folder_mask, 0, open_folder_xpm);
        }
        gdkpixmap = open_folder_pixmap;
        mask = open_folder_mask;
        break;

    case TYPE_PRINTER:
        if ( !printer_pixmap )
        {
          printer_pixmap = gdk_pixmap_create_from_xpm_d(main_window->window,
                               &printer_mask, 0, printer_xpm);
        }
        gdkpixmap = printer_pixmap;
        mask = printer_mask;
        break;

    default:
        if ( !root_pixmap )
        {
          root_pixmap = gdk_pixmap_create_from_xpm_d(main_window->window,
                               &root_mask, 0, root_xpm);
        }
        gdkpixmap = root_pixmap;
        mask = root_mask;
        break;

  }

  pixmap = gtk_pixmap_new(gdkpixmap, mask);

  return pixmap;
}

/* ------------------------------------------------------------------------- */

/* --------------------------- */
/* --- workgroup functions --- */
/* --------------------------- */

static GtkTreeItem *group_create_tree_item (char *name)
{
  GtkWidget *item;
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *pixmap;
  static char type_text[16];
  
  item = gtk_tree_item_new();
  gtk_signal_connect(GTK_OBJECT(item), "button_press_event",
                     GTK_SIGNAL_FUNC(item_press_event), NULL);
  gtk_signal_connect(GTK_OBJECT(item), "button_release_event",
                     GTK_SIGNAL_FUNC(item_release_event), NULL);
  
  pixmap = get_item_pixmap(TYPE_GROUP);

  box = gtk_hbox_new(FALSE, 0);
  gtk_widget_show(box);

  /* insert icon */
  gtk_box_pack_start( GTK_BOX(box), pixmap, FALSE, TRUE, 0);
  gtk_widget_show(pixmap);

  /* insert item type, no 'show' because it's invisible */
  sprintf(type_text, "%i", TYPE_GROUP);
  label = gtk_label_new(type_text);
  gtk_box_pack_start( GTK_BOX(box), label, FALSE, FALSE, 0);

  /* create name label */
  label = gtk_label_new(name);
  gtk_label_set_justify( GTK_LABEL(label), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment( GTK_MISC(label), 0, 0);
  gtk_widget_set_usize(label, size.t_comment_pos - USIZE_SIZE - GROUP_INSET, -1);
  gtk_box_pack_start( GTK_BOX(box), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  gtk_container_add( GTK_CONTAINER(item), box);

  return GTK_TREE_ITEM(item);
}

static void group_get_name (GtkTreeItem *item, char **group)
{
  GtkWidget *box;
  GtkBoxChild *child;
  GtkLabel *label;
  GList *step_list;
  
  *group = cempty;
  
  if ( tree_get_item_type(item) == TYPE_GROUP )
  {
    box = GTK_BIN(item)->child;

    /* type entry */
    step_list = GTK_BOX(box)->children->next;

    /* name entry */
    step_list = step_list->next;
    child = step_list->data;
    label = GTK_LABEL(child->widget);
    gtk_label_get(label, group);
  }
}

static GtkTreeItem *group_get_by_name (const char *group)
{
  GtkTreeItem *item, *ritem;
  GList *list;
  char *name;
  
  ritem = NULL;
  if ( root_item != NULL )
  {
    if ( GTK_TREE_ITEM(root_item)->subtree != NULL )
    {
      list = GTK_TREE(GTK_TREE_ITEM(root_item)->subtree)->children;
      while ( list != NULL )
      {
        item = GTK_TREE_ITEM(list->data);
        if ( item )
        {
          name = NULL;
          group_get_name(item, &name);
          if ( name )
          {
            if ( compare_smb_groupname(name, group) )
            {
              ritem = item;
              break;
            }
          }
        list = list->next;
        }
      }
    }
  }
  
  return ritem;
}

void group_delete_all (void)
{
  GtkTreeItem *root;
  
  root = GTK_TREE_ITEM(root_item);
  if ( root != NULL )
  {
    if ( root->subtree != NULL )
    {
      gtk_tree_item_remove_subtree(root);
      root->subtree = NULL;
    }
  }
}

static void group_add_enumerate (group_struct *group, gpointer data)
{
  GtkTreeItem *item;

  item = group_create_tree_item(group->name);
  if ( item != NULL )
  {
    gtk_tree_append( GTK_TREE(GTK_TREE_ITEM(root_item)->subtree), GTK_WIDGET(item));
    gtk_widget_show(GTK_WIDGET(item));
  }
}

static void group_retrigger ()
{
  GtkTreeItem *root;
  GtkWidget* subtree;
	
  if ( !root_item )
    return;

  root = GTK_TREE_ITEM(root_item);
  
  /* delete all item entries */
  group_delete_all();
  
  /* is any group in the list ? */
  if ( group_list_count() > 0 )
  {
    /* recreate subtree */
    subtree = gtk_tree_new();
    gtk_tree_item_set_subtree(GTK_TREE_ITEM(root), subtree);
    /* -- suppress tree standard click behaviour -- */
    gtk_signal_connect(GTK_OBJECT(root->subtree), "event",
                    	 GTK_SIGNAL_FUNC(item_dummy), NULL);

    /* enumerate all workgroups to insert them */
    group_list_enumerate(group_add_enumerate, NULL);
    
    gtk_tree_item_expand(GTK_TREE_ITEM(root));

    tree_move_scrollwindow(root);
  }
}

/* ------------------------- */
/* --- machine functions --- */
/* ------------------------- */

static GtkTreeItem *machine_create_tree_item (char *name, char *comment)
{
  GtkWidget *item;
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *pixmap;
  static char type_text[16];
  
  item = gtk_tree_item_new();
  gtk_signal_connect(GTK_OBJECT(item), "button_press_event",
                     GTK_SIGNAL_FUNC(item_press_event), NULL);
  gtk_signal_connect(GTK_OBJECT(item), "button_release_event",
                     GTK_SIGNAL_FUNC(item_release_event), NULL);
  
  pixmap = get_item_pixmap(TYPE_MACHINE);

  box = gtk_hbox_new(FALSE, 0);
  gtk_widget_show(box);

  /* insert icon */
  gtk_box_pack_start( GTK_BOX(box), pixmap, FALSE, TRUE, 0);
  gtk_widget_show(pixmap);

  /* insert item type, no 'show' because it's invisible */
  sprintf(type_text, "%i", TYPE_MACHINE);
  label = gtk_label_new(type_text);
  gtk_box_pack_start( GTK_BOX(box), label, FALSE, FALSE, 0);

  /* create name label */
  label = gtk_label_new(name);
  gtk_label_set_justify( GTK_LABEL(label), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment( GTK_MISC(label), 0, 0);
  gtk_widget_set_usize(label, size.t_comment_pos - USIZE_SIZE - MACHINE_INSET, -1);
  gtk_box_pack_start( GTK_BOX(box), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  /* create comment label */
  label = gtk_label_new(comment);
  gtk_label_set_justify( GTK_LABEL(label), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment( GTK_MISC(label), 0, 0);
  gtk_widget_set_uposition( label, size.t_comment_pos, -1);
  gtk_widget_set_usize( label, size.t_mountpoint_pos - USIZE_SIZE - COLUMN_INSET, -1);
  gtk_box_pack_start( GTK_BOX(box), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  gtk_container_add( GTK_CONTAINER(item), box);

  return GTK_TREE_ITEM(item);
}

static void machine_get_name (GtkTreeItem *item, char **name)
{
  GtkWidget *box;
  GList *step_list;
  GtkBoxChild *child;
  GtkLabel *label;
  
  *name = cempty;
  
  if ( tree_get_item_type(item) == TYPE_MACHINE )
  {
    box = GTK_BIN(item)->child;

    /* type entry */
    step_list = GTK_BOX(box)->children->next;

    /* name entry */
    step_list = step_list->next;
    child = step_list->data;
    label = GTK_LABEL(child->widget);
    gtk_label_get(label, name);
  }
}

static GtkTreeItem *machine_get_by_name (const char *group, const char *machine)
{
  GtkTreeItem *item, *ritem;
  GList *list;
  char *name;
  
  ritem = NULL;
  item = group_get_by_name(group);
  if ( item != NULL )
  {
    if ( item->subtree != NULL )
    {
      list = GTK_TREE(item->subtree)->children;
      while ( list != NULL )
      {
        item = GTK_TREE_ITEM(list->data);
        if ( item )
        {
          name = NULL;
          machine_get_name(item, &name);
          if ( name )
          {
            if ( compare_smb_machinename(name, machine) )
            {
              ritem = item;
              break;
            }
          }
        }
        list = list->next;
      }
    }
  }
  
  return ritem;
}

static void machine_get_name_and_group (GtkTreeItem *item, char **group, char **machine)
{
  GtkTreeItem *group_item;

  *group = cempty;
  machine_get_name(item, machine);
  group_item = tree_item_get_parent_item(item);
  if ( group_item != NULL )
  {
    group_get_name(group_item, group);
  }
}

static machine_struct *machine_get_data_by_item (GtkTreeItem *item)
{
  ITEMTYPE item_type;
  char *group, *machine;
  machine_struct *m_struct;
  
  m_struct = NULL;
  
  if ( GTK_IS_TREE_ITEM(item) )
  {
    item_type = tree_get_item_type(item);
    if ( item_type == TYPE_MACHINE )
    {
      group = cempty;
      machine = cempty;
      machine_get_name_and_group(item, &group, &machine);
      m_struct = machine_list_search_by_group_name(group, machine);
    }
  }
  return m_struct;
}

static void machine_delete_all (GtkTreeItem *group)
{
  if ( group != NULL )
  {
    if ( group->subtree != NULL )
    {
      gtk_tree_item_remove_subtree(group);
      group->subtree = NULL;
    }
  }
}

static void machine_add_enumerate (machine_struct *machine, gpointer data)
{
  GtkTreeItem *item;
  GtkTreeItem *groupitem;

  groupitem = GTK_TREE_ITEM(data);
  if ( groupitem == NULL )
    return;
  
  item = machine_create_tree_item(machine->name, machine->comment);
  if ( item != NULL )
  {
    gtk_tree_append( GTK_TREE(groupitem->subtree), GTK_WIDGET(item));
    gtk_widget_show(GTK_WIDGET(item));
  }
}

static void machine_retrigger (data_notify_struct *notify)
{
  GtkWidget* subtree;
  GtkTreeItem *groupitem;
  BROWSE_MODE mode;

  mode = (BROWSE_MODE)(notify->data);
  groupitem = group_get_by_name(notify->group);
  
  if ( groupitem != NULL )
  {
    /* delete all item entries */
    machine_delete_all(groupitem);

    /* is any machine in the group ? */
    if ( machine_list_count(notify->group) > 0 )
    {
      /* recreate subtree */
      subtree = gtk_tree_new();
      gtk_tree_item_set_subtree( GTK_TREE_ITEM(groupitem), subtree);
      /* -- suppress tree standard click behaviour -- */
      gtk_signal_connect(GTK_OBJECT(groupitem->subtree), "event",
                           GTK_SIGNAL_FUNC(item_dummy), NULL);

      /* enumerate all workgroups to insert them */
      machine_list_enumerate(notify->group, machine_add_enumerate, (gpointer)groupitem);
    
      if ( !(mode & BROWSE_MODE_QUICK) )
        gtk_tree_item_expand(GTK_TREE_ITEM(groupitem));
    }
    tree_move_scrollwindow(groupitem);
  }
}

/* ----------------------- */
/* --- share functions --- */
/* ----------------------- */

static char share_current_machine[MAXMACHNAMEL+1];

static GtkTreeItem *share_create_tree_item (char *name, char *comment, SHARE_TYPE st, char *mpoint)
{
  GtkWidget *item;
  GtkWidget *box;
  GtkWidget *label;
  ITEMTYPE type;
  char *mountpoint;
  mount_struct *m_struct;
  GtkWidget *pixmap;
  static char type_text[16];
  
  item = gtk_tree_item_new();
  gtk_signal_connect(GTK_OBJECT(item), "button_press_event",
                     GTK_SIGNAL_FUNC(item_press_event), NULL);
  gtk_signal_connect(GTK_OBJECT(item), "button_release_event",
                     GTK_SIGNAL_FUNC(item_release_event), NULL);
  
  mountpoint = cempty;
  /* what kind of share is it ? */
  switch ( st )
  {
    case shtype_folder:
        type = TYPE_FOLDER;
        /* force a mounted folder ? */
        if ( mpoint != NULL )
        {
          type = TYPE_OPEN_FOLDER;
          mountpoint = mpoint;
        }
        else
        {
          /* check if the share's currently mounted */
          m_struct = mount_list_search_by_machine_share(share_current_machine, name);
          if ( m_struct != NULL )
          {
            type = TYPE_OPEN_FOLDER;
            mountpoint = m_struct->mountpoint;
          }
        }
        break;
    
    case shtype_printer:
        type = TYPE_PRINTER;
        break;
    
    default:
        return NULL;
  }
  
  pixmap = get_item_pixmap(type);

  box = gtk_hbox_new(FALSE, 0);
  gtk_widget_show(box);

  /* insert icon */
  gtk_box_pack_start( GTK_BOX(box), pixmap, FALSE, TRUE, 0);
  gtk_widget_show(pixmap);

  /* insert item type, no 'show' because it's invisible */
  sprintf(type_text, "%i", type);
  label = gtk_label_new(type_text);
  gtk_box_pack_start( GTK_BOX(box), label, FALSE, FALSE, 0);

  /* create name label */
  label = gtk_label_new(name);
  gtk_label_set_justify( GTK_LABEL(label), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment( GTK_MISC(label), 0, 0);
  gtk_widget_set_usize(label, size.t_comment_pos - USIZE_SIZE - SHARE_INSET, -1);
  gtk_box_pack_start( GTK_BOX(box), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  /* create comment label */
  label = gtk_label_new(comment);
  gtk_label_set_justify( GTK_LABEL(label), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment( GTK_MISC(label), 0, 0);
  gtk_widget_set_uposition( label, size.t_comment_pos, -1);
  gtk_widget_set_usize( label, size.t_mountpoint_pos - USIZE_SIZE - COLUMN_INSET, -1);
  gtk_box_pack_start( GTK_BOX(box), label, FALSE, FALSE, 2);
  gtk_widget_show(label);
  
  /* create mountpoint label */
  label = gtk_label_new(mountpoint);
  gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
  gtk_widget_set_uposition(label, size.t_mountpoint_pos + size.t_comment_pos, -1);
  gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  gtk_container_add( GTK_CONTAINER(item), box);

  return GTK_TREE_ITEM(item);
}

#ifndef NO_SMBMOUNT

static void share_get_name (GtkTreeItem *item, char **name)
{
  ITEMTYPE type;
  GtkWidget *box;
  GList *step_list;
  GtkBoxChild *child;
  GtkLabel *label;
  
  *name = cempty;
  
  type = tree_get_item_type(item);
  if ( (type == TYPE_FOLDER) || (type == TYPE_OPEN_FOLDER) )
  {
    box = GTK_BIN(item)->child;

    /* type entry */
    step_list = GTK_BOX(box)->children->next;

    /* name entry */
    step_list = step_list->next;
    child = step_list->data;
    label = GTK_LABEL(child->widget);
    gtk_label_get(label, name);
  }
}

static GtkTreeItem *share_get_by_name (const char *group, const char *machine, const char *share)
{
  GtkTreeItem *item, *ritem;
  GList *list;
  char *name;
  
  ritem = NULL;
  if ( !is_empty_string(group) )
  {
    item = machine_get_by_name(group, machine);
  }
  else
  {
    item = host_get_by_name(machine);
  }
  
  if ( item != NULL )
  {
    if ( item->subtree != NULL )
    {
      list = GTK_TREE(item->subtree)->children;
      while ( list != NULL )
      {
        item = GTK_TREE_ITEM(list->data);
        if ( item != NULL )
        {
          name = NULL;
          share_get_name(item, &name);
          if ( name )
          {
            if ( compare_smb_sharename(name, share) )
            {
              ritem = item;
              break;
            }
          }
        }
        list = list->next;
      }
    }
  }
  
  return ritem;
}

static void share_get_mountpoint (GtkTreeItem *item, char **mountpoint)
{
  ITEMTYPE type;
  GtkWidget *box;
  GList *step_list;
  GtkBoxChild *child;
  GtkLabel *label;
  
  *mountpoint = cempty;
  
  type = tree_get_item_type(item);
  if ( (type == TYPE_FOLDER) || (type == TYPE_OPEN_FOLDER) )
  {
    box = GTK_BIN(item)->child;

    /* type entry */
    step_list = GTK_BOX(box)->children->next;

    /* name entry */
    step_list = step_list->next;
    
    /* comment entry */
    step_list = step_list->next;
    
    /* mountpoint entry */
    step_list = step_list->next;
    child = step_list->data;
    label = GTK_LABEL(child->widget);
    gtk_label_get(label, mountpoint);
  }
}

static share_struct *share_get_data_by_item (GtkTreeItem *item)
{
  ITEMTYPE item_type;
  char *share, *machine, *group;
  share_struct *s_struct;
  GtkTreeItem *machine_item;
  
  s_struct = NULL;
  
  item_type = tree_get_item_type(item);
  if ( (item_type == TYPE_FOLDER) || (item_type == TYPE_OPEN_FOLDER) )
  {
    share = cempty;
    machine = cempty;
    group = cempty;
    share_get_name(item, &share);
    machine_item = tree_item_get_parent_item(item);
    if ( machine_item != NULL )
    {
      item_type = tree_get_item_type(machine_item);
      
      if ( item_type == TYPE_MACHINE )
      {
        machine_get_name_and_group(machine_item, &group, &machine);
      }
      else if ( item_type == TYPE_HOST )
      {
        host_get_name(machine_item, &machine);
      }
    }
    s_struct = share_list_search_by_group_machine_name(group, machine, share);
  }

  return s_struct;
}

#endif

static void share_delete_all (GtkTreeItem *machine)
{
  if ( machine != NULL )
  {
    if ( machine->subtree != NULL )
    {
      gtk_tree_item_remove_subtree(machine);
      machine->subtree = NULL;
    }
  }
}

static void share_add_enumerate (share_struct *share, gpointer data)
{
  GtkTreeItem *item;
  GtkTreeItem *machineitem;

  if ( data == NULL )
    return;

  machineitem = GTK_TREE_ITEM(data);
  
  item = share_create_tree_item(share->name, share->comment, share->st, NULL);
  if ( item != NULL )
  {
    gtk_tree_append( GTK_TREE(machineitem->subtree), GTK_WIDGET(item));
    gtk_widget_show(GTK_WIDGET(item));
  }
}

//static void share_retrigger (char *groupname, char *machinename)
static void share_retrigger (const char *groupname, data_notify_struct *notify)
{
  GtkWidget* subtree;
  GtkTreeItem *machineitem;
  BROWSE_MODE mode;

  /* group/machine or pref host ? */
  if ( !is_empty_string(groupname) )
  {
    machineitem = machine_get_by_name(groupname, notify->machine);
  }
  else
  {
    machineitem = host_get_by_name(notify->machine);
  }
  
  if ( machineitem != NULL )
  {
    /* delete all item entries */
    share_delete_all(machineitem);

    /* is any share in the machine of the group ? */
    if ( share_list_count(groupname, notify->machine) > 0 )
    {
      /* recreate subtree */
      subtree = gtk_tree_new();
      gtk_tree_item_set_subtree( GTK_TREE_ITEM(machineitem), subtree);
      /* -- suppress tree standard click behaviour -- */
      gtk_signal_connect(GTK_OBJECT(machineitem->subtree), "event",
                           GTK_SIGNAL_FUNC(item_dummy), NULL);

      /* enumerate all shares to insert them */
      string_ncopy(share_current_machine, notify->machine, MAXMACHNAMEL);
      share_list_enumerate(groupname, notify->machine, share_add_enumerate, (gpointer)machineitem);

      mode = (BROWSE_MODE)(notify->data);
      if ( !(mode & BROWSE_MODE_NETWORK) )
      {
        gtk_tree_item_expand(GTK_TREE_ITEM(machineitem));
      }
    }
  }
  tree_move_scrollwindow(machineitem);
}

/* ---------------------- */
/* --- host functions --- */
/* ---------------------- */

static GtkTreeItem *host_create_tree_item (char *name, char *group)
{
  GtkWidget *item;
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *pixmap;
  static char type_text[16];
  
  item = gtk_tree_item_new();
  gtk_signal_connect(GTK_OBJECT(item), "button_press_event",
                     GTK_SIGNAL_FUNC(item_press_event), NULL);
  gtk_signal_connect(GTK_OBJECT(item), "button_release_event",
                     GTK_SIGNAL_FUNC(item_release_event), NULL);
  
  pixmap = get_item_pixmap(TYPE_HOST);

  box = gtk_hbox_new(FALSE, 0);
  gtk_widget_show(box);

  /* insert icon */
  gtk_box_pack_start( GTK_BOX(box), pixmap, FALSE, TRUE, 0);
  gtk_widget_show(pixmap);

  /* insert item type, no 'show' because it's invisible */
  sprintf(type_text, "%i", TYPE_HOST);
  label = gtk_label_new(type_text);
  gtk_box_pack_start( GTK_BOX(box), label, FALSE, FALSE, 0);

  /* create name label */
  label = gtk_label_new(name);
  gtk_label_set_justify( GTK_LABEL(label), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment( GTK_MISC(label), 0, 0);
  gtk_widget_set_usize(label, size.t_comment_pos - USIZE_SIZE - PREF_MACHINE_INSET, -1);
  gtk_box_pack_start( GTK_BOX(box), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  /* create group label */
  label = gtk_label_new(group);
  gtk_label_set_justify( GTK_LABEL(label), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment( GTK_MISC(label), 0, 0);
  gtk_widget_set_uposition( label, size.t_comment_pos, -1);
  gtk_widget_set_usize( label, size.t_mountpoint_pos - USIZE_SIZE - COLUMN_INSET, -1);
  gtk_box_pack_start( GTK_BOX(box), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  gtk_container_add( GTK_CONTAINER(item), box);

  return GTK_TREE_ITEM(item);
}

static GtkTreeItem *host_get_by_name (const char *host)
{
  GtkTreeItem *item, *ritem;
  GList *list;
  guint count;
  char *name;
  
  ritem = NULL;
  if ( root_tree != NULL )
  {
    count = g_list_length((GTK_TREE(root_tree))->children);
    if ( count > 0 )
    {
      count--;
      list = GTK_TREE(root_tree)->children;
      while ( count )
      {
        if ( list != NULL )
        {
          item = GTK_TREE_ITEM(list->data);
          if ( item )
          {
            name = cempty;
            host_get_name(item, &name);
            if ( !is_empty_string(name) )
            {
              if ( compare_smb_machinename(name, host) )
              {
                ritem = item;
                break;
              }
            }
          }
        }
        list = list->next;
        count--;
      }
    }
  }
  
  return ritem;
}

static void host_get_name (GtkTreeItem *item, char **name)
{
  GtkWidget *box;
  GList *step_list;
  GtkBoxChild *child;
  GtkLabel *label;
  
  *name = cempty;
  
  if ( tree_get_item_type(item) == TYPE_HOST )
  {
    box = GTK_BIN(item)->child;

    /* type entry */
    step_list = GTK_BOX(box)->children->next;

    /* name entry */
    step_list = step_list->next;
    child = step_list->data;
    label = GTK_LABEL(child->widget);
    gtk_label_get(label, name);
  }
}

static host_struct *host_get_data_by_item (GtkTreeItem *item)
{
  ITEMTYPE item_type;
  char *host;
  host_struct *h_struct;
  
  h_struct = NULL;
  
  item_type = tree_get_item_type(item);
  if ( item_type == TYPE_HOST )
  {
    host = cempty;
    host_get_name(item, &host);
    h_struct = host_list_search_by_name(host);
  }
  
  return h_struct;
}

static void host_delete_all ()
{
  guint count;
  
  if ( root_tree != NULL )
  {
    count = g_list_length((GTK_TREE(root_tree))->children);
    if ( count > 2 )
    {
      count--;
      count--;
      gtk_tree_clear_items(GTK_TREE(root_tree), 0, count);
    }
    /* bug in gtk_tree_clear_items(..., 0, 0) doesn't work ? */
    else
    {
      if ( count == 2 )
      {
        gtk_container_remove(GTK_CONTAINER(root_tree),
                             (GTK_TREE(root_tree))->children->data);
      }
    }
  }
}

static void host_add_enumerate (host_struct *host, gpointer data)
{
  GtkTreeItem *item;
  guint pos;

  if ( root_tree != NULL )
  {
    item = host_create_tree_item(host->name, host->group);
    if ( item != NULL )
    {
      pos = g_list_length(GTK_TREE(root_tree)->children);
      if ( pos && (root_item != NULL) )
        pos--;
      gtk_tree_insert( GTK_TREE(root_tree), GTK_WIDGET(item), pos);
      gtk_widget_show( GTK_WIDGET(item));
    }
  }
}

static void host_retrigger ()
{
  /* delete all preferred hosts */
  host_delete_all();
  
  /* enumerate all hosts to insert them */
  host_list_enumerate(host_add_enumerate, NULL);
}

/* ---------------------- */
/* --- tree functions --- */
/* ---------------------- */

/* callback function for all changes in the browsing data
   -> this function is called every time when the browse data changes */

static void tree_data_change_callback (data_notify_struct *notify)
{
  change_win_cursor(main_window->window, GDK_TOP_LEFT_ARROW);

  switch ( notify->dt )
  {
    /* review all groups */
    case dt_group:
        group_retrigger();
        break;
    
    /* review all machines in a group */
    case dt_machine:
        machine_retrigger(notify);
        break;
    
    /* retrigger all preferred hosts */
    case dt_pref_host:
        host_retrigger();
        break;
    
    /* retrigger all shares */
    case dt_share:
        share_retrigger(notify->group, notify);
        break;
    
    /* retrigger shares of a preferred host */
    case dt_share_pref_host:
        share_retrigger(cempty, notify);
        break;
    
    /* changes in the mount list */
    case dt_mount:
#ifndef NO_SMBMOUNT
        mount_retrigger();
#endif
        break;
    
    default:
        break;
  }
}

/* ------------------------------------------------------------------------ */

static GtkTreeItem *tree_item_get_parent_item (GtkTreeItem *item)
{
  return GTK_TREE_ITEM(GTK_TREE(GTK_WIDGET(item)->parent)->tree_owner);
}

/* ------------------------------------------------------------------------ */

static unsigned char tree_check_domain_browse (void)
{
  unsigned char with_domain;
  GtkTreeItem *group_item;
  ITEMTYPE type;
  char *groupname;
  
  with_domain = 1;
  strcpy(globals.domain_name, "");
  
  type = tree_get_item_type(clicked_item);
  
  if ( type == TYPE_ROOT )
    with_domain = 0;
  
  if ( (type == TYPE_MACHINE) && pref.v.use_group_browse )
  {
    group_item = tree_item_get_parent_item(clicked_item);
    if ( group_item != NULL )
    {
      group_get_name (group_item, &groupname);
      string_ncopy(globals.domain_name, groupname, MAXGROUPNAMEL);
    }
  }
  
  if ( (type == TYPE_GROUP) && pref.v.use_group_browse )
  {
    group_get_name (clicked_item, &groupname);
    string_ncopy(globals.domain_name, groupname, MAXGROUPNAMEL);
  }

  return with_domain;
}

static void tree_browse_action (GtkTreeItem *item, BROWSE_MODE mode)
{
  GtkTreeItem *machine_item;
  char *group, *machine, *share, *user, *password, *ip;
  machine_struct *m_struct;
  host_struct *h_struct;
  ITEMTYPE type;
  share_window_struct ws;
  GtkWidget *share_window;

  if ( item == NULL )
    return;
  
  ip = cempty;
  user = cempty;
  password = cempty;
  
  machine_item = tree_item_get_parent_item(item);
  
  if ( machine_item != NULL )
  {
    type = tree_get_item_type(machine_item);
    if ( type == TYPE_MACHINE )
    {
      machine_get_name_and_group(machine_item, &group, &machine);
      m_struct = machine_list_search_by_group_name(group, machine);
      if ( m_struct != NULL )
      {
        /* use the user/password while machine browsing as default */
        user = m_struct->browse_user;
        password = m_struct->browse_password;
        ip = m_struct->ipaddr;
      }
    }
    if ( type == TYPE_HOST )
    {
      host_get_name(machine_item, &machine);
      h_struct = host_list_search_by_name(machine);
      if ( h_struct != NULL )
      {
        /* use the user/password while machine browsing as default */
        user = h_struct->browse_user;
        password = h_struct->browse_password;
        ip = h_struct->ipaddr;
        group = h_struct->group;
      }
    }
    share_get_name(item, &share);
    
    /* create a new share window */
    memset(&ws, 0, sizeof(share_window_struct));
    string_ncopy(ws.group, group, MAXGROUPNAMEL);
    string_ncopy(ws.machine, machine, MAXMACHNAMEL);
    string_ncopy(ws.share, share, MAXSHRNAMEL);
    ws.path[0] = 0;
    string_ncopy(ws.user, user, USER_LEN);
    string_ncopy(ws.password, password, PASSWORD_LEN);
    string_ncopy(ws.ip, ip, MAXIPL);
    if ( mode & BROWSE_MODE_USER )
    {
      string_ncopy(ws.user, globals.browse_user, USER_LEN);
      string_ncopy(ws.password, globals.browse_password, PASSWORD_LEN);
    }
    ws.mode = mode;
    
    share_window = share_window_new(&ws);
    
    /* set the window cursor */
    if ( share_window != NULL )
    {
      change_win_cursor(share_window->window, GDK_WATCH);
    }
    
    /* browse the share */
    browse_path(&ws, mode);
  }
}

#ifndef NO_SMBMOUNT

static void tree_mount_action (GtkTreeItem *item, unsigned char show_gui)
{
  GtkTreeItem *machine_item;
  char *group, *machine, *share, *ip;
  machine_struct *m_struct;
  host_struct *h_struct;
  char resource[MAX_REC_LEN+1];
  ITEMTYPE type;

  group = cempty;
  machine = cempty;
  share = cempty;
  ip = cempty;
  
  if ( item != NULL )
  {
    machine_item = tree_item_get_parent_item(item);
    if ( machine_item != NULL )
    {
      type = tree_get_item_type(machine_item);
      if ( type == TYPE_MACHINE )
      {
        machine_get_name_and_group(machine_item, &group, &machine);
        m_struct = machine_list_search_by_group_name(group, machine);
        if ( m_struct != NULL )
        {
          if ( !is_empty_string(m_struct->domain) )
            group = m_struct->domain;
          ip = m_struct->ipaddr;
        }
      }
      else if ( type == TYPE_HOST )
      {
        host_get_name(machine_item, &machine);
        h_struct = host_list_search_by_name(machine);
        if ( h_struct != NULL )
        {
          ip = h_struct->ipaddr;
          group = h_struct->group;
          if ( !is_empty_string(h_struct->domain) )
            group = h_struct->domain;
        }
      }
      share_get_name(item, &share);

      strcpy(resource, "//");
      string_ncat(resource, machine, MAX_REC_LEN);
      string_ncat(resource, "/", MAX_REC_LEN);
      string_ncat(resource, share, MAX_REC_LEN);

      if ( !pref.v.use_group_mount )
      {
        group = cempty;
      }
      /* don't show the mount dialog when 'defaults' + 'no_dialog' */
      if ( !pref.v.root_mnt_enable || !pref.v.mount_default_no_dialog )
        show_gui = 1;

      gui_mount_dialog(resource, ip, group, show_gui);
    }
  }
}

#endif

static ITEMTYPE tree_get_item_type (GtkTreeItem * item)
{
  GList *step_list;
  GtkWidget *box;
  GtkBoxChild *child;
  GtkLabel *label;
  char *label_string;
  ITEMTYPE itemtype = TYPE_NONE;
  int type;
  
  if ( item != NULL )
  {
    if ( GTK_IS_TREE_ITEM(item) )
    {
      box = GTK_BIN(item)->child;

      step_list = GTK_BOX(box)->children->next;  /* type entry */
      child = step_list->data;
      label = GTK_LABEL(child->widget);

      gtk_label_get(label, &label_string);
      if ( strlen(label_string) )
      {
        sscanf(label_string, "%i", &type);
        itemtype = type;
      }
    }
  }
  
  return itemtype;
}

/* this function returns the number of objects(items) in the subtree */

static unsigned int tree_item_count (GtkTreeItem *item)
{
  unsigned int ireturn;

  if ( item != NULL )
  {
    if ( item->subtree )
    {
      if ( GTK_TREE(item->subtree)->children )
      {
        ireturn = (unsigned int)g_list_length(GTK_TREE(item->subtree)->children);
      }
      else
        ireturn = 0;
    }
    else
      ireturn = 0;
  }
  else
    ireturn = 0;

  return ireturn;
}

void tree_scan_item (BROWSE_MODE mode)
{
  char *groupname, *machinename;
  host_struct *h_struct;
  machine_struct *m_struct;
  ITEMTYPE item_type;
  
  item_type = tree_get_item_type(clicked_item);
  if ( item_type != TYPE_NONE )
  {
    if ( clicked_item->subtree ) {
      gtk_tree_item_remove_subtree(clicked_item);
    }
  }

  switch ( item_type ) {

    case TYPE_ROOT:			/* scan root item */
        if ( pref.v.quick_browse == YES )
          mode |= BROWSE_MODE_QUICK;
        change_win_cursor(main_window->window, GDK_WATCH);
        browse_groups(mode | BROWSE_MODE_EXPAND);
        break;

    case TYPE_GROUP:			/* scan group item */
        group_get_name(clicked_item, &groupname);
        change_win_cursor(main_window->window, GDK_WATCH);
        if ( !browse_machines(groupname, mode | BROWSE_MODE_EXPAND) )
          change_win_cursor(main_window->window, GDK_TOP_LEFT_ARROW);
        break;

    case TYPE_MACHINE:		/* scan machine item */
        machine_get_name_and_group(clicked_item, &groupname, &machinename);
        m_struct = machine_get_data_by_item(clicked_item);
        if ( m_struct != NULL )
        {
          /* if normal browse, reset domain entry */
          if ( !(mode & BROWSE_MODE_USER) )
            m_struct->domain[0] = 0;
          /* if user browse, set domain entry */
          if ( mode & BROWSE_MODE_DOMAIN )
          {
            string_ncopy(m_struct->domain, globals.domain_name, MAXGROUPNAMEL);
          }
        }
        change_win_cursor(main_window->window, GDK_WATCH);
        browse_shares(groupname, machinename, mode | BROWSE_MODE_EXPAND);
        break;

    case TYPE_HOST:  /* scan favourite machine item */
        host_get_name(clicked_item, &machinename);
        groupname = cempty;
        h_struct = host_get_data_by_item(clicked_item);
        if ( h_struct != NULL )
        {
          /* if normal browse, reset domain entry */
          if ( !(mode & BROWSE_MODE_USER) )
            h_struct->domain[0] = 0;  
          /* if user browse, set domain entry */
          if ( mode & BROWSE_MODE_DOMAIN )
            string_ncopy(h_struct->domain, globals.domain_name, MAXGROUPNAMEL);
        }
        change_win_cursor(main_window->window, GDK_WATCH);
        browse_shares(groupname, machinename, mode | BROWSE_MODE_PREF_HOST);
        break;

    case TYPE_FOLDER:
        if ( lib_smb_is_loaded() )
        {
          /* do further browse into share */
          tree_browse_action(clicked_item, mode);
        }
        else
        {
          /* show mountdialog */
          tree_mount_action(clicked_item, 0);
        }
        break;

    default:
          break;
  }
}

#ifndef NO_SMBMOUNT

/* change item icon from folder to open_folder + set mointpoint */
static void tree_folder_to_mounted (GtkTreeItem * item, char *mountpoint)
{
  GtkTreeItem *new_item;
  GtkWidget *tree;
  share_struct *s_struct;
  gint position;

  if ( GTK_IS_TREE_ITEM(item) )
  {
    if ( (tree_get_item_type(item) == TYPE_FOLDER) || (tree_get_item_type(item) == TYPE_OPEN_FOLDER) )
    {
      s_struct = share_get_data_by_item(item);
      if ( s_struct != NULL )
      {
        /* create the new share tree item */
        new_item = share_create_tree_item(s_struct->name, s_struct->comment, shtype_folder, mountpoint);
        /* replace old entry */
        tree = GTK_WIDGET(item)->parent;
        position = gtk_tree_child_position(GTK_TREE(tree), GTK_WIDGET(item));
        gtk_tree_insert(GTK_TREE(tree), GTK_WIDGET(new_item), position + 1);
        gtk_tree_remove_item(GTK_TREE(tree), GTK_WIDGET(item));
        gtk_widget_show(GTK_WIDGET(new_item));
      }
    }
  }
}

/* change item icon from open_folder to folder + clear mountpoint */
static void tree_mounted_to_folder (GtkTreeItem * item)
{
  /* in this GUI it's the same action as 'folder_to_mounted' */
  tree_folder_to_mounted(item, NULL);
}

#endif

/* sets the focus to the root item */
void tree_focus_root_item (void)
{
  gtk_widget_grab_focus(root_item);
  clicked_item = GTK_TREE_ITEM(root_item);
}

/* ---------------------------------------------------------------------- */

static GtkTreeItem * actual_root_item;
static GtkTreeItem * actual_group_item;
static GtkTreeItem * actual_machine_item;
static GList * actual_root_list;
static GList * actual_group_list;
static GList * actual_machine_list;
static GList * actual_share_list;
static unsigned char first_root;
static unsigned char first_group;
static unsigned char first_machine;
static unsigned char first_share;

static void tree_reset_iteration (void)
{
  actual_root_item = NULL;
  actual_group_item = NULL;
  actual_machine_item = NULL;
  first_root = 1;
  first_group = 1;
  first_machine = 1;
  first_share = 1;
}

static GtkTreeItem * tree_get_next_item (GList **actual_list)
{
  GtkTreeItem *ritem = NULL;

  if ( (*actual_list) && (*actual_list)->data )
  {
    ritem = GTK_TREE_ITEM((*actual_list)->data);
    *actual_list = (*actual_list)->next;
  }
  
  return ritem;
}

static GtkTreeItem *tree_iterate (void)
{
  GtkTreeItem *ritem;
  GtkTree *search_tree;

  search_tree = NULL;
  ritem = NULL;
  
  while ( 1 )
  {
    /* -- current root ? -- */
    if ( actual_root_item )
    {
      /* -- current group ? -- */
      if ( actual_group_item )
      {
        /* -- current machine ? -- */
        if ( actual_machine_item )
        {
          /* -- first share -- */
          if ( first_share )
          {
            first_share = 0;
            if ( actual_machine_item && actual_machine_item->subtree )
              search_tree = GTK_TREE(actual_machine_item->subtree);
          
            if ( search_tree )
            {
              actual_share_list = search_tree->children;
              ritem = tree_get_next_item(&actual_share_list);
              if ( ritem )
                break;
            }
          }
          /* -- next share -- */
          else
          {
            ritem = tree_get_next_item(&actual_share_list);
            if ( ritem )
              break;
          }
          first_share = 1;
          actual_machine_item = NULL;
        }
        else   /* -- machine loop -- */
        {
          /* -- first machine -- */
          if ( first_machine )
          {
            first_machine = 0;
            if ( actual_group_item && actual_group_item->subtree )
              search_tree = GTK_TREE(actual_group_item->subtree);
          
            if ( search_tree ) {
        
              actual_machine_list = search_tree->children;
              ritem = tree_get_next_item(&actual_machine_list);
              actual_machine_item = ritem;
              if ( ritem )
                break;
            }
          }
          /* -- next machine -- */
          else
          {
            ritem = tree_get_next_item(&actual_machine_list);
            actual_machine_item = ritem;
            if ( ritem )
              break;
          }
          /* -- ready with actual group -- */
          first_machine = 1;
          actual_group_item = NULL;
        }
      }
      else  /* -- group loop -- */
      {
        /* -- first group -- */
        if ( first_group )
        {
          first_group = 0;
          if ( actual_root_item->subtree )
            search_tree = GTK_TREE(actual_root_item->subtree);
      
          if ( search_tree )
          {
            actual_group_list = search_tree->children;
            ritem = tree_get_next_item(&actual_group_list);
            actual_group_item = ritem;
            if ( ritem )
              break;
          }
        }
        /* -- next group -- */
        else
        {
          ritem = tree_get_next_item(&actual_group_list);
          actual_group_item = ritem;
          if ( ritem )
            break;
        }
        /* -- ready with actual root -- */
        first_group = 1;
        actual_root_item = NULL;
      }
    }
    else  /* -- root loop -- */
    {
      /* first root */
      if ( first_root )
      {
        first_root = 0;
        if ( root_tree )
          search_tree = GTK_TREE(root_tree);
        
        if ( search_tree )
        {
          actual_root_list = search_tree->children;
          ritem = tree_get_next_item(&actual_root_list);
          actual_root_item = ritem;
        }
        break;
      }
      /* -- next root -- */
      else
      {
        ritem = tree_get_next_item(&actual_root_list);
        actual_root_item = ritem;
        break;
      }
    }
        
  }
  return ritem;
}

#ifndef NO_SMBMOUNT

static unsigned char tree_enumerate_share_item (char *machine, char *share, GtkTreeItem **folder_item)
{
  GtkTreeItem *item;
  ITEMTYPE type;
  char *name;
  
  do
  {
    item = tree_iterate();
    
    type = tree_get_item_type(item);
    if ( type == TYPE_MACHINE )
    {
      machine_get_name(item, &name);
      if ( compare_smb_machinename(machine, name) )
      {
        /* machine name found */
        do
        {
          item = tree_iterate();
          type = tree_get_item_type(item);
          if ( (type == TYPE_FOLDER) || (type == TYPE_OPEN_FOLDER) )
          {
            share_get_name(item, &name);
            if ( compare_smb_sharename(share, name) )
            {
              /* found */
              *folder_item = item;
              return 1;
            }
          }
          else
            break;
        }
        while ( item != NULL );
      }
    }
  }
  while ( item != NULL );
  
  return 0;
}

static unsigned char tree_enumerate_mounted_item (GtkTreeItem **folder_item)
{
  GtkTreeItem *item;
  ITEMTYPE type;
  
  do
  {
    item = tree_iterate();
    
    type = tree_get_item_type(item);
    if ( type == TYPE_OPEN_FOLDER )
    {
      /* found */
      *folder_item = item;
      return 1;
    }
  }
  while ( item != NULL );
  
  return 0;
}

#endif

static guint tree_subtree_length (GtkTreeItem *item)
{
  guint number;
  
  number = 0;
  if ( item != NULL )
  {
    if ( item->subtree != NULL )
      number = g_list_length(GTK_TREE(item->subtree)->children);
  }
  
  return number;
}

#define TREE_HEIGHT_BORDER            4

static unsigned int tree_calculate_height (void)
{
  unsigned int height;
  
  height = 0;
  if ( scrolled_box )
  {
    height = scrolled_box->allocation.height;
    height -= TREE_HEIGHT_BORDER;
  }
  
  return height;
}

static unsigned int tree_calculate_y_value (GtkTreeItem *item)
{
  ITEMTYPE type;
  GtkTreeItem *parent;
  unsigned int y_value;

  y_value = 0;
  if ( item != NULL )
  {
    /* calculate the y value of the tree item in the scroll window */
    y_value = GTK_WIDGET(item)->allocation.y;
    type = tree_get_item_type(item);
    parent = item;
    while ( (type != TYPE_ROOT) && (type != TYPE_HOST) )
    {
      /* calculate as long as top is reached */
      parent = tree_item_get_parent_item(parent);
      if ( parent != NULL )
      {
        y_value += GTK_WIDGET(parent)->allocation.y;
        y_value += GTK_WIDGET(parent)->allocation.height;
        type = tree_get_item_type(parent);
      }
      else
      {
        /* break condition */
        type = TYPE_ROOT;
      }
    }
  }
  
  return y_value;
}

static unsigned int tree_calculate_subtree_height (GtkTreeItem *item)
{
  unsigned int height;
  unsigned int h_value;
  
  h_value = 0;
  if ( item != NULL )
  {
    height = GTK_WIDGET(item)->allocation.height;
    h_value = height;
    /* number of subtree items */
    h_value += tree_subtree_length(item) * height;
  }

  return h_value;
}

static void tree_move_scrollwindow (GtkTreeItem *item)
{
  int h_tree, y_click, y_scrolled, h_sub;
  int y_move, y_max_move;
  
  /* height of the visible tree area */
  h_tree = tree_calculate_height();
  
  /* position of the scrollbar */
  y_scrolled = GTK_SCROLLBAR(GTK_SCROLLED_WINDOW(scrolled_win)->vscrollbar)->range.adjustment->value;
  
  /* y position of the clicked item */
  y_click = (int)tree_calculate_y_value(item);
  
  /* height of the subtree */
  h_sub = (int)tree_calculate_subtree_height(item);
  
  /* calculate move value */
  y_move = ( y_click + h_sub ) - ( h_tree + y_scrolled );
  if ( y_move < 0 )
    y_move = 0;
  
  y_max_move = y_click - y_scrolled;
  
  if ( y_move > y_max_move )
    y_move = y_max_move;
  
  /* do the scroll */
  if ( y_move > 0 )
  {
    GTK_SCROLLBAR(GTK_SCROLLED_WINDOW(scrolled_win)->vscrollbar)->range.adjustment->value += y_move;
    gtk_signal_emit_by_name(GTK_OBJECT(GTK_SCROLLBAR(GTK_SCROLLED_WINDOW(scrolled_win)->vscrollbar)->range.adjustment), "value_changed");
  }
}

/* ---------------------------------------------------------------------- */
/* --- Create ----------------------------------------------------------- */
/* ---------------------------------------------------------------------- */

void tree_create (GtkWidget *vbox)
{
  GtkWidget *pixmap;
  GtkWidget *box;
  GtkWidget *label;
  char type_text[16];
  gchar *scroller_titles[3] = {"", _("Comment"), _("Mountpoint")};
  gchar *mounted_titles[2] = {_("Resource"), _("Mountpoint")};
  char host[HOST_LEN];
  
  popup_menu_initialize();
  
  scroller = gtk_clist_new_with_titles(3, scroller_titles);
  gtk_widget_set_usize(scroller, -1, 25);
  gtk_clist_column_titles_passive( GTK_CLIST(scroller) );
  gtk_clist_set_column_min_width( GTK_CLIST(scroller), 0, COLUMN_0_MIN_WIDTH);
  gtk_clist_set_column_min_width( GTK_CLIST(scroller), 1, COLUMN_1_MIN_WIDTH);
  gtk_clist_set_column_width( GTK_CLIST(scroller), 0, size.t_comment_pos - (2*COLUMN_INSET));
  gtk_clist_set_column_width( GTK_CLIST(scroller), 1, size.t_mountpoint_pos - (4*COLUMN_INSET));
  gtk_signal_connect(GTK_OBJECT(scroller), "resize_column",
                     GTK_SIGNAL_FUNC(scroller_column_resize), NULL);
  gtk_box_pack_start( GTK_BOX(vbox), scroller, FALSE, FALSE, 0);
  gtk_widget_show(scroller);
  
  vpaned = gtk_vpaned_new();
  gtk_widget_show(vpaned);
  
  scrolled_box = gtk_vbox_new(FALSE, 0);
  
  scrolled_win = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(scrolled_win),
                                 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start( GTK_BOX(scrolled_box), scrolled_win, TRUE, TRUE, 0);
  gtk_widget_set_usize(scrolled_win, -1, 150);
  gtk_widget_show(scrolled_win);

  /* -- create root tree -- */
  root_tree = gtk_tree_new();
  gtk_tree_set_selection_mode( GTK_TREE(root_tree), GTK_SELECTION_EXTENDED);
  gtk_tree_set_view_mode( GTK_TREE(root_tree), GTK_TREE_VIEW_ITEM);
  gtk_widget_show(root_tree);
  
  /* -- suppress tree standard click behaviour -- */
  gtk_signal_connect(GTK_OBJECT(root_tree), "event",
                     GTK_SIGNAL_FUNC(item_dummy), NULL);
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_win), root_tree);

  /* insert any pref hosts if stored */
  host_retrigger();

  /* --- create root item --- */
  if ( gethostname(host, HOST_LEN-1) < 0 ) {
    strcpy(host, _("Local Machine"));
  }
  
  /* create root tree item */
  root_item = gtk_tree_item_new();
  gtk_signal_connect(GTK_OBJECT(root_item), "button_press_event",
                     GTK_SIGNAL_FUNC(item_press_event), NULL);
  gtk_signal_connect(GTK_OBJECT(root_item), "button_release_event",
                     GTK_SIGNAL_FUNC(item_release_event), NULL);
  
  pixmap = get_item_pixmap(TYPE_ROOT);

  box = gtk_hbox_new(FALSE, 0);
  gtk_widget_show(box);

  /* insert icon */
  gtk_box_pack_start( GTK_BOX(box), pixmap, FALSE, TRUE, 0);
  gtk_widget_show(pixmap);

  /* insert item type, no 'show' because it's invisible */
  sprintf(type_text, "%i", TYPE_ROOT);
  label = gtk_label_new(type_text);
  gtk_box_pack_start( GTK_BOX(box), label, FALSE, FALSE, 0);

  /* create name label */
  label = gtk_label_new(host);
  gtk_label_set_justify( GTK_LABEL(label), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment( GTK_MISC(label), 0, 0);
  gtk_widget_set_usize(label, size.t_comment_pos - USIZE_SIZE - GROUP_INSET, -1);
  gtk_box_pack_start( GTK_BOX(box), label, FALSE, FALSE, 2);
  gtk_widget_show(label);

  gtk_container_add( GTK_CONTAINER(root_item), box);
  gtk_tree_append( GTK_TREE(root_tree), root_item);
  gtk_widget_show(root_item);

  gtk_paned_pack1( GTK_PANED(vpaned), scrolled_box, 1, 1);
  gtk_widget_show(scrolled_box);

  /* create CList for mounted resources */
  
  clist_win = gtk_scrolled_window_new(NULL, NULL);
  gtk_widget_set_usize(clist_win, -1, size.m_list_h);
  gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(clist_win),
                                 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  
  mount_clist = gtk_clist_new_with_titles(2, mounted_titles);
  gtk_clist_set_selection_mode( GTK_CLIST(mount_clist), GTK_SELECTION_SINGLE);
  gtk_clist_column_titles_passive( GTK_CLIST(mount_clist) );
  gtk_clist_set_column_width( GTK_CLIST(mount_clist), 0, size.m_mpoint_pos);
  gtk_clist_set_column_min_width( GTK_CLIST(mount_clist), 0, MOUNT_MPOINT_MIN);
  gtk_clist_set_column_min_width( GTK_CLIST(mount_clist), 1, MOUNT_MPOINT_MIN);
  gtk_signal_connect(GTK_OBJECT(mount_clist), "resize_column",
                     GTK_SIGNAL_FUNC(mlist_column_resize), NULL);
  gtk_signal_connect( GTK_OBJECT(mount_clist), "button_release_event",
                      GTK_SIGNAL_FUNC(new_clist_button_release), NULL);
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(clist_win), mount_clist);
  gtk_widget_show(mount_clist);

  gtk_widget_set_usize(clist_win, -1, size.m_list_h);
  gtk_paned_pack2( GTK_PANED(vpaned), clist_win, 0, 1);
  gtk_widget_show(clist_win);
  
  gtk_signal_connect( GTK_OBJECT(vpaned), "button_release_event",
                      GTK_SIGNAL_FUNC(new_vpaned_button_release), NULL);
  gtk_box_pack_start( GTK_BOX(vbox), vpaned, TRUE, TRUE, 0);
  
  /* install the data change notifier */
  notify_add_callback(tree_data_change_callback);
  
  /* read mount list */
  notify_mount_change();
}


/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */

/* -------------------- */
/* mount list functions */
/* -------------------- */

#ifndef NO_SMBMOUNT

static void mount_delete_all ()
{
  gtk_clist_clear(GTK_CLIST(mount_clist));
}

static void mount_add_enumerate (mount_struct *mount, gpointer data)
{
  GtkTreeItem *item;
  gchar *texts[2];
  char resource[MAX_REC_LEN+1];
  
  if ( mount != NULL )
  {
    strcpy(resource, "//");
    string_ncat(resource, mount->machine, MAX_REC_LEN);
    string_ncat(resource, "/", MAX_REC_LEN);
    string_ncat(resource, mount->share, MAX_REC_LEN);
  }

  /* add to mountlist */
  texts[0] = resource;
  texts[1] = mount->mountpoint;
  if ( mount_clist && GTK_IS_CLIST(mount_clist) )
  {
    gtk_clist_append(GTK_CLIST(mount_clist), texts);
    gtk_clist_set_selectable(GTK_CLIST(mount_clist), GTK_CLIST(mount_clist)->rows - 1, 0);
  }

  /* change view of the folder in pref hosts */
  item = share_get_by_name(cempty, mount->machine, mount->share);
  string_ncopy(share_current_machine, mount->machine, MAXMACHNAMEL);
  tree_folder_to_mounted(item, mount->mountpoint);
  /* change view in normal browse tree */
  tree_reset_iteration();
  while ( tree_enumerate_share_item(mount->machine, mount->share, &item) )
  {
    tree_folder_to_mounted(item, mount->mountpoint);
  }
}

static void mount_retrigger ()
{
  GtkTreeItem *item;

  /* change the folder to unmounted */
  strcpy(share_current_machine, cempty);
  tree_reset_iteration();
  while ( tree_enumerate_mounted_item(&item) )
  {
    tree_mounted_to_folder(item);
  }
  /* delete all entries in mountlist */
  mount_delete_all();

  /* enumerate all mounts to insert them */
  mount_list_enumerate(mount_add_enumerate, NULL);
}

#endif

/* ------------------------------------------------------------------------- */
