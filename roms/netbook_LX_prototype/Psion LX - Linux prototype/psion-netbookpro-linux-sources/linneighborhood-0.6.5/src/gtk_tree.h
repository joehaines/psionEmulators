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

#ifndef _gtk_tree_h_
#define _gtk_tree_h_

#ifdef __cplusplus
extern "C" {
#endif


#include <gtk/gtk.h>
#include "define.h"

/* ------------------------------------------------------------------------ */

extern unsigned char is_password_once;
extern GtkTreeItem *clicked_item;

extern void callback_stop (GtkWidget *widget, gpointer data);
extern void callback_mount (GtkWidget *widget, gpointer data);
extern void callback_umount (GtkWidget *widget, gpointer data);

extern void group_delete_all (void);

extern void tree_scan_item (BROWSE_MODE mode);
extern void tree_create (GtkWidget *vbox);
extern void tree_focus_root_item (void);

/* ------------------------------------------------------------------------ */

#ifdef __cplusplus
}
#endif

#endif   /* _gtk_tree_h_ */
