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

#ifndef _gtk_dialog_h_
#define _gtk_dialog_h_

#ifdef __cplusplus
extern "C" {
#endif


#include "define.h"
#include <gtk/gtktreeitem.h>

/* ------------------------------------------------------------------------- */

typedef struct _PREF_MACHINE_DIALOG_STRUCT {
        unsigned char is_edit;
        GtkTreeItem *pref_machine;
        unsigned char query_ok;
        prefhost_struct prefhost;
} PREF_MACHINE_DIALOG_STRUCT;

typedef void (*dual_input_ok_callback)(const char *input1, const char *input2);

typedef struct _DUAL_INPUT_DIALOG_STRUCT {
        char *title;
        char *text1;
        char *text2;
        char *input1;
        char *input2;
        guint16 maxlen1;
        guint16 maxlen2;
        dual_input_ok_callback ok_callback;
} DUAL_INPUT_DIALOG_STRUCT;

typedef struct _dialog_pointer_struct {
        gpointer data;
        GtkWidget *dialog;
} dialog_pointer_struct;

typedef struct _file_copy_progress_struct {
        GtkWidget *dialog;
        GtkWidget *progress;
        unsigned char *do_exit;
} file_copy_progress_struct;

/* ------------------------------------------------------------------------- */

extern void about_dialog (void);
extern void user_browse_dialog (unsigned char with_domain);

extern unsigned char log_window_active (void);
extern void log_window_toggle (void);
extern void log_window_dialog (void);
extern void insert_log_string (char * log, unsigned char add_linefeed);

extern void enter_pref_machine (void);
extern void edit_pref_machine (GtkTreeItem *item, char *name, char *group, char *ip);

extern void machine_info_dialog (char *name, MACHINEINFO_STRUCT *machineinfo);
extern void browse_network_image_dialog (void);
extern void export_network_image_dialog (void);
extern void preferences_dialog (void);
extern void export_mountscript_dialog (void);

extern void master_browser_dialog (void);
extern void group_master_dialog (void);
extern void dual_input_dialog (DUAL_INPUT_DIALOG_STRUCT *dialog_struct);

extern file_copy_progress_struct *dialog_file_copy_progress_create (unsigned char *do_cancel);
extern void dialog_file_copy_progress_update (file_copy_progress_struct *copy_struct, gfloat value);


extern void dialog_pointer_struct_delete (dialog_pointer_struct *d_struct);

extern void dialog_ok_cancel (char *title, char *text, GtkSignalFunc ok_function, GtkSignalFunc cancel_function, gpointer data, int data_length);
extern void dialog_file_overwrite (GtkSignalFunc ok_function, GtkSignalFunc cancel_function, gpointer data, int data_length);

/* ------------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif   /* _gtk_dialog_h_ */
