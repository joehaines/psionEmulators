
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

#ifndef _browsewrap_h_
#define _browsewrap_h_

#ifdef __cplusplus
extern "C" {
#endif

#include "define.h"

/* ------------------------------------------------------------------------- */

typedef void (*browse_groups_callback)(GSList *groupmasterlist,
                                       BROWSE_MODE mode);

typedef void (*browse_machines_callback)(const char *group_name,
                                       GSList * machinecommentlist,
                                       BROWSE_MODE mode);

typedef void (*browse_shares_callback)(const char *group_name,
                                       const char *machine_name,
                                       const char *user,
                                       const char *password,
                                       MACHINEINFO_STRUCT *machineinfo,
                                       GSList *sharecommenttypelist,
                                       BROWSE_MODE mode);

typedef void (*browse_path_callback)(  const char *group_name,
                                       const char *machine_name,
                                       const char *share_name,
                                       const char *path_name,
                                       GSList *files,
                                       BROWSE_MODE mode);

typedef void (*browse_network_wait_callback)(void);

typedef struct _GROUPS_BROWSE_INFO {
        BROWSE_MODE mode;
        unsigned int browse_id;
        browse_groups_callback callback;
} GROUPS_BROWSE_INFO;

typedef struct _MACHINES_BROWSE_INFO {
        BROWSE_MODE mode;
        unsigned int browse_id;
        browse_machines_callback callback;
} MACHINES_BROWSE_INFO;

typedef struct _SHARES_BROWSE_INFO {
        BROWSE_MODE mode;
        unsigned int browse_id;
        browse_shares_callback callback;
} SHARES_BROWSE_INFO;

typedef struct _PATH_BROWSE_INFO {
        BROWSE_MODE mode;
        unsigned int browse_id;
        browse_path_callback callback;
} PATH_BROWSE_INFO;

typedef union _BROWSE_INFO {
        GROUPS_BROWSE_INFO g_info;
        MACHINES_BROWSE_INFO h_info;
        SHARES_BROWSE_INFO s_info;
        PATH_BROWSE_INFO p_info;
} BROWSE_INFO;

/* ------------------------------------------------------------------------- */

typedef struct _BL_REFERENCE {
        unsigned int type;
        BROWSE_MODE mode;
} BL_REFERENCE;

typedef struct _BL_GROUPS_ENTRY {
        unsigned int type;
        BROWSE_MODE mode;
        char group[MAXGROUPNAMEL+1];
        char browser[MAXMACHNAMEL+1];
        browse_groups_callback callback;
} BL_GROUPS_ENTRY;

typedef struct _BL_MACHINES_ENTRY {
        unsigned int type;
        BROWSE_MODE mode;
        char group[MAXGROUPNAMEL+1];
        char master[MAXMACHNAMEL+1];
        char domain[MAXGROUPNAMEL+1];
        browse_machines_callback callback;
} BL_MACHINES_ENTRY;

typedef struct _BL_SHARES_ENTRY {
        unsigned int type;
        BROWSE_MODE mode;
        char group[MAXGROUPNAMEL+1];
        char machine[MAXMACHNAMEL+1];
        char domain[MAXGROUPNAMEL+1];
        char ip[MAXIPL+1];
        browse_shares_callback callback;
} BL_SHARES_ENTRY;

typedef struct _BL_PATH_ENTRY {
        unsigned int type;
        BROWSE_MODE mode;
        char group[MAXGROUPNAMEL+1];
        char machine[MAXMACHNAMEL+1];
        char domain[MAXGROUPNAMEL+1];
        char ip[MAXIPL+1];
        char path[MAXSHAREPATHL+1];
        char user[USER_LEN+1];
        char password[PASSWORD_LEN+1];
        browse_path_callback callback;
} BL_PATH_ENTRY;

typedef union _BROWSE_LIST_ENTRY {
        BL_REFERENCE r_entry;
        BL_GROUPS_ENTRY g_entry;
        BL_MACHINES_ENTRY h_entry;
        BL_SHARES_ENTRY s_entry;
        BL_PATH_ENTRY p_entry;
} BROWSE_LIST_ENTRY;

/* ------------------------------------------------------------------------- */

extern void GetSMBGroups_done ( GSList *groupmasterlist,
                                GROUPS_BROWSE_INFO *browse_info,
                                GetSMBStateType state );

extern void GetSMBMachines_done  ( const char *group_name,
                                   GSList *machinecommentlist,
                                   MACHINES_BROWSE_INFO *browse_info,
                                   GetSMBStateType state );

extern void GetSMBShares_done ( char *group_name,
                                char *machine_name,
                                char *user,
                                char *password,
                                MACHINEINFO_STRUCT *machineinfo,
                                GSList *sharecommenttypelist,
                                SHARES_BROWSE_INFO *browse_info,
                                GetSMBStateType state );

extern void GetSMBPath_done (   char *group_name,
                                char *machine_name,
                                char *share_name,
                                char *path_name,
                                GSList *files,
                                PATH_BROWSE_INFO *browse_info,
                                GetSMBStateType state);

extern void LookupByName_done (char * machine_name, char *group_name, char *ipaddr,GetSMBStateType state);
extern void LookupByIP_done ( char *ipaddr,char * machine_name, char *group_name,GetSMBStateType state);

/* ------------------------------------------------------------------------- */

/* initialization functions */
extern void browse_init ();
extern void browse_clean ();

/* browse limit functions */
extern unsigned int browse_job_get_max ();
extern void browse_job_set_max (unsigned int jobs);

/* browsing functions */
extern void browse_groups ( BROWSE_MODE mode );

extern unsigned char browse_machines ( const char *groupname,
                                       BROWSE_MODE mode);

extern void browse_shares ( const char *groupname, const char *machinename,
                            BROWSE_MODE mode);

extern void browse_path (share_window_struct *sw, BROWSE_MODE mode);

extern void browse_network (BROWSE_MODE mode);
extern void browse_network_wait_done (browse_network_wait_callback callback);

/* ------------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif  /* _browsewrap_h_ */
