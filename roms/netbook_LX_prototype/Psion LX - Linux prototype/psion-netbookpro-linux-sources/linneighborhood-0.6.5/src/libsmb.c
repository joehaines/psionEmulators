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


/* must be included first */
#include <pthread.h>

#include <stdio.h>
#include <dlfcn.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#include "libsmb.h"
#include "utility.h"
#include "guiwrap.h"


/* ------------------------------------------------------------------------- */


#ifndef NO_LIBSMB

/*
 *  Pointer to the functions/symbols of the libsmbclient.so library.
 *  This library will be linked dynamically at run time.
 */


int (*l_smbc_init)(smbc_get_auth_data_fn fn, int debug);           /* smbc_init() */
int (*l_smbc_open)(const char *furl, int flags, mode_t mode);      /* smbc_open() */
int (*l_smbc_creat)(const char *furl, mode_t mode);                /* smbc_creat() */
ssize_t (*l_smbc_read)(int fd, void *buf, size_t bufsize);         /* smbc_read() */
ssize_t (*l_smbc_write)(int fd, void *buf, size_t bufsize);        /* smbc_write() */
off_t (*l_smbc_lseek)(int fd, off_t offset, int whence);           /* smbc_lseek() */
int (*l_smbc_close)(int fd);                                       /* smbc_close() */
int (*l_smbc_unlink)(const char *furl);                            /* smbc_unlink() */
int (*l_smbc_rename)(const char *ourl, const char *nurl);          /* smbc_rename() */
int (*l_smbc_opendir)(const char *durl);                           /* smbc_opendir() */
int (*l_smbc_closedir)(int dh);                                    /* smbc_closedir() */
int (*l_smbc_getdents)(unsigned int dh, struct smbc_dirent *dirp, int count); /* smbc_getdents() */
struct smbc_dirent* (*l_smbc_readdir)(unsigned int dh);            /* smb_readdir() */
off_t (*l_smbc_telldir)(int dh);                                   /* smbc_telldir() */
int (*l_smbc_lseekdir)(int fd, off_t offset);                      /* smbc_lseekdir() */
int (*l_smbc_mkdir)(const char *durl, mode_t mode);                /* smbc_mkdir() */
int (*l_smbc_rmdir)(const char *durl);                             /* smbc_rmdir() */
int (*l_smbc_stat)(const char *url, struct stat *st);              /* smbc_stat() */
int (*l_smbc_fstat)(int fd, struct stat *st);                      /* smbc_fstat() */
/*int (*l_smbc_chown)(const char *url, uid_t owner, gid_t group);*/    /* smbc_chown() */
/*int (*l_smbc_chmod)(const char *url, mode_t mode);*/                 /* smbc_chmod() */
int (*l_smbc_print_file)(const char *fname, const char *printq);   /* smbc_print_file() */
int (*l_smbc_open_print_job)(const char *fname);                   /* smbc_open_print_job() */
int (*l_smbc_list_print_jobs)(const char *purl, smbc_get_print_job_info fn);  /* smbc_list_print_jobs() */
int (*l_smbc_unlink_print_job)(const char *purl, int id);          /* smbc_unlink_print_job() */

#endif

/* ------------------------------------------------------------------------- */

#ifndef NO_LIBSMB
/* Handle for dynamically linked library */
static void *handle;
#endif

/* Flag: is library successfully loaded ? */
unsigned char lib_smb_loaded = 0;

/* ------------------------------------------------------------------------- */

#define AUTH_DATABASE_NAMELEN   ( MAXMACHNAMEL + MAXGROUPNAMEL + USER_LEN + 4 )

/* =============================== */
/* +++ authentication database +++ */
/* =============================== */

/*
 *  database_name example : machine = "machine"
 *                          group = "group"
 *                          user = "user"
 *    name -> "group/machine/user"
 */

typedef struct _auth_database_entry {
        char database_name[AUTH_DATABASE_NAMELEN+1];
        char password[PASSWORD_LEN+1];
}
auth_database_entry;


/* single linked list for user/password authentication entries */
static GSList *auth_database = NULL;

/* mutex object for synchronizing authentication database access */
static pthread_mutex_t auth_mutex;
#ifndef NO_LIBSMB
static pthread_mutexattr_t auth_mutex_attr;
#endif


/*
 *  computes the database_name from the group, machine and user names
 */
 
static void create_database_name (char *name, const char *group,
                                              const char *machine,
                                              const char *user )
{
  name[0] = 0;
  
  strncpy(name, group, AUTH_DATABASE_NAMELEN);
  string_ncat(name, "/", AUTH_DATABASE_NAMELEN);
  string_ncat(name, machine, AUTH_DATABASE_NAMELEN);
  string_ncat(name, "/", AUTH_DATABASE_NAMELEN);
  string_ncat(name, user, AUTH_DATABASE_NAMELEN);
  
  name[AUTH_DATABASE_NAMELEN] = 0;
}

/*
 *  gets a certain entry of the authentication list,
 *  create a new one if it doesn't exist
 */

static auth_database_entry *lib_smb_auth_database_get_entry (const char *group,
                                                             const char *machine,
                                                             const char *user,
                                                             unsigned char create)
{
  auth_database_entry *entry, *ret_entry;
  GSList *temp_list;
  char database_name[AUTH_DATABASE_NAMELEN+1];
  
  ret_entry = NULL;

  /* synchronize database access */
  pthread_mutex_lock(&auth_mutex);

  create_database_name(database_name, group, machine, user);
  
  temp_list = auth_database;
  
  /* iterate through the authentication list */
  while ( temp_list != NULL )
  {
    entry = (auth_database_entry*)(temp_list->data);
    
    if ( entry != NULL )
    {
      /* compare database name */
      if ( !strcasecmp(entry->database_name, database_name) )
      {
        /* name identical, entry found */
        ret_entry = entry;
        break;
      }
    }
    
    temp_list = temp_list->next;
  }
  
  /* if entry not found, is create flag set ? */
  if ( (ret_entry == NULL) && (create != 0) )
  {
    /* entry not found, create a new one */
    ret_entry = (auth_database_entry*)g_malloc( sizeof(auth_database_entry) );
    strncpy(ret_entry->database_name, database_name, AUTH_DATABASE_NAMELEN);
    ret_entry->password[0] = 0;

    /* append to rop level list */
    auth_database = g_slist_append(auth_database, ret_entry);
  }

  pthread_mutex_unlock(&auth_mutex);
  
  return ret_entry;
}

/* ------------------------------------------------------------------------- */

/*
 *  adds an authentication entry to the list
 */

void lib_smb_auth_database_add ( const char *group,
                                 const char *machine,
                                 const char *user,
                                 const char *password )
{
  auth_database_entry *entry;

  if ( !lib_smb_is_loaded() )
    return;
  
  entry = lib_smb_auth_database_get_entry(group, machine, user, 1);

  if ( entry != NULL )
  {
    /* entry already in database or new created */
    strncpy(entry->password, password, PASSWORD_LEN);
  }
}

/*
 *  deletes the whole authentication entry list
 */

#ifndef NO_LIBSMB
 
static void lib_smb_auth_database_delete (void)
{
  /* synchronize database access */
  pthread_mutex_lock(&auth_mutex);

  if ( auth_database != NULL )
  {
    slist_free_with_data( &auth_database );
    auth_database = NULL;
  }

  pthread_mutex_unlock(&auth_mutex);
}

#endif

/*
 *  gets an authentication entry of the list, fills up the password
 *
 *  return value :  0 ->  entry not found
 *                  1 ->  entry found, group and password filled
 */

#ifndef NO_LIBSMB
  
static unsigned char lib_smb_auth_database_get_auth ( const char *machine,
                                                      const char *user,
                                                      char *group, int g_len,
                                                      char *password, int p_len)
{
  char t_machine[MAXMACHNAMEL+1];
  char t_user[USER_LEN+1];
  auth_database_entry *entry;
  
  strncpy(t_machine, machine, MAXMACHNAMEL);
  strncpy(t_user, user, USER_LEN);

  entry = lib_smb_auth_database_get_entry(group, t_machine, t_user, 0);
  
  if ( entry != NULL )
  {
    /* if entry found, set password */
    strncpy(password, entry->password, p_len - 1);
    return 1;
  }
  else
  {
    return 0;
  }
}

#endif

/* ============================================================ */
/* +++ authentication function for libsmbclient invocations +++ */
/* ============================================================ */

#ifndef NO_LIBSMB

static void lib_smb_auth_fn ( const char *srv,        /* server */
                              const char *shr,        /* share */
                              char *wg, int wglen,    /* workgroup + length */
                              char *un, int unlen,    /* user + length */
                              char *pw, int pwlen)    /* password + length */
{
  /* get the password from the database */
  lib_smb_auth_database_get_auth(srv, un, wg, wglen, pw, pwlen);
}

#endif

/* ------------------------------------------------------------------------- */

/* error check routine, 0 -> Ok, 1 -> Error */

#ifndef NO_LIBSMB

static unsigned int lib_smb_check_error (void)
{
  const char *error;
  
  error = dlerror();
  
  if ( error != NULL )
  {
    lib_smb_close();
    
    fprintf(stdout, "lib_smb error : %s\n", error);
    return 1;
  }
  
  return 0;
}

#endif

/*
 *  loads dynamically the library 'libsmbclient.so', which is provided
 *  by the Samba package since version 2.2.1
 *
 *  parameter : path -> name (and path) of the shared library
 *
 *  return value :  0 -> loading Ok
 *                 -1 -> load error, cannot open library
 *                 -2 -> error on resolving symbol
 *                 -3 -> error initializing libsmbclient
 */

int lib_smb_init (const char *path)
{

#ifndef NO_LIBSMB
  
  handle = dlopen(path, RTLD_LAZY);
  
  if ( handle == NULL )
  {
    /* could not find library */
    return -1;
  }
  
  /* link the symbols */
  
  l_smbc_init = dlsym(handle, "smbc_init");
  if ( lib_smb_check_error() )
  {
    return -2;
  }
  
  l_smbc_open = dlsym(handle, "smbc_open");
  if ( lib_smb_check_error() )
  {
    return -2;
  }
  
  l_smbc_creat = dlsym(handle, "smbc_creat");
  if ( lib_smb_check_error() )
  {
    return -2;
  }
  
  l_smbc_read = dlsym(handle, "smbc_read");
  if ( lib_smb_check_error() )
  {
    return -2;
  }
  
  l_smbc_write = dlsym(handle, "smbc_write");
  if ( lib_smb_check_error() )
  {
    return -2;
  }
  
  l_smbc_lseek = dlsym(handle, "smbc_lseek");
  if ( lib_smb_check_error() )
  {
    return -2;
  }
  
  l_smbc_close = dlsym(handle, "smbc_close");
  if ( lib_smb_check_error() )
  {
    return -2;
  }
  
  l_smbc_unlink = dlsym(handle, "smbc_unlink");
  if ( lib_smb_check_error() )
  {
    return -2;
  }
  
  l_smbc_rename = dlsym(handle, "smbc_rename");
  if ( lib_smb_check_error() )
  {
    return -2;
  }
  
  l_smbc_opendir = dlsym(handle, "smbc_opendir");
  if ( lib_smb_check_error() )
  {
    return -2;
  }

  l_smbc_closedir = dlsym(handle, "smbc_closedir");
  if ( lib_smb_check_error() )
  {
    return -2;
  }

  l_smbc_getdents = dlsym(handle, "smbc_getdents");
  if ( lib_smb_check_error() )
  {
    return -2;
  }

  l_smbc_readdir = dlsym(handle, "smbc_readdir");
  if ( lib_smb_check_error() )
  {
    return -2;
  }

  l_smbc_telldir = dlsym(handle, "smbc_telldir");
  if ( lib_smb_check_error() )
  {
    return -2;
  }

  l_smbc_lseekdir = dlsym(handle, "smbc_lseekdir");
  if ( lib_smb_check_error() )
  {
    return -2;
  }

  l_smbc_mkdir = dlsym(handle, "smbc_mkdir");
  if ( lib_smb_check_error() )
  {
    return -2;
  }

  l_smbc_rmdir = dlsym(handle, "smbc_rmdir");
  if ( lib_smb_check_error() )
  {
    return -2;
  }

  l_smbc_stat = dlsym(handle, "smbc_stat");
  if ( lib_smb_check_error() )
  {
    return -2;
  }

  l_smbc_fstat = dlsym(handle, "smbc_fstat");
  if ( lib_smb_check_error() )
  {
    return -2;
  }

/*  l_smbc_chown = dlsym(handle, "smbc_chown");
  if ( lib_smb_check_error() )
  {
    return -2;
  }*/

/*  l_smbc_chmod = dlsym(handle, "smbc_chmod");
  if ( lib_smb_check_error() )
  {
    return -2;
  }*/

  l_smbc_print_file = dlsym(handle, "smbc_print_file");
  if ( lib_smb_check_error() )
  {
    return -2;
  }

  l_smbc_open_print_job = dlsym(handle, "smbc_open_print_job");
  if ( lib_smb_check_error() )
  {
    return -2;
  }

  l_smbc_list_print_jobs = dlsym(handle, "smbc_list_print_jobs");
  if ( lib_smb_check_error() )
  {
    return -2;
  }

  l_smbc_unlink_print_job = dlsym(handle, "smbc_unlink_print_job");
  if ( lib_smb_check_error() )
  {
    return -2;
  }
  
  lib_smb_loaded = 1;
  
  /* initialize the authentication mutex */
  pthread_mutex_init(&auth_mutex, &auth_mutex_attr);
  
  /* =============================== */
  /* +++ initialize libsmbclient +++ */
  /* =============================== */
  
  if ( lib_smbc_init(lib_smb_auth_fn, 0) < 0 )
  {
    lib_smb_close();
    return -3;
  }
  
  return 0;

#else

  return -1;

#endif

}


/* Unlink the library */

void lib_smb_close (void)
{

#ifndef NO_LIBSMB

  /* destroy authentication mutex */
  pthread_mutex_destroy(&auth_mutex);
  
  if ( handle != NULL )
  {
    dlclose(handle);
  }
  
  lib_smb_auth_database_delete();
  
  lib_smb_loaded = 0;
  
  handle = NULL;
 
#endif

}

unsigned char lib_smb_is_loaded (void)
{
  return lib_smb_loaded;
}

/* ------------------------------------------------------------------------- */

/*
 ************************************************
 *  wrapper function about the library functions
 ************************************************
 */

#ifndef NO_LIBSMB
 
int lib_smbc_init (smbc_get_auth_data_fn fn, int debug)
{
  if ( lib_smb_is_loaded() )
    return (*l_smbc_init)(fn, debug);
  else
    return -1;
}

int lib_smbc_open (const char *furl, int flags, mode_t mode)
{
  if ( lib_smb_is_loaded() )
    return (*l_smbc_open)(furl, flags, mode);
  else
    return -1;
}

int lib_smbc_creat (const char *furl, mode_t mode)
{
  if ( lib_smb_is_loaded() )
    return (*l_smbc_creat)(furl, mode);
  else
    return -1;
}

ssize_t lib_smbc_read (int fd, void *buf, size_t bufsize)
{
  if ( lib_smb_is_loaded() )
    return (*l_smbc_read)(fd, buf, bufsize);
  else
    return -1;
}

ssize_t lib_smbc_write (int fd, void *buf, size_t bufsize)
{
  if ( lib_smb_is_loaded() )
    return (*l_smbc_write)(fd, buf, bufsize);
  else
    return -1;
}

off_t lib_smbc_lseek (int fd, off_t offset, int whence)
{
  if ( lib_smb_is_loaded() )
    return (*l_smbc_lseek)(fd, offset, whence);
  else
    return -1;
}

int lib_smbc_close (int fd)
{
  if ( lib_smb_is_loaded() )
    return (*l_smbc_close)(fd);
  else
    return -1;
}

int lib_smbc_unlink (const char *furl)
{
  if ( lib_smb_is_loaded() )
    return (*l_smbc_unlink)(furl);
  else
    return -1;
}

int lib_smbc_rename (const char *ourl, const char *nurl)
{
  if ( lib_smb_is_loaded() )
    return (*l_smbc_rename)(ourl, nurl);
  else
    return -1;
}

int lib_smbc_opendir (const char *durl)
{
  if ( lib_smb_is_loaded() )
    return (*l_smbc_opendir)(durl);
  else
    return -1;
}

int lib_smbc_closedir (int dh)
{
  if ( lib_smb_is_loaded() )
    return (*l_smbc_closedir)(dh);
  else
    return -1;
}

int lib_smbc_getdents (unsigned int dh, struct smbc_dirent *dirp, int count)
{
  if ( lib_smb_is_loaded() )
    return (*l_smbc_getdents)(dh, dirp, count);
  else
    return -1;
}

struct smbc_dirent* lib_smbc_readdir (unsigned int dh)
{
  if ( lib_smb_is_loaded() )
    return (*l_smbc_readdir)(dh);
  else
    return NULL;
}

off_t lib_smbc_telldir (int dh)
{
  if ( lib_smb_is_loaded() )
    return (*l_smbc_telldir)(dh);
  else
    return -1;
}

int lib_smbc_lseekdir (int fd, off_t offset)
{
  if ( lib_smb_is_loaded() )
    return (*l_smbc_lseekdir)(fd, offset);
  else
    return -1;
}

int lib_smbc_mkdir (const char *durl, mode_t mode)
{
  if ( lib_smb_is_loaded() )
    return (*l_smbc_mkdir)(durl, mode);
  else
    return -1;
}

int lib_smbc_rmdir (const char *durl)
{
  if ( lib_smb_is_loaded() )
    return (*l_smbc_rmdir)(durl);
  else
    return -1;
}

int lib_smbc_stat (const char *url, struct stat *st)
{
  if ( lib_smb_is_loaded() )
    return (*l_smbc_stat)(url, st);
  else
    return -1;
}

int lib_smbc_fstat (int fd, struct stat *st)
{
  if ( lib_smb_is_loaded() )
    return (*l_smbc_fstat)(fd, st);
  else
    return -1;
}

/*int lib_smbc_chown (const char *url, uid_t owner, gid_t group)
{
  if ( lib_smb_is_loaded() )
    return (*l_smbc_chown)(url, owner, group);
  else
    return -1;
}*/

/*int lib_smbc_chmod (const char *url, mode_t mode)
{
  if ( lib_smb_is_loaded() )
    return (*l_smbc_chmod)(url, mode);
  else
    return -1;
}*/

int lib_smbc_print_file (const char *fname, const char *printq)
{
  if ( lib_smb_is_loaded() )
    return (*l_smbc_print_file)(fname, printq);
  else
    return -1;
}

int lib_smbc_open_print_job (const char *fname)
{
  if ( lib_smb_is_loaded() )
    return (*l_smbc_open_print_job)(fname);
  else
    return -1;
}

int lib_smbc_list_print_jobs (const char *purl, smbc_get_print_job_info fn)
{
  if ( lib_smb_is_loaded() )
    return (*l_smbc_list_print_jobs)(purl, fn);
  else
    return -1;
}

int lib_smbc_unlink_print_job (const char *purl, int id)
{
  if ( lib_smb_is_loaded() )
    return (*l_smbc_unlink_print_job)(purl, id);
  else
    return -1;
}

#endif

/* ------------------------------------------------------------------------- */

/* +++ create share path +++ */

static void create_share_path (char *share_path, char *share, char *path, int max_length)
{
  strncpy(share_path, share, max_length);
  string_ncat(share_path, "/", max_length);
  string_ncat(share_path, path, max_length);
  share_path[max_length] = 0;
}

/*
 *    there are several syntaxes possible:
 *
 *        "smb://machine/path"
 *        "smb://user:password@machine/path"    -> take a look to libsmbclient source
 */

#ifndef NO_LIBSMB
 
static void create_smb_url (char *url, char *machine, char *share_path,
                char *user, char *password, char *domain, int max_length)
{
  strncpy(url, "smb://", max_length);
  
  /* add domain/user/password to the url ? */
  if ( user[0] != 0 )
  {
    if ( domain[0] != 0 )
    {
      string_ncat(url, domain, max_length);
      string_ncat(url, ";",  max_length);
    }
    
    string_ncat(url, user, max_length);
    
    if ( password[0] != 0 )
    {
      string_ncat(url, ":", max_length);
      string_ncat(url, password, max_length);
    }
    string_ncat(url, "@", max_length);
  }
  
  string_ncat(url, machine, max_length);
  string_ncat(url, "/", max_length);
  string_ncat(url, share_path, max_length);
  url[max_length] = 0;
}

#endif

static void create_smb_url_complete (char *url, char *machine, char *share, char *path,
                char *user, char *password, char *domain, int max_length)
{
  strncpy(url, "smb://", max_length);
  
  /* add domain/user/password to the url ? */
  if ( user[0] != 0 )
  {
    if ( domain[0] != 0 )
    {
      string_ncat(url, domain, max_length);
      string_ncat(url, ";",  max_length);
    }
    
    string_ncat(url, user, max_length);
    
    if ( password[0] != 0 )
    {
      string_ncat(url, ":", max_length);
      string_ncat(url, password, max_length);
    }
    string_ncat(url, "@", max_length);
  }
  
  string_ncat(url, machine, max_length);
  string_ncat(url, "/", max_length);
  string_ncat(url, share, max_length);
  string_ncat(url, "/", max_length);
  string_ncat(url, path, max_length);
  url[max_length] = 0;
}

/* ------------------------------------------------------------------------- */

/*
 *  sort the list in alphabetical order, directories first
 *
 *  return value :  0 -> insert value at this position
 *                  1 -> don't insert, do further iteration
 *
 */

#ifndef NO_LIBSMB
 
static gint lib_smb_share_compare_func (gconstpointer a, gconstpointer b)
{
  file_struct *file1, *file2;
  unsigned char isdir1, isdir2;
  
  file1 = (file_struct*)a;
  file2 = (file_struct*)b;
  
  if ( (file1 != NULL) && (file2 != NULL) )
  {
    isdir1 = file1->attr & FILE_ATTR_DIRECTORY;
    isdir2 = file2->attr & FILE_ATTR_DIRECTORY;
    
    /* first check if the files a directories */
    if ( isdir1 && isdir2 )
    {
      if ( strcmp(file1->name, file2->name) >= 0 )
      {
        return 1;
      }
      else
      {
        return 0;
      }
    }
    
    if ( isdir1 && !isdir2 )
    {
      return 0;
    }
    
    if ( !isdir1 && isdir2 )
    {
      return 1;
    }
    
    /* both files are normal files */
    if ( strcmp(file1->name, file2->name) >= 0 )
    {
      return 1;
    }
    else
    {
      return 0;
    }
  }
  else
  {
    return 0;
  }
}

#endif

/*
 *  function for browsing a smb share
 *
 *  return value :  list of entries of the share
 *
 */

GSList *lib_smb_browse_share (const char *group, const char *machine, const char *share_path,
                              const char *user, const char *password)
{
#ifndef NO_LIBSMB
  char *share_url;
  int url_length;
  int dh, err;
  char dirbuf[1024];
  struct smbc_dirent *dirp;
  int dirlen;
  char stat_url[1024];
  struct stat st1;
  file_struct *file;
  struct tm *time;
#endif
  GSList *entry_list = NULL;
    
#ifndef NO_LIBSMB

  if ( machine == NULL )
    goto browse_share_exit;
  if ( share_path == NULL )
    goto browse_share_exit;
  if ( user == NULL )
    goto browse_share_exit;
  if ( password == NULL )
    goto browse_share_exit;
  
  /* ========================= */
  /* +++ create browse url +++ */
  /* ========================= */
  
  url_length = 512;
  url_length += 10;
  
  share_url = (char*)g_malloc( url_length + 1);
  
  create_smb_url(share_url, machine, share_path, user, password, cempty, url_length);
  
  /* ========================== */
  /* +++ browse the smb url +++ */
  /* ========================== */
  
  dh = lib_smbc_opendir(share_url);
  
  if ( dh < 0 )
  {
    /* error occurred */
    goto browse_share_exit_free;
  }
  
  /* ==================================== */
  /* +++ get the result of the browse +++ */
  /* ==================================== */
  
  while ( (err = lib_smbc_getdents(dh, (struct smbc_dirent*) dirbuf,
                                sizeof(dirbuf))) != 0 )
  {
    if ( err < 0 )
    {
      /* error occurred */
      break;
    }
  
    dirp = (struct smbc_dirent*) dirbuf;
    
    /* any more entries left in the browse */
    
    while ( err > 0 )
    {
      dirlen = dirp->dirlen;  // sigsegfault
      
      switch ( dirp->smbc_type )
      {
        case SMBC_DIR:
        case SMBC_FILE:
        
            /* drop the directory aliases */
            if ( strcmp(dirp->name, ".") == 0 )
              break;
            if ( strcmp(dirp->name, "..") == 0 )
              break;
            
            /* create file structure */
            
            file = (file_struct*)g_malloc( sizeof(file_struct) );
            
            /* copy name of file/directory */
            
            file->name_length = strlen(dirp->name);
            file->name = (char*)g_malloc( file->name_length + 1 );
            
            strncpy(file->name, dirp->name, file->name_length);
            file->name[file->name_length] = 0;
            
            /* ======================================== */
            /* +++ create url of the file/directory +++ */
            /* ======================================== */
            
            strncpy(stat_url, share_url, sizeof(stat_url));
            strncat(stat_url, "/", sizeof(stat_url) - strlen(share_url));
            strncat(stat_url, dirp->name, sizeof(stat_url) - strlen(share_url));
            
            /* preset attributes */
            
            file->attr = 0;

            if ( lib_smbc_stat(stat_url, &st1) < 0 )
            {
              /* error occurred */
              file->size = 0;
              file->hour = 0;
              file->minute = 0;
              file->day = 0;
              file->month = 0;
              file->year = 0;
            }
            else
            {
              /* length of file */
              file->size = st1.st_size;
              
              /* date of file */
              time = localtime( &(st1.st_mtime) );
              
              file->hour = time->tm_hour;
              file->minute = time->tm_min;
              file->day = time->tm_mday;
              file->month = time->tm_mon + 1;
              file->year = time->tm_year + 1900;
              
              /* ================== */
              /* +++ attributes +++ */
              /* ================== */
              
              /* directory attribute */
              
              if ( dirp->smbc_type == SMBC_DIR )
                file->attr |= FILE_ATTR_DIRECTORY;
              
              /* read only attribute */
              
              file->attr |= FILE_ATTR_READONLY;
              if ( st1.st_mode | (S_IWUSR & S_IWGRP & S_IWOTH) )
              {
                file->attr &= ~FILE_ATTR_READONLY;
              }
              
              /* hidden attribute */
              
              if ( dirp->name[0] == '.' )
                file->attr |= FILE_ATTR_HIDDEN;
            }
            
            /* ================================= */
            /* +++ add the entry to the list +++ */
            /* ================================= */
            
            entry_list = g_slist_insert_sorted(entry_list, file, lib_smb_share_compare_func);
            
            break;
        
        default:
            break;
      }
      
      (char*)dirp += dirlen;
      
      err -= dirlen;
    }
  }
  
  lib_smbc_closedir(dh);
  
  
  /* =================== */
  /* +++ free memory +++ */
  /* =================== */
  
browse_share_exit_free:

  g_free(share_url);
  
browse_share_exit:

#endif

  return entry_list;
}

/* --------------------------------------------------------------------------
 * frees the former browsed file list
 * --------------------------------------------------------------------------
 */

void lib_smb_file_list_delete (GSList **files)
{
  GSList *templist;
  file_struct *file;
  
  templist = *files;
  
  while ( templist != NULL )
  {
    file = (file_struct*)(templist->data);
    
    if ( file != NULL )
    {
      /* delete the memory area for the file name */
      if ( file->name != NULL )
        g_free(file->name);
      
      /* delete the file structure */
      g_free(file);
      templist->data = NULL;
    }
    
    templist = templist->next;
  }
  
  /* delete the linked list */
  if ( *files != NULL )
  {
    g_slist_free(*files);
    *files = NULL;
  }
}

/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */

#define GDK_INPUT_READ__      ( GDK_INPUT_READ | GDK_INPUT_EXCEPTION ) 

#define CALLBACK_MESSAGE_LEN              127

#define PATH_MESSAGE_BROWSE_READY        "PATH_BROWSE_READY"
#define COPY_MESSAGE_READY               "COPY_MESSAGE_READY"


typedef struct _LIBSMB_PBROWSE_STRUCT {
        int fd[2];
        pthread_t thread;
        GSList *files;
        gint input_tag;
        char group[MAXGROUPNAMEL+1];
        char machine[MAXMACHNAMEL+1];
        char share[MAXSHRNAMEL+1];
        char path[MAXSHAREPATHL+1];
        char user[USER_LEN+1];
        char password[PASSWORD_LEN+1];
        PATH_BROWSE_INFO *browse_info;
        char linebuf[MAXSMBLIBLINEL+1];
        int linepos;
} LIBSMB_PBROWSE_STRUCT;

/* ------------------------------------------------------------------------- */

static GSList *pbrowsedatlist = NULL;
static GSList *pcopysmbtolocallist = NULL;


void lib_smb_stop_all_browse (void) /*Stop all unfinished browsing actions*/
{
  GSList *templist;
  LIBSMB_PBROWSE_STRUCT *pbrowsedat;
  
  if ( !lib_smb_is_loaded() )
    return;
  
  usleep(50000);
  
  //pthread_kill_other_threads_np();
  
  templist = pbrowsedatlist;
  while ( templist != NULL )
  {
    pbrowsedat = (LIBSMB_PBROWSE_STRUCT*)(templist->data);
    pthread_cancel(pbrowsedat->thread);
    close(pbrowsedat->fd[0]);
    close(pbrowsedat->fd[1]);
    pthread_join(pbrowsedat->thread, NULL);
    gdk_input_remove(pbrowsedat->input_tag); 
    templist = templist->next;
    g_print("stop-thread-cancel\n");
  }
  slist_free_with_data(&pbrowsedatlist);
}

void lib_smb_stop_all_copy_action (void) /*Stop all unfinished browsing actions*/
{
  GSList *templist;
  COPY_SMB_TO_LOCAL_STRUCT *pcopysmbtolocaldat;
  
  if ( !lib_smb_is_loaded() )
    return;
  
  usleep(50000);
  
  //pthread_kill_other_threads_np();
  
  templist = pcopysmbtolocallist;
  while ( templist != NULL )
  {
    pcopysmbtolocaldat = (COPY_SMB_TO_LOCAL_STRUCT*)(templist->data);
    pthread_cancel(pcopysmbtolocaldat->thread);
    close(pcopysmbtolocaldat->fd[0]);
    close(pcopysmbtolocaldat->fd[1]);
    pthread_join(pcopysmbtolocaldat->thread, NULL);
    gdk_input_remove(pcopysmbtolocaldat->input_tag); 
    templist = templist->next;
    g_print("stop-thread-cancel\n");
  }
  slist_free_with_data(&pcopysmbtolocallist);
}

/* ------------------------------------------------------------------------- */

static int lib_smb_read_line (int fd, int *linec, int *linepos, char *linebuf)
{
  int numread;
  int ok = 0;
  char c;
  
  do
  {
    numread = read(fd, &c, 1);
    
    if ( numread == 0 )
    {
      *linec = EOF;
    }
    else
    {
      *linec = c;
    }
    
    if ( numread && (c != '\n') )
    {
      if ( *linepos < MAXSMBLIBLINEL )
        *(linebuf+(*linepos)++) = c;
      
      linebuf[*linepos] = 0;
      
      ok = 0;  
    }
    else
    {
      *(linebuf+(*linepos)) = 0;
      ok = (*linepos > 0) || (*linec == '\n');
    }
    
  } while ( !((*linec == EOF) || ok) );
  
  return (ok);
}

static void GetLibSMBPath_inputcallback ( LIBSMB_PBROWSE_STRUCT *browsedat,
                                          gint source,
                                          GdkInputCondition condition)
{
  int linec;
  void *ret_value;

  if ( condition & GDK_INPUT_READ__ )
  {
    /* read a complete line from the pipe input */
    
    if ( lib_smb_read_line(source, &linec, &(browsedat->linepos), browsedat->linebuf) )
    {
      /* scan the input for some special messages sent by the child process */
      
      if ( strstr(browsedat->linebuf, PATH_MESSAGE_BROWSE_READY) )
      {
        /* message : browse is done */
        linec = EOF;
      }
    }
    
    if ( linec == EOF )
    {
      /* close pipe */
      close(browsedat->fd[0]);
      close(browsedat->fd[1]);
      
      pthread_join(browsedat->thread, &ret_value);
      gdk_input_remove(browsedat->input_tag);
    
      GetSMBPath_done( browsedat->group,
                       browsedat->machine,
                       browsedat->share,
                       browsedat->path,
                       browsedat->files,
                       browsedat->browse_info,
                       SMB_STATE_OK);

     pbrowsedatlist = g_slist_remove(pbrowsedatlist, browsedat);
     g_free(browsedat);
    }
  }
}

/* ========================== */
/* path browse thread routine */
/* ========================== */

static void GetLibSMBPath_thread (void *arg)
{
  LIBSMB_PBROWSE_STRUCT *browsedat;
  char share_path[MAXSHAREPATHL+1];
  char callback_message[CALLBACK_MESSAGE_LEN+1];
  
  browsedat = (LIBSMB_PBROWSE_STRUCT*)arg;
  
  if ( arg != NULL )
  {
    /* create the share path */
    create_share_path(share_path, browsedat->share, browsedat->path, MAXSHAREPATHL);

    /* browse the share */
    browsedat->files = lib_smb_browse_share( browsedat->group,
                                             browsedat->machine,
                                             share_path,
                                             browsedat->user,
                                             browsedat->password);
    
    /* inform input callback that browsing is ready */
    strncpy(callback_message, PATH_MESSAGE_BROWSE_READY, CALLBACK_MESSAGE_LEN);
    strncat(callback_message, "\n", CALLBACK_MESSAGE_LEN);
    callback_message[CALLBACK_MESSAGE_LEN] = 0;
    write(browsedat->fd[1], callback_message, strlen(callback_message) );
  }
  
  pthread_exit( 0 );
}

void GetLibSMBPath (const char *group, const char *machine, const char *share, const char *path,
                      const char *user, const char *password, PATH_BROWSE_INFO *browse_info)
{
  int pipe_nok;
  LIBSMB_PBROWSE_STRUCT *browsedat;
  
  if ( !lib_smb_is_loaded() )
    return;
  
  browsedat = (LIBSMB_PBROWSE_STRUCT*)g_malloc( sizeof(LIBSMB_PBROWSE_STRUCT) );
  
  pipe_nok = pipe(browsedat->fd);
  
  if ( pipe_nok != 0 )
  {
    g_free(browsedat);
    g_print("GetLibSMBPath() pipe error\n");
    
    return;
  }
  
  strncpy(browsedat->group, group, MAXGROUPNAMEL);
  strncpy(browsedat->machine, machine, MAXMACHNAMEL);
  strncpy(browsedat->share, share, MAXSHRNAMEL);
  strncpy(browsedat->path, path, MAXSHAREPATHL);
  strncpy(browsedat->user, user, USER_LEN);
  strncpy(browsedat->password, password, PASSWORD_LEN);
  browsedat->browse_info = browse_info;
  browsedat->linepos = 0;
  browsedat->linebuf[0] = 0;
  browsedat->files = 0;
  
  /* create a posix thread for browsing */
  if ( pthread_create(&(browsedat->thread), NULL, (void*)GetLibSMBPath_thread, (void*)browsedat) != 0 )
  {
    close(browsedat->fd[0]);
    close(browsedat->fd[1]);
    
    g_free(browsedat);
    g_print("GetLibSMBPath() thread error\n");
    
    return;
  }

  gui_log_window_insert_string("", 1);

  pbrowsedatlist = g_slist_append(pbrowsedatlist, browsedat);
  
  browsedat->input_tag = gdk_input_add ( browsedat->fd[0],
                                         GDK_INPUT_READ__,
                                         (GdkInputFunction)GetLibSMBPath_inputcallback,
                                         browsedat);
}

/* ------------------------------------------------------------------------- */

static void CopySMBToLocal_inputcallback ( COPY_SMB_TO_LOCAL_STRUCT *copydat,
                                           gint source,
                                           GdkInputCondition condition)
{
  int linec;
  void *ret_value;

  if ( condition & GDK_INPUT_READ__ )
  {
    /* read a complete line from the pipe input */
    
    if ( lib_smb_read_line(source, &linec, &(copydat->linepos), copydat->linebuf) )
    {
      /* scan the input for some special messages sent by the child process */
      
      if ( strstr(copydat->linebuf, COPY_MESSAGE_READY) )
      {
        /* message : browse is done */
        linec = EOF;
      }
    }
    
    if ( linec == EOF )
    {
      /* close pipe */
      close(copydat->fd[0]);
      close(copydat->fd[1]);
      
      pthread_join(copydat->thread, &ret_value);
      gdk_input_remove(copydat->input_tag);
    
      /*GetSMBPath_done( browsedat->group,
                       browsedat->machine,
                       browsedat->share,
                       browsedat->path,
                       browsedat->files,
                       browsedat->browse_info,
                       SMB_STATE_OK);*/

     pcopysmbtolocallist = g_slist_remove(pcopysmbtolocallist, copydat);
     g_free(copydat);
    }
  }
}

/* ================================ */
/* copy SMB to Local thread routine */
/* ================================ */

static void CopySMBToLocal_thread (void *arg)
{
  COPY_SMB_TO_LOCAL_STRUCT *copydat;
  char share_path[MAXSHAREPATHL+1];
  char callback_message[CALLBACK_MESSAGE_LEN+1];
  file_copy_struct copy_struct;
  
  copydat = (COPY_SMB_TO_LOCAL_STRUCT*)arg;
  
  if ( arg != NULL )
  {
    /* create the share path */
    create_share_path(share_path, copydat->share, copydat->path, MAXSHAREPATHL);
    
    copy_struct.do_exit = copydat->do_exit;
    copy_struct.callback = copydat->callback;
    
    /* copy the file */
    lib_smb_copy_local_to_smb( copydat->filename,
                               copydat->machine, 
                               copydat->share,
                               copydat->path,
                               &copy_struct);
    
    copydat->do_exit = copy_struct.do_exit;
    
    /* inform input callback that copying is finished */
    strncpy(callback_message, COPY_MESSAGE_READY, CALLBACK_MESSAGE_LEN);
    strncat(callback_message, "\n", CALLBACK_MESSAGE_LEN);
    callback_message[CALLBACK_MESSAGE_LEN] = 0;
    write(copydat->fd[1], callback_message, strlen(callback_message) );
  }
  
  pthread_exit( 0 );
}

void CopySMBToLocal (const char *group, const char *machine, const char *share, const char *path,
                     const char *filename, const char *user, const char *password, copy_file_callback  callback)
{
  int pipe_nok;
  COPY_SMB_TO_LOCAL_STRUCT *copydat;
  
  if ( !lib_smb_is_loaded() )
    return;
  
  copydat = (COPY_SMB_TO_LOCAL_STRUCT*)g_malloc( sizeof(COPY_SMB_TO_LOCAL_STRUCT) );
  
  pipe_nok = pipe(copydat->fd);
  
  if ( pipe_nok != 0 )
  {
    g_free(copydat);
    g_print("CopySMBToLocal() pipe error\n");
    
    return;
  }
  
  strncpy(copydat->group, group, MAXGROUPNAMEL);
  strncpy(copydat->machine, machine, MAXMACHNAMEL);
  strncpy(copydat->share, share, MAXSHRNAMEL);
  strncpy(copydat->path, path, MAXSHAREPATHL);
  strncpy(copydat->filename, filename, MAXFILEL);
  strncpy(copydat->user, user, USER_LEN);
  strncpy(copydat->password, password, PASSWORD_LEN);
  copydat->callback = callback;
  copydat->do_exit = 0;
  copydat->linepos = 0;
  copydat->linebuf[0] = 0;
  
  /* create a posix thread for browsing */
  if ( pthread_create(&(copydat->thread), NULL, (void*)CopySMBToLocal_thread, (void*)copydat) != 0 )
  {
    close(copydat->fd[0]);
    close(copydat->fd[1]);
    
    g_free(copydat);
    g_print("CopySMBToLocal() thread error\n");
    
    return;
  }

  gui_log_window_insert_string("", 1);

  pcopysmbtolocallist = g_slist_append(pcopysmbtolocallist, copydat);
  
  copydat->input_tag = gdk_input_add ( copydat->fd[0],
                                       GDK_INPUT_READ__,
                                       (GdkInputFunction)CopySMBToLocal_inputcallback,
                                       copydat);
}

/* ------------------------------------------------------------------------- */

/* ======================================================================== */
/* +++ move / copy helper functions +++                                     */
/* ======================================================================== */

/*
 *  initialize copy structure
 *
 */

void lib_smb_init_copy_struct (file_copy_struct *cp_struct)
{
  cp_struct->do_exit = 0;
  cp_struct->callback = NULL;
}

/*
 *  get the length of a file on the local file system in bytes
 *
 */

#ifndef NO_LIBSMB
 
static off_t lib_smb_get_local_file_length (int fd)
{
  off_t length;
  
  length = lseek(fd, 0, SEEK_END);
  
  /* reposition of descriptor */
  lseek(fd, 0, SEEK_SET);
  
  return length;
}

#endif

/* ======================================================================== */
/* +++ file move / copy functions +++                                       */
/* ======================================================================== */

/*
 *  checks whether a smb file does exist on the server
 *
 *    return value :  = 0  -> file doesn't exist
 *                   != 0  -> file exists
 */

unsigned char lib_smb_file_exists (const char *machine, const char *share, const char *path)
{
  unsigned char exist = 0;
#ifndef NO_LIBSMB
  char url[MAXSMBURLL+1];
  mode_t mode = 0;
  int fd;
  
  create_smb_url_complete(url, machine, share, path, cempty, cempty, cempty, MAXSMBURLL);
  
  fd = lib_smbc_open(url, O_RDONLY, mode);
  if ( fd < 0 )
  {
    exist = 0;
  }
  else
  {
    /* close the url */
    lib_smbc_close(fd);
    
    exist = 1;
  }
  
#endif
  
  return exist;
}

/* ------------------------------------------------------------------------- */

/*
 *  Copy a local file to a remote host. The functions doesn't check whether
 *  the remote file does already exist. If this is the case the remote
 *  file will be truncated to zero and then be overwritten without any question.
 *
 *    version 1 : target url in smb notation
 *    version 2 . target url separated by machine, share, path
 *
 *    return value :  = 0  -> copying successful
 *                   != 0  -> file exists
 */
 
int lib_smb_copy_local_to_smb_path (char *local_path, char *smb_path, file_copy_struct *c_struct)
{
#ifndef NO_LIBSMB
  int local_fd, remote_fd;
  struct stat state;
  off_t file_len;
  off_t current_len;
  size_t size;
  ssize_t read_len;
  unsigned char puffer[LIB_SMB_COPY_SIZE];
#endif
  int result;
  
  result = LIB_SMB_ACTION_OK;
  
#ifndef NO_LIBSMB
  
  /* fetch the mode of the source file */
  if ( lstat(local_path, &state) < 0 )
  {
    return LIB_SMB_FILE_STAT_ERROR;
  }
  
  /* open local file for reading */
  local_fd = open(local_path, O_RDONLY);
  if ( local_fd < 0 )
  {
    return LIB_SMB_LOCALFILE_READ_ERROR;
  }
  
  /* open / create remote file */
  remote_fd = lib_smbc_creat(smb_path, state.st_mode);
  
  if ( remote_fd < 0 )
  {
    /* error creating remote file */
    close(local_fd);
    return LIB_SMB_SMBFILE_OPEN_ERROR;
  }
  
  /* get the length of the local file */
  file_len = lib_smb_get_local_file_length(local_fd);
  
  /* copy the file, always 8K at once */
  current_len = 0;
  
  do
  {
    size = file_len - current_len;
    if ( size > LIB_SMB_COPY_SIZE )
      size = LIB_SMB_COPY_SIZE;

    if ( size > 0 )
    {
      /* read from source */
      read_len = read(local_fd, puffer, size);
      /* write to target */
      if ( lib_smbc_write(remote_fd, puffer, read_len) != read_len )
      {
        /* error writing bytes */
        result = LIB_SMB_SMBFILE_WRITE_ERROR;
        break;
      }
    }
    else
    {
      break;
    }
    
    /* abort copying ? */
    if ( c_struct->do_exit )
    {
      result = LIB_SMB_ACTION_ABORT;
      break;
    }
    
    current_len += size;
    
    /* invoke the callback, maybe there's a progress bar */
    if ( c_struct->callback != NULL )
    {
      c_struct->callback(current_len, size, &(c_struct->do_exit));
    }
  }
  while ( current_len < file_len );
  
  /* close file handle */
  close(local_fd);
  lib_smbc_close(remote_fd);
  
#endif 
 
  return result;
}

int lib_smb_copy_local_to_smb (char *local_path, char *machine,
                                                 char *share,
                                                 char *path,
                                                 file_copy_struct *c_struct)
{
  char remote_url[MAXSMBURLL+1];
  
  create_smb_url_complete(remote_url, machine, share, path, cempty, cempty, cempty, MAXSMBURLL);
  
  return lib_smb_copy_local_to_smb_path(local_path, remote_url, c_struct);
}

/* ------------------------------------------------------------------------- */
