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

#ifndef _io_h_
#define _io_h_

#ifdef __cplusplus
extern "C" {
#endif


#define SMBNETWORK_FILE  "smbnetwork"

extern char networkimage_file[PATH_MAX];

/* ------------------------------------------------------------------------- */

extern FILE* io_write_open_zero (char *filename, unsigned char show_error);

/* ------------------------------------------------------------------------- */

extern void io_write_network_image_header (FILE *fd);
extern void io_write_network_image_workgroup (FILE *fd, char *workgroup);
extern void io_write_network_image_machine (FILE *fd, char *machine);
extern void io_write_network_image_shareheader (FILE *fd);
extern void io_write_network_image_printerheader (FILE *fd);
extern void io_write_network_image_share (FILE *fd, char *share);

extern void io_write_mount_script (char *filename);

/* ------------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif  // _io_h_
