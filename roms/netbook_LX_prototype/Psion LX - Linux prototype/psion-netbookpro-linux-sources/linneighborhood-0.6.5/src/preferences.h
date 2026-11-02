
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

#ifndef _preferences_h_
#define _preferences_h_

#ifdef __cplusplus
extern "C" {
#endif

#include "define.h"

/* ------------------------------------------------------------------------- */

extern global_struct globals;               /* global variables */
extern pref_struct pref;                    /* global preferences */
extern size_struct size;                    /* window geometry */
extern startup_struct startup;              /* startup parameters */

/* ------------------------------------------------------------------------- */

extern unsigned char load_lmhosts_file (const char *file);

/* ------------------------------------------------------------------------- */

extern void globals_init (void);

extern void preferences_load (pref_struct *pref);
extern void preferences_save (pref_struct *pref);

extern void add_master_load (void);
extern void add_master_save (void);

extern void groupmaster_load (void);
extern void groupmaster_save (void);

extern void prefhost_load (void);
extern void prefhost_save (void);

extern void password_load (void);
extern void password_save (void);

extern void mountpoint_save (void);
extern GSList *mountpoint_load (void);

extern void size_save (size_struct *size);
extern void size_load (size_struct *size);

extern void startup_parse_command_line (int argc, char *argv[], startup_struct *startup);

/* ------------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif  /* _preferences_h_ */
