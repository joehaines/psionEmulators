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
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "define.h"
#include "io.h"
#include "utility.h"
#include "data.h"
#include "preferences.h"

/* ------------------------------------------------------------------------- */

#define IO_TEXT_LEN        MASTER_LEN

char networkimage_file[PATH_MAX] = SMBNETWORK_FILE;

static char io_text[IO_TEXT_LEN+1];

/* ------------------------------------------------------------------------- */

/* -- basic file routines -- */

/* opens a file for writing (creates if not exist), truncates to zero length */
FILE* io_write_open_zero (char *filename, unsigned char show_error)
{
  FILE* fd;
  
  fd = fopen(filename, "w+");
  
  if ( !fd)
  {
    if ( show_error )
    {
      g_print(_("cannot open/create %s !\n"), filename);
    }
  }
  
  return fd;
}

/* ------------------------------------------------------------------------- */

/* -- network image io functions -- */

void io_write_network_image_header (FILE *fd)
{
  string_ncopy(io_text, _("\n# SMB Network Image\n"), IO_TEXT_LEN);
  fwrite(io_text, 1, strlen(io_text), fd);
  string_ncopy(io_text, _("# created by LinNeighborhood\n\n"), IO_TEXT_LEN);
  fwrite(io_text, 1, strlen(io_text), fd);
}

void io_write_network_image_workgroup (FILE *fd, char *workgroup)
{
  string_ncopy(io_text, _("\n[WORKGROUP] "), IO_TEXT_LEN);
  string_ncat(io_text, workgroup, IO_TEXT_LEN);
  string_ncat(io_text, "\n", IO_TEXT_LEN);
  fwrite(io_text, 1, strlen(io_text), fd);
}

void io_write_network_image_machine (FILE *fd, char *machine)
{
  string_ncopy(io_text, _("\n    [MACHINE] "), IO_TEXT_LEN);
  string_ncat(io_text, machine, IO_TEXT_LEN);
  string_ncat(io_text, "\n", IO_TEXT_LEN);
  fwrite(io_text, 1, strlen(io_text), fd);
}

void io_write_network_image_shareheader (FILE *fd)
{
  string_ncopy(io_text, _("        [SHARES]\n"), IO_TEXT_LEN);
  fwrite(io_text, 1, strlen(io_text), fd);
}

void io_write_network_image_printerheader (FILE *fd)
{
  string_ncopy(io_text, _("        [PRINTERS]\n"), IO_TEXT_LEN);
  fwrite(io_text, 1, strlen(io_text), fd);
}

void io_write_network_image_share (FILE *fd, char *share)
{
  string_ncopy(io_text, "            ", IO_TEXT_LEN);
  string_ncat(io_text, share, IO_TEXT_LEN);
  string_ncat(io_text, "\n", IO_TEXT_LEN);
  fwrite(io_text, 1, strlen(io_text), fd);
}

/* ------------------------------------------------------------------------- */

/* -- mount script functions -- */

static void io_write_mount_script_header (FILE *fd)
{
  string_ncopy(io_text, "#! /bin/sh\n\n", IO_TEXT_LEN);
  fwrite(io_text, 1, strlen(io_text), fd);
  string_ncopy(io_text, _("# SMB mount script\n"), IO_TEXT_LEN);
  fwrite(io_text, 1, strlen(io_text), fd);
  string_ncopy(io_text, _("# created by LinNeighborhood\n\n"), IO_TEXT_LEN);
  fwrite(io_text, 1, strlen(io_text), fd);
}

static void mount_script_save_enumerate (mem_mount_struct *mount, gpointer data)
{
  FILE *fd;
  char str[MAXMACHNAMEL+MAXSHRNAMEL+PATH_LEN+20];
  char *temp;
  
  fd = (FILE*)data;
  if ( fd != NULL )
  {
    /* save only 'not root' mounts -> plain password */
    if ( (mount != NULL) && (mount->type != mem_mount_root) )
    {
      string_ncopy(str, pref.v.smbmount_exe, MAXMACHNAMEL+MAXSHRNAMEL+PATH_LEN+19);
      temp = str;
      temp += strlen(str);
      merge_mount(&temp, mount, MAXMACHNAMEL+MAXSHRNAMEL+PATH_LEN+19 - strlen(str), 1);
      string_ncat(str, "\n", MAXMACHNAMEL+MAXSHRNAMEL+PATH_LEN+19);
      fwrite(str, 1, strlen(str), fd);
    }
  }
}

void io_write_mount_script (char *filename)
{
  FILE *fd;

  fd = fopen(filename, "w+");
    
  if ( !fd )
  {
    g_print(_("cannot export mount script %s !\n"), filename);
    return;
  }
    
  chmod(filename, S_IRUSR | S_IWUSR | S_IXUSR);

  io_write_mount_script_header(fd);

  /* enumerate all stored mountpoints */
  mem_mount_list_enumerate(mount_script_save_enumerate, (gpointer)(fd));
  
  fclose(fd);
}

/* ------------------------------------------------------------------------- */
