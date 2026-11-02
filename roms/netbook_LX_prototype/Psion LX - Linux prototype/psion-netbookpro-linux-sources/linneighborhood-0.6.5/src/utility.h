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

#ifndef _utility_h_
#define _utility_h_

#ifdef __cplusplus
extern "C" {
#endif

#include <gdk/gdk.h>
#include <limits.h>
#include <string.h>
#include "define.h"

/* ------------------------------------------------------------------------- */

extern char cempty[];
extern char temp_path[PATH_MAX+1];

/* ------------------------------------------------------------------------- */

extern void slist_free_with_data (GSList **list);

extern unsigned char is_tab (char c);
extern unsigned char whitespace(char token);
extern char *read_until_char (char *mem, char token);
extern char *read_until_whitespace (char *mem);
extern char *read_after_whitespace (char *mem);
extern char *read_until_last_whitespace (char *mem);
extern unsigned char whitespace_sameline(char token);
extern char *skip_nonspaces (char *s);

extern void string_ncopy(char *dest, const char *src, size_t len);
extern void string_ncat(char *dest, const char *src, size_t len);
extern void string_trim (char **str);
extern void string_trim_left (char **str);
extern void string_trim_right (char *str);
extern void string_replace_char_by_char (char *str, char old, char replace);

extern int tokenize_string(char **dest, char *src, int max_token);

extern char *mangle(unsigned char *s);
extern void unmangle (char *dest, char *s, int size);
extern unsigned char is_empty_string (const char * str);
extern unsigned char is_ipv4_address (const char * str);
extern unsigned char file_exist (const char * filename);
extern unsigned char path_exist (const char *path);
extern unsigned char path_create (const char *path, mode_t modus);
extern unsigned char path_check_create (const char *path, mode_t modus);

extern unsigned char compare_smb_groupname (const char *group1, const char *group2);
extern unsigned char compare_smb_machinename (const char *mach1, const char *mach2);
extern unsigned char compare_smb_sharename (const char *share1, const char *share2);
extern unsigned char compare_path (const char * path1, const char * path2);

extern void merge_smb_service (char **service, char *machine, char *share,  size_t len);
extern void extract_smb_service (char *service, char **machine, char **share);

extern void merge_prefhost (char **host, char *name, char *group, char *ip, size_t len);
extern void extract_prefhost (char *host, char **name, char **group, char **ip);

extern void merge_groupmaster (char **cmaster, group_master_struct *master, size_t len);
extern void extract_groupmaster (char *cmaster, char **group, char **name);

extern void merge_mount (char **cmount, mem_mount_struct *mount, size_t len, int first_arg);
extern void extract_mount (char *cmount, mem_mount_struct *mount);
extern void utility_do_mount (mem_mount_struct *mount);
extern void prepare_mount_path (char *path);

extern int get_textcase_in_gslist (GSList *list, char *text);
extern void makro_replace(char *target, char *source, int maxlen);
extern void postmount_execute (char *mountpoint);

extern char *pref_get_root_mnt_dir (void);
extern char *pref_get_filemanager_exe (void);
extern char *pref_get_smbclient_exe (void);
extern char *pref_get_nmblookup_exe (void);
extern char *pref_get_smbmount_exe (void);
extern char *pref_get_smbumount_exe (void);

extern void network_image_export (char *filename);

/* ------------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif		/* _utility_h_ */
