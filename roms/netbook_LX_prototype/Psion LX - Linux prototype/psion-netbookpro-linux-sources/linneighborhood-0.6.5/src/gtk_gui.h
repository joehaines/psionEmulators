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

#ifndef _gtk_gui_h_
#define _gtk_gui_h_

#ifdef __cplusplus
extern "C" {
#endif


extern GtkWidget *main_window;

/* ------------------------------------------------------------------------- */

extern GtkWidget * new_pixmap (GtkWidget * widget, const char **data);

extern void status_set_status_scan (unsigned char scan);
extern void status_set_status_scan_text (char *text);
extern void status_set_status_text (unsigned int text);
extern void status_set_status (char * status);
extern void status_set_object_text (unsigned int objects);
extern void status_set_logging_text (void);

extern void gtk_gui_set_locale (void);
extern void gtk_gui_init (int argc, char *argv[]);
extern void gtk_gui_start ();

extern int scan_mouse_press (GdkEventButton * event, guint32 * last_time);
extern void show_popup_menu (GtkItemFactoryEntry *menu_item, int items, int x, int y);
extern void change_win_cursor (GdkWindow * win, GdkCursorType cursortype);

#define MOUSE_NO_EVENT            0
#define MOUSE_LEFT_SINGLE         1
#define MOUSE_LEFT_DOUBLE         2
#define MOUSE_MIDDLE_SINGLE       3
#define MOUSE_MIDDLE_DOUBLE       4
#define MOUSE_RIGHT_SINGLE        5
#define MOUSE_RIGHT_DOUBLE        6


extern void ShowMessageBox (char *title, char *text);


/* -- button definitions -- */

#define OK_B                0x00000001
#define YES_B               0x00000002
#define CONTINUE_B          0x00000004
#define RETRY_B             0x00000008
#define CANCEL_B            0x00000010
#define NO_B                0x00000020
#define ABORT_B             0x00000040


typedef void (*msgdlg_callback)(unsigned int result);
typedef void (*txtdlg_callback)(char *input);

extern void ShowMessageDialog (const char *title,
                               const char *text,
                               unsigned int buttons,
                               msgdlg_callback callback
                               );

extern void TextInputDialog (const char *title,
                             const char *text,
                             char *input,
                             int input_maxlen,
                             int w,
                             int h,
                             txtdlg_callback callback
                             );

extern void set_toolbar_tooltip_color (GtkToolbar *toolbar);

/* ------------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif   /* _gtk_gui_h_ */
