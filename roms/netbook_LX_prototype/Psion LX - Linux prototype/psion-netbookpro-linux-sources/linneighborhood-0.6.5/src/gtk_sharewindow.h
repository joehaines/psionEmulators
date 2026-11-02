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

#ifndef _gtk_sharewindow_h_
#define _gtk_sharewindow_h_

#ifdef __cplusplus
extern "C" {
#endif


#include <gtk/gtk.h>
#include "define.h"

/* ------------------------------------------------------------------------- */

extern GtkWidget *share_window_new (share_window_struct *sw_struct);

extern unsigned char share_window_is_local_file (share_window_struct *sw_struct);
extern GtkWidget *share_window_already_exists (share_window_struct *sw_struct);
extern GtkWidget *share_window_get_window (const char *group, const char *machine, const char *share,
                                            const char *path, unsigned char reset);
extern GtkWidget *share_window_get_window_by_clist (GtkCList *clist, unsigned char reset);
extern GtkWidget *share_window_get_local_window (const char *path, unsigned char reset);
extern void share_window_clean (void);
extern void share_window_add_files (GtkWidget *window, GSList *files);
extern void share_window_copy_smb_to_local(share_file_struct f_struct);
extern share_window_struct *share_window_get_struct (GtkWidget *window);
extern void share_window_list_set_cursor (GdkCursorType cursortype);

/* ------------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif  /* _gtk_sharewindow_h_ */
