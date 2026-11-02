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

#ifndef _gtk_mountdlg_h_
#define _gtk_mountdlg_h_

#include "define.h"

/* ------------------------------------------------------------------------- */

extern void smb_mount_dialog (const char *servicename, const char *ipaddr,const char *group_name, unsigned char show_gui);
extern void smb_umount_dialog (char *mount_point);
extern void smb_mount_dialog_init(void); 
extern void gtk_smbumount_done(char *mount_point,char *errstr,smbmountStateType smbmountState,int tag);

GtkWidget *message_box (char        *message,
                        GtkCallback  callback,
                        gpointer     data);

/* ------------------------------------------------------------------------- */

#endif /* _gtk_mountdlg_h_ */
