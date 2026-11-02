
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

#ifndef _data_h_
#define _data_h_

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>
#include "define.h"
  
/* ------------------------------------------------------------------------- */

/* callback defines */

/* callback for notifying browse data changes */
typedef void (*data_change_callback)(data_notify_struct *notify);

/* callbacks for host, group, machine and share enumerations */
typedef void (*host_enumerate_callback)(host_struct *host, gpointer data);
typedef void (*group_enumerate_callback)(group_struct *group, gpointer data);
typedef void (*machine_enumerate_callback)(machine_struct *machine, gpointer data);
typedef void (*share_enumerate_callback)(share_struct *share, gpointer data);
typedef void (*mount_enumerate_callback)(mount_struct *mount, gpointer data);
typedef void (*mem_mount_enumerate_callback)(mem_mount_struct *mount, gpointer data);
typedef void (*master_enumerate_callback)(master_struct *master, gpointer data);
typedef void (*groupmaster_enumerate_callback)(group_master_struct *master, gpointer data);

/* ------------------------------------------------------------------------- */

/* initialization functions */
void data_init ();
void data_clean ();

/* change notify functions */
extern void notify_add_callback (data_change_callback callback);
extern void notify_delete_callback (data_change_callback callback);
extern void notify_delete_all ();
extern void notify_start ();
extern void notify_stop ();
extern void notify_mount_change ();
extern void notify_group_change ();
extern void notify_host_change ();
extern void notify_master_change ();
extern void notify_groupmaster_change ();

/* mount list functions */
extern void data_mount_add (char *service, const char *mountpoint);
extern void data_mount_delete (const char *mountpoint);
extern unsigned char mount_list_in_list_mountpoint (const char *mountpoint);
extern guint mount_list_count ();
extern void mount_list_enumerate (mount_enumerate_callback callback, gpointer data);
extern mount_struct *mount_list_search_by_mountpoint (const char *mountpoint);
extern mount_struct *mount_list_search_by_machine_share (const char *machine, const char *share);
#ifndef NO_SMBMOUNT
extern void mount_list_retrigger ();
#endif

/* memorized mounts list functions */
extern void data_mem_mount_add (MEM_MOUNT_TYPE type, const char *mountpoint, char **arglist);
extern void data_mem_mount_delete (const char *mountpoint);
extern guint mem_mount_list_count (void);
extern void mem_mount_list_enumerate (mem_mount_enumerate_callback callback, gpointer data);
extern mem_mount_struct *mem_mount_list_search_by_mpoint(const char *mountpoint);

/* master browser list functions */
extern void data_master_add (const char *name, master_browser_type type);
extern void data_master_delete (const char *name);
extern guint master_list_count ();
extern void master_list_enumerate (master_enumerate_callback callback, gpointer data);
extern master_struct *master_list_search_by_name(const char *name);

/* preferred host list functions */
extern void data_hosts_add (const char *machine, const char *group, const char *ip, host_type type);
extern void data_hosts_delete (const char *machine);
extern void data_hosts_delete_all (void);
extern void data_hosts_shares_add (const char *machine, const char *user, const char *password, GSList * sharecommenttypelist);
extern void data_hosts_machineinfo_set (const char *machine, MACHINEINFO_STRUCT *machineinfo);
extern guint host_list_count (void);
extern void host_list_enumerate (host_enumerate_callback callback, gpointer data);
extern host_struct *host_list_search_by_name(const char *name);

/* group list functions */
extern void data_groups_add (GSList *groupmasterlist, unsigned char append);
extern guint group_list_count ();
extern void group_list_enumerate (group_enumerate_callback callback, gpointer data);
extern void group_list_append_additional_master (const char *group_name);
extern void group_list_master_delete_local (GSList **masterlist, const char *name);
extern group_struct *group_list_search_by_name(const char *name);
extern group_master_struct *group_list_master_search_by_name (GSList *masterlist, const char *name);

/* machine list functions */
extern unsigned char data_machines_add (const char *group, GSList *machinecommentlist, BROWSE_MODE mode);
extern void data_machines_machineinfo_set (const char *group, const char *machine, MACHINEINFO_STRUCT *machineinfo);
extern guint machine_list_count (const char *groupname);
extern void machine_list_enumerate (const char *groupname, machine_enumerate_callback callback, gpointer data);
extern machine_struct *machine_list_search_by_name (GSList *machinelist, const char *name);
extern machine_struct *machine_list_search_by_group_name (const char *group, const char *machine);
extern void machine_list_delete_all (GSList **machinelist);

/* share list functions */
extern unsigned char data_shares_add (const char *group, const char *machine, const char *user, const char *password, GSList * sharecommenttypelist, BROWSE_MODE mode);
extern guint share_list_count (const char *groupname, const char *machinename);
extern void share_list_enumerate (const char *groupname, const char *mschinename, share_enumerate_callback callback, gpointer data);
extern share_struct *share_list_search_by_group_machine_name (const char *group, const char *machine, const char *share);

/* group master browser functions */
extern void data_groupmaster_add (const char *group, const char *name, group_master_type type);
extern void data_groupmaster_delete (const char *group, const char *name);
extern guint groupmaster_list_count ();
extern void groupmaster_list_enumerate (groupmaster_enumerate_callback callback, gpointer data);
extern group_master_struct *groupmaster_list_search_by_workgroup(const char *workgroup, unsigned char reset);
extern group_master_struct *groupmaster_list_search_by_workgroup_name (const char *workgroup, const char *master);

/* ------------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif    /* _data_h_ */
