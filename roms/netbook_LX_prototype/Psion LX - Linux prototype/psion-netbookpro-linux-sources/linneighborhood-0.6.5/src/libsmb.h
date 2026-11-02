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


#ifndef _libsmb_h_
#define _libsmb_h_


#ifdef __cplusplus
extern "C" {
#endif


#include "define.h"


#include <pthread.h>
#include <glib.h>

#ifndef NO_LIBSMB
  #include <libsmbclient.h>
#endif

#include "browsewrap.h"


/* ------------------------------------------------------------------------- */

/* -----------------------------------------------------------------------
 * pointer to the extracted symbols of the shared library
 * 'libsmbclient.so'. This can be function pointers or variable pointers.
 * -----------------------------------------------------------------------
 */

extern int lib_smb_init (const char *path);
extern void lib_smb_close (void);
extern unsigned char lib_smb_is_loaded (void);


#ifndef NO_LIBSMB

extern int lib_smbc_init (smbc_get_auth_data_fn fn, int debug);
extern int lib_smbc_open (const char *furl, int flags, mode_t mode);
extern int lib_smbc_creat (const char *furl, mode_t mode);
extern ssize_t lib_smbc_read (int fd, void *buf, size_t bufsize);
extern ssize_t lib_smbc_write (int fd, void *buf, size_t bufsize);
extern off_t lib_smbc_lseek (int fd, off_t offset, int whence);
extern int lib_smbc_close (int fd);
extern int lib_smbc_unlink (const char *furl);
extern int lib_smbc_rename (const char *ourl, const char *nurl);
extern int lib_smbc_opendir (const char *durl);
extern int lib_smbc_closedir (int dh);
extern int lib_smbc_getdents (unsigned int dh, struct smbc_dirent *dirp, int count);
extern struct smbc_dirent* lib_smbc_readdir (unsigned int dh);
extern off_t lib_smbc_telldir (int dh);
extern int lib_smbc_lseekdir (int fd, off_t offset);
extern int lib_smbc_mkdir (const char *durl, mode_t mode);
extern int lib_smbc_rmdir (const char *durl);
extern int lib_smbc_stat (const char *url, struct stat *st);
extern int lib_smbc_fstat (int fd, struct stat *st);
/*extern int lib_smbc_chown (const char *url, uid_t owner, gid_t group);*/
/*extern int lib_smbc_chmod (const char *url, mode_t mode);*/
extern int lib_smbc_print_file (const char *fname, const char *printq);
extern int lib_smbc_open_print_job (const char *fname);
extern int lib_smbc_list_print_jobs (const char *purl, smbc_get_print_job_info fn);
extern int lib_smbc_unlink_print_job (const char *purl, int id);

#endif

/* ------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------
 * Add information to the authentication database.
 * The authentication callback from the shared library 'libsmclient.so'
 * will use this database to get its authentication.
 * ---------------------------------------------------------------------
 */
extern void lib_smb_auth_database_add ( const char *group,
                                        const char *machine,
                                        const char *user,
                                        const char *password );
                                        
/* -------------------------------------------------------------------------
 * Does make a browse of a share. Returns a GSList containig objects
 * of the struct 'file_struct'. This list is sorted in alphabetical order,
 * directories first.
 * -------------------------------------------------------------------------
 */
extern GSList *lib_smb_browse_share (const char *group, const char *machine, const char *share_path,
                              const char *user, const char *password);

/* frees the SLink List with the browsed files */
extern void lib_smb_file_list_delete (GSList **files);

/* browse share using its own thread/fork */
extern void GetLibSMBPath ( const char *group,
                            const char *machine,
                            const char *share,
                            const char *path,
                            const char *user,
                            const char *password,
                            PATH_BROWSE_INFO *browse_info);

extern void lib_smb_stop_all_browse (void);
extern void lib_smb_stop_all_copy_action (void);

/* file move / copy functions */

#define LIB_SMB_COPY_SIZE                         8192

#define LIB_SMB_ACTION_OK                            0
#define LIB_SMB_FILE_STAT_ERROR                     -1
#define LIB_SMB_SMBFILE_OPEN_ERROR                  -2
#define LIB_SMB_SMBFILE_WRITE_ERROR                 -3
#define LIB_SMB_LOCALFILE_READ_ERROR                -4
#define LIB_SMB_ACTION_ABORT                        -5

#define MAXSMBLIBLINEL                    127


typedef void (*copy_file_callback)(long long current,
                                   long long size,
                                   unsigned char *do_abort);

typedef struct _COPY_SMB_TO_LOCAL_STRUCT {
        int fd[2];
        pthread_t thread;
        gint input_tag;
        char group[MAXGROUPNAMEL+1];
        char machine[MAXMACHNAMEL+1];
        char share[MAXSHRNAMEL+1];
        char path[MAXSHAREPATHL+1];
        char filename[MAXFILEL+1];
        char user[USER_LEN+1];
        char password[PASSWORD_LEN+1];
        unsigned char do_exit;              /* abort file copying */
        copy_file_callback callback;        /* progress callback */
        char linebuf[MAXSMBLIBLINEL+1];
        int linepos;
} COPY_SMB_TO_LOCAL_STRUCT;

typedef struct _file_copy_struct
{
        unsigned char do_exit;              /* abort file copying */
        copy_file_callback callback;        /* progress callback */
}
file_copy_struct;

extern void lib_smb_init_copy_struct (file_copy_struct *cp_struct);

extern unsigned char lib_smb_file_exists (const char *machine, const char *share, const char *path);

extern int lib_smb_copy_local_to_smb_path (char *local_path, char *smb_path, file_copy_struct *c_struct);
extern int lib_smb_copy_local_to_smb (char *local_path, char *machine,
                                                     char *share,
                                                     char *path,
                                                     file_copy_struct *c_struct);

/* ------------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif   /* _libsmb_h_ */
