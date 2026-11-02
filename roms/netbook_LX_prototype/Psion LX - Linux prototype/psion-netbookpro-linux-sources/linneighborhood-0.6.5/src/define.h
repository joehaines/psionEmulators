
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

#ifndef _define_h_
#define _define_h_


#include "config.h"


#define NO_LIBSMB


#include <libintl.h>
#include <sys/types.h>
#include <glib.h>

#define _(x) gettext(x)

#ifdef gettext_noop
# define N_(x) gettext_noop(x)
#else
# define N_(x) (x)
#endif

/* dummy prepare for i18n */
/*#define _(x) (x)
#define N_(x) (x)
#define textdomain(Domain)
#define bindtextdomain(Package, Directory)*/

/* Include file for any global defines and types */

#ifndef PACKAGE
  #define PACKAGE "linneighborhood"
#endif
#ifndef LOCALEDIR
  #define LOCALEDIR "/usr/share/locale"
#endif

/*#define NO_SMBMOUNT*/

#define USE_LIBSMBCLIENT         1

/* ------------------------------------------------------------------------- */

/* path definitions */

#define GLOBAL_PATH             "/LinNeighborhood/"            /* global configuration path */
#define GLOBAL_PATH_DPCTD       "/linneighborhood/"            /* old (deprecated) global configuration path */
#define LOCAL_PATH              ".LinNeighborhood/"            /* local configuration path, relative from $HOME */
#define LOCAL_PATH_DPCTD        ".linneighborhood/"            /* old (deprecated) local configuration path */

#define PREF_FILE               "preferences"                  /* name of preferences file */
#define SIZE_FILE               "size"                         /* name of the file for the window geometry */
#define HOST_FILE               "hosts"                        /* file for preferred hosts */
#define MASTER_FILE             "masterbrowsers"               /* file for additional master browsers */
#define GROUPMASTER_FILE        "groupmasters"                 /* file for additional group masters */
#define MOUNTPOINT_FILE         "mountpoints"                  /* file for memorized mounts */
#define PASSWORD_FILE           "password"                     /* file for default password */

/* definitions of the default window geometry */

#define MAIN_X_DEF                60                           /* main window's X-position */
#define MAIN_Y_DEF                40                           /* Y-Position */
#define MAIN_W_DEF                450                          /* width */
#define MAIN_H_DEF                330                          /* height */
#define TREE_COMMENT_DEF          180                          /* tree's comment position */
#define TREE_MOUNTPOINT_DEF       180                          /* tree's relative mountpoint position */
#define MOUNT_LIST_H_DEF          80                           /* mount list's height */
#define MOUNT_MPOINT_DEF          180                          /* mountpoint column position */

#define MAIN_X_MIN                0
#define MAIN_X_MAX                780
#define MAIN_Y_MIN                0
#define MAIN_Y_MAX                580
#define MAIN_W_MIN                400                          /* minimum/maximum values */
#define MAIN_W_MAX                1500
#define MAIN_H_MIN                330
#define MAIN_H_MAX                1200
#define TREE_COMMENT_MIN          90
#define TREE_MOUNTPOINT_MIN       90
#define MOUNT_LIST_H_MIN          40
#define MOUNT_MPOINT_MIN          120

#define TIMER_INTERVAL_UPDATE     5000                         /* in ms for mountlist update */

/* definitions for browsing modes and flags */

typedef unsigned int BROWSE_MODE;

#define BROWSE_MODE_NONE                0x0000    /* default mode */
#define BROWSE_MODE_USER                0x0001    /* browse as user */
#define BROWSE_MODE_EXPAND              0x0002    /* expand tree after browse */
#define BROWSE_MODE_QUICK               0x0004    /* do a quick browse */
#define BROWSE_MODE_PREF_HOST           0x0008    /* browse a preferred host */
#define BROWSE_MODE_NETWORK             0x0010    /* complete network browse */
#define BROWSE_MODE_WORKGROUP           0x0020    /* browse with workgroup */
#define BROWSE_MODE_WINS                0x0040    /* browse with WINS server */
#define BROWSE_MODE_PRIM_MASTER         0x0080    /* browse with primary master browser */
#define BROWSE_MODE_ADD_MASTER          0x0100    /* browse with additional master browser */
#define BROWSE_MODE_NO_GUI              0x0200    /* browse without GUI output */
#define BROWSE_MODE_DOMAIN              0x0400    /* browse with domain name */
#define BROWSE_MODE_FILE_SYSTEM         0x0800    /* browse of the local file system */

/* ------------------------------------------------------------------------- */

#define MAX_BROWSE_JOBS           20

#define MASTER_LEN                63    /* often used lengths */
#define PATH_LEN                 255
#define ARG_LEN                  255
#define MAX_REC_LEN              127
#define SERVICE_LEN               63    /* max length of service */
#define USER_LEN                  25
#define PASSWORD_LEN              25

#define MAXGROUPNAMEL             25    /* max CIFS length of a group name */
#define MAXMACHNAMEL              25    /* max CIFS length of a machine name */
#define MAXSHRNAMEL               20    /* max CIFS length of a share name */
#define MAXCOMMENTL              100    /* max CIFS length of a comment */
#define MAXIPL                    15    /* max IP adress length "xxx.xxx.xxx.xxx" */
#define MAXFILEL                 255    /* max length of file name */
#define MAXSHAREPATHL            255    /* max length of share name path */
#define MAXSMBURLL             (MAXSHAREPATHL + MAXMACHNAMEL + 10)

#define MAX_FILE_LEN           60000    /* max allowable length of preferences text file */
#define MAX_LINE_LEN             255    /* max length of a line in a text file */

/* status texts */
#define TEXT_NOTHING      0
#define TEXT_IDLE         1
#define TEXT_SMBERR       2
#define TEXT_EXEERR       3

/* ------------------------------------------------------------------------- */

typedef enum _SHARE_TYPE {
        shtype_folder,
        shtype_printer
} SHARE_TYPE;

typedef enum {
        SMB_STATE_OK,
        SMB_STATE_SMBERR,    /* cannot browse machine */
        SMB_STATE_EXEERR     /* smbclient not found */
} GetSMBStateType;

typedef enum {
        mem_mount_none,
        mem_mount_user,
        mem_mount_root
} MEM_MOUNT_TYPE;

typedef enum {
        MOUNT_STATE_OK,
        MOUNT_STATE_ERR
} smbmountStateType;

typedef enum {
        host_local,
        host_global
} host_type;

typedef enum {
        master_browser_local,
        master_browser_global
} master_browser_type;

/* ------------------------------------------------------------------------- */

/* defs for browsing + mounting */

typedef struct _GROUPMASTER_STRUCT {
        char group_name[MAXGROUPNAMEL+1];
        char master_browser[MAXMACHNAMEL+1];
} GROUPMASTER_STRUCT;

typedef struct _MACHINECOMMENT_STRUCT {
        char entry_name[MAXMACHNAMEL+1];
        char entry_comment[MAXCOMMENTL+1];
} MACHINECOMMENT_STRUCT;

typedef struct _SHARECOMMENTTYPE_STRUCT {
        char entry_name[MAXSHRNAMEL+1];
        char entry_comment[MAXCOMMENTL+1];
        SHARE_TYPE share_type;
} SHARECOMMENTTYPE_STRUCT;

typedef struct _MACHINEINFO_STRUCT {
        char ipaddr[MAXIPL+1];
        char Domain[MAXGROUPNAMEL+1];
        char OS[MAXCOMMENTL+1];
        char Server[MAXCOMMENTL+1];
} MACHINEINFO_STRUCT;

typedef struct _SMBFS_STRUCT {
        char service[SERVICE_LEN+1];
        char mountpoint[PATH_LEN+1];
} SMBFS_STRUCT;

typedef enum _LINUX_VERSION {
        LINUX_2_0,
        LINUX_2_2
} LINUX_VERSION;

typedef enum _SMBMOUNT {
        SMBMOUNT_204,
        SMBMOUNT_205,
        SMBMOUNT_206
} SMBMOUNT;

typedef enum _ITEMTYPE {
        TYPE_NONE,
        TYPE_ROOT,
        TYPE_GROUP,
        TYPE_MACHINE,
        TYPE_HOST,
        TYPE_FOLDER,
        TYPE_OPEN_FOLDER,
        TYPE_PRINTER,
        TYPE_SMB_DIRECTORY,
        TYPE_SMB_FILE,
        TYPE_LOCAL_DIRECTORY,
        TYPE_LOCAL_FILE
} ITEMTYPE;

typedef enum _SMB_ACTION {
        SMB_ACTION_NONE,
        SMB_ACTION_COPY_SMB_TO_LOCAL
} SMB_ACTION;

/* ------------------------------------------------------------------------- */

typedef struct _prefhost_struct {
        char machine[MAXMACHNAMEL+1];
        char group[MAXGROUPNAMEL+1];
        char ip[MAXIPL+1];
} prefhost_struct;

typedef struct _mount_struct {
        char machine[MAXMACHNAMEL+1];
        char share[MAXSHRNAMEL+1];
        char mountpoint[PATH_LEN+1];
} mount_struct;

typedef struct _mem_mount_struct {
        MEM_MOUNT_TYPE type;
        char mountpoint[PATH_LEN+1];
        GSList *arglist;
} mem_mount_struct;

typedef struct _master_struct {
        master_browser_type type;
        char name[MAXMACHNAMEL+1];
} master_struct;

typedef struct _makro_struct {
        char *identifier;
        char *replace_string;
} makro_struct;

typedef enum {
        type_groups_browse,                /* returned from groups browse */
        type_user_add_local,               /* added by the user */
        type_user_add_global               /* added by the sysadmin */
} group_master_type;

typedef struct _group_master_struct {
        group_master_type type;
        char name[MAXMACHNAMEL+1];
        char group[MAXGROUPNAMEL+1];
} group_master_struct;

/* ------------------------------------------------------------------------- */

/* defs for browsing data */

typedef enum _browse_type_t {
        bt_smb,
        bt_libsmb
} browse_type_t;

typedef enum _DATA_TYPE {
        dt_pref_host,
        dt_group,
        dt_machine,
        dt_share,
        dt_share_pref_host,
        dt_mount,
        dt_master,
        dt_groupmaster
} DATA_TYPE;

typedef struct _dt_struct {
        DATA_TYPE dt;
} dt_struct;

typedef struct _host_struct {
        DATA_TYPE dt;
        host_type type;
        char name[MAXMACHNAMEL+1];
        char comment[MAXCOMMENTL+1];
        char group[MAXGROUPNAMEL+1];
        char browse_user[USER_LEN+1];
        char browse_password[PASSWORD_LEN+1];
        char ipaddr[MAXIPL+1];
        char domain[MAXGROUPNAMEL+1];
        char os[MAXCOMMENTL+1];
        char server[MAXCOMMENTL+1];
        GSList *sharelist;
} host_struct;

typedef struct _group_struct {
        DATA_TYPE dt;
        char name[MAXGROUPNAMEL+1];
        GSList *masterlist;
        GSList *machinelist;
        unsigned int pending_jobs;
} group_struct;

typedef struct _machine_struct {
        DATA_TYPE dt;
        char name[MAXMACHNAMEL+1];
        char comment[MAXCOMMENTL+1];
        char browse_user[USER_LEN+1];
        char browse_password[PASSWORD_LEN+1];
        char ipaddr[MAXIPL+1];
        char domain[MAXGROUPNAMEL+1];
        char os[MAXCOMMENTL+1];
        char server[MAXCOMMENTL+1];
        group_struct *group;
        GSList *sharelist;
} machine_struct;

typedef struct _share_struct {
        DATA_TYPE dt;
        char name[MAXSHRNAMEL+1];
        char comment[MAXCOMMENTL+1];
        SHARE_TYPE st;
        gpointer machine;
        GSList *folderlist;
} share_struct;

typedef struct _share_window_struct {
        BROWSE_MODE mode;
        char group[MAXGROUPNAMEL+1];
        char machine[MAXMACHNAMEL+1];
        char share[MAXSHRNAMEL+1];
        char path[MAXSHAREPATHL+1];
        char ip[MAXIPL+1];
        char user[USER_LEN+1];
        char password[PASSWORD_LEN+1];
} share_window_struct;

typedef struct _share_file_struct {
        char group[MAXGROUPNAMEL+1];
        char machine[MAXMACHNAMEL+1];
        char share[MAXSHRNAMEL+1];
        char path[MAXSHAREPATHL+1];
        char filename[MAXFILEL+1];
        char user[USER_LEN+1];
        char password[PASSWORD_LEN+1];
} share_file_struct;

#define FILE_ICON_HEIGHT                17

typedef struct _file_struct {
        unsigned int name_length;
        char *name;
        guint64 size;
        unsigned char hour;
        unsigned char minute;
        unsigned char day;
        unsigned char month;
        unsigned short year;
        unsigned char attr;
} file_struct;

#define FILE_ATTR_NUMBER                4

#define FILE_ATTR_HIDDEN                0x01
#define FILE_ATTR_READONLY              0x02
#define FILE_ATTR_ARCHIVE               0x04
#define FILE_ATTR_DIRECTORY             0x08

/* ---------- */

typedef struct _data_notify_struct {
        DATA_TYPE dt;
        const char *group;
        const char *machine;
        gpointer data;
} data_notify_struct;

/* ------------------------------------------------------------------------- */

/* structure of the command line (startup) parameters */

#define ST_USER_LEN                31
#define ST_PASSWORD_LEN            31

#define YES         1
#define NO          0

/* structure of available startup parameters */

typedef struct _startup_value_struct {
        unsigned char show_help;
        unsigned char restore_shares;
        unsigned char print_version;
        unsigned char start_iconify;
        unsigned char network_image;
        char netimage_file[PATH_LEN+1];
        unsigned char is_user;
        unsigned char is_password;
        char user[USER_LEN+1];
        char password[PASSWORD_LEN+1];
        unsigned int job;
} startup_value_struct;

typedef struct _startup_bool_struct {
        unsigned char show_help;
        unsigned char restore_shares;
        unsigned char print_version;
        unsigned char start_iconify;
        unsigned char network_image;
        unsigned char netimage_file;
        unsigned char is_user;
        unsigned char is_password;
        unsigned char user;
        unsigned char password;
        unsigned char job;
} startup_bool_struct;

typedef struct _startup_struct {
        startup_value_struct v;
        startup_bool_struct b;
} startup_struct;

/* structure of global parameters (preferences) */

typedef struct _pref_value_struct {
        char workgroup[MAXGROUPNAMEL+1];            /* samba workgroup of the own machine */
        char master[MAXMACHNAMEL+1];                /* primary master browser */
        char wins_server[MAXMACHNAMEL+1];           /* wins server */
        unsigned char master_browser_scan;          /* do scan with predefined master-browser */
        unsigned char wins_server_scan;             /* do scan with predefined WINS-server */
        unsigned char use_group_browse;             /* use group name on browsing */
        unsigned char use_group_mount;              /* use group name on mounting */
        unsigned char quick_browse;                 /* browse for machines directly after groups browse */
        unsigned char groups_scan_user;             /* groups scan default as user */
        unsigned char machines_scan_user;           /* machines scan default as user */
        unsigned char shares_scan_user;             /* shares scan default as user */
        unsigned char password_once;                /* ask password only once (first time) */
        unsigned char mount_retrigger_enable;       /* do a periodically mountlist update */
        unsigned char run_file_manager;             /* run a file manager after mount action */
        unsigned char startup_browse;               /* do a initial browse immediately after startup */
        unsigned char memorize_mounts;              /* memorize currently mounted shares */
        unsigned char root_mnt_enable;              /* use default root mount directory */
        unsigned char save_default_password;        /* save the browse/mount default password */
        unsigned char delete_mountpoints;           /* delete the mountpoint on unmount */
        unsigned char replace_space_mount;          /* replace spaces in mount path with underscores */
        unsigned char mount_default_no_dialog;      /* don't show the mount dialog, use defaults */
        unsigned char use_smb_port;                 /* use the dedicated smb port */
        int smb_port;                               /* dedicated smb port */
        SMBMOUNT smbmount_version;                  /* smbmount from samba x.x.x */
        char root_mnt_dir[PATH_LEN+1];              /* default root mount directory */
        char filemanager_exe[PATH_LEN+1];           /* file manager command */
        char smbclient_exe[PATH_LEN+1];             /* command path smbclient */
        char nmblookup_exe[PATH_LEN+1];             /* command path nmblookup */
        char smbmount_exe[PATH_LEN+1];              /* command path smbmount */
        char smbumount_exe[PATH_LEN+1];             /* command path smbumount */
        char smbclient_arg[ARG_LEN+1];              /* additional arguments for smbclient */
        char nmblookup_arg[ARG_LEN+1];              /*  - " -  nmblookup */
        char smbmount_arg[ARG_LEN+1];               /*  - " -  smbmount */
        char smbumount_arg[ARG_LEN+1];              /*  - " -  smbumount */
        char default_user[USER_LEN+1];              /* default user for browsing */
} pref_value_struct;

typedef struct _pref_bool_struct {
        unsigned char workgroup;
        unsigned char master;
        unsigned char wins_server;
        unsigned char master_browser_scan;
        unsigned char wins_server_scan;
        unsigned char use_group_browse;
        unsigned char use_group_mount;
        unsigned char quick_browse;
        unsigned char groups_scan_user;
        unsigned char machines_scan_user;
        unsigned char shares_scan_user;
        unsigned char password_once;
        unsigned char mount_retrigger_enable;
        unsigned char run_file_manager;
        unsigned char startup_browse;
        unsigned char memorize_mounts;
        unsigned char root_mnt_enable;
        unsigned char save_default_password;
        unsigned char delete_mountpoints;
        unsigned char replace_space_mount;
        unsigned char mount_default_no_dialog;
        unsigned char use_smb_port;
        unsigned char smb_port;
        unsigned char smbmount_version;
        unsigned char root_mnt_dir;
        unsigned char filemanager_exe;
        unsigned char smbclient_exe;
        unsigned char nmblookup_exe;
        unsigned char smbmount_exe;
        unsigned char smbumount_exe;
        unsigned char smbclient_arg;
        unsigned char nmblookup_arg;
        unsigned char smbmount_arg;
        unsigned char smbumount_arg;
        unsigned char default_user;
} pref_bool_struct;

typedef struct _pref_struct {
        pref_value_struct v;
        pref_bool_struct b;
} pref_struct;

/* structure of the window geometry */

typedef struct _size_struct {
        int main_x;                            /* main window's X-position */
        int main_y;                            /* main window's Y-Position */
        int main_w;                            /* main window's width */
        int main_h;                            /* main window's height */
        int t_comment_pos;                     /* position of the tree comment's column */
        int t_mountpoint_pos;                  /* relative position of the tree mountpoint's column */
        int m_list_h;                          /* height of the mounted share list view */
        int m_mpoint_pos;                      /* position of the mountpoint's column in mounted share list view */
} size_struct;

/* structure of global parameters (runtime) */

typedef struct _global_struct {
        LINUX_VERSION linux_version;                /* version of the linux kernel */
        unsigned short int linux_major;             /* kernel version */
        unsigned short int linux_minor;
        unsigned short int linux_patch;
        uid_t uid;                                  /* user ID */
        gid_t gid;                                  /* group ID */
        char user[USER_LEN+1];                      /* program user name */
        char home_dir[PATH_LEN+1];                  /* user's home directory */
        char domain_name[MAXGROUPNAMEL+1];          /* domain name for browsing */
        char browse_user[USER_LEN+1];               /* username for browsing with user/password */
        char browse_password[PASSWORD_LEN+1];       /* password for browsing with user/password */
        unsigned char is_password;                  /* password already asced ? */
        char default_password[PASSWORD_LEN+1];      /* user's default password */
        unsigned char do_file_manager;              /* flag to run the file manager after mount */
} global_struct;

/* ------------------------------------------------------------------------- */

#endif    /* _define_h_ */
