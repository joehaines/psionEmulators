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

#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdlib.h>
#include "utility.h"
#include "preferences.h"
#include "io.h"
#include "data.h"
#include "smbif.h"


char cempty[] = {0};

/*--------------------------------------------------------------------------*/

void slist_free_with_data (GSList **list)
{
 GSList *thislist;

 thislist = *list;
 while ( thislist )
 {
   g_free(thislist->data);
   thislist = thislist->next; 
 }
 if ( *list ) 
 { 
   g_slist_free(*list);
   *list = (GSList*)NULL;
 }
}

/*--------------------------------------------------------------------------*/

unsigned char is_tab (char c)
{
  return ( c == '\t' );
}

unsigned char whitespace (char token)
{
  unsigned char ret = 0;
  
  switch ( token )
  {
    case ' ' :
    case '\n':
    case '\r':
    case '\f':
    case '\t':
    case 0   :
        ret = 1;
        break;
  }
  
  return ret;
}

/* does a search for a certain character,
   return value = pointer to character of '0' on string end */
char *read_until_char (char *mem, char token)
{
  while ( *mem != token )
  {
    /* end of string ? */
    if ( !(*mem) )
      break;
    mem++;
  }
  
  return mem;
}

char *read_until_whitespace (char *mem)
{
  while ( !whitespace(*mem) )
    mem++;
    
  return mem;
}

char *read_after_whitespace (char *mem)
{
  while ( whitespace_sameline(*mem) ) {
    if ( !(*mem) )
      break;
    mem++;
  }
  return mem;
}

char *read_until_last_whitespace (char *mem)
{
  int len;
  
  len = strlen(mem);
  while ( len && (whitespace(mem[len])) ) {
    len--;
  }
  len++;
  return (mem + len);
}

unsigned char whitespace_sameline(char token)
{
  unsigned char ret = 0;
  
  switch ( token ) {
    case ' ' :
    case '\t':
    case 0   :
        ret = 1;
        break;
  }
  
  return ret;
}

char *skip_nonspaces (char *s)
{
  while ( *s && !is_tab(*s) )
    s++;
  return s;
}

/*--------------------------------------------------------------------------*/

/* buffer 'dest' must be at least (len + 1) bytes */

void string_ncopy (char *dest, const char *src, size_t len)
{
  if ( !is_empty_string(src) )
    strncpy(dest, src, len);
  else
    dest[0] = 0;
  dest[len] = 0;
}

/* buffer 'dest' must be at least (len + 1) bytes */
void string_ncat (char *dest, const char *src, size_t len)
{
  size_t l, diff;
  
  l = strlen(dest);
  diff = len - l;
  if ( diff > 0 )
  {
    strncat(dest, src, diff);
    dest[len] = 0;
  }
}

void string_trim_left (char **str)
{
  while ( (**str != 0) && whitespace(**str) )
  {
    (*str)++;
  }
}

void string_trim_right (char *str)
{
  size_t len;
  
  len = strlen(str);
  if ( len > 0 )
  {
    len--;
    while ( len > 0 )
    {
      if ( whitespace(str[len]) )
      {
        str[len] = 0;
        len--;
      }
      else
      {
        break;
      }
    }
  }
}

void string_trim (char **str)
{
  string_trim_left(str);
  string_trim_right(*str);
}

/* replaces a character in a string with another character */
void string_replace_char_by_char (char *str, char old, char replace)
{
  int len, i;

  if ( !is_empty_string(str) )
  {
    if ( replace > 0 )
    {
      len = strlen(str);
      for ( i = 0; i < len; i++ )
      {
        if ( str[i] == old )
          str[i] = replace;
      }
    }
  }
}

/*
 *  tokenizes a string by whitespaces.
 *  take a look to " and ' characters.
 */

int tokenize_string(char **dest, char *src, int max_token)
{
  char *next;
  int length;
  int token_count = 0;
  unsigned char do_further;
  
  *dest = NULL;
  if ( !src )
    return 0;
  
  while ( token_count < (max_token - 1) )
  {
    string_trim(&src);
    length = strlen(src);
    if ( !length )
      break;
    
    do_further = 0;
    
    if ( *src == '"' )
    {
      src++;
      next = read_until_char(src, '"');
      if ( *next == '"' )
      {
        *next = 0;
        do_further = 1;
      }
      
    }
    else if ( *src == '\'' )
    {
      src++;
      next = read_until_char(src, '\'');
      if ( *next == '\'' )
      {
        *next = 0;
        do_further = 1;
      }
    }
    else
    {
      next = read_until_whitespace(src);
      if ( *next != 0 )
        do_further = 1;
    }
    
    dest[token_count++] = src;
    
    if ( !do_further )
    {
      break;
    }
    
/*    if ( *next == 0 )
    {
      break;
    }*/
    
    *next = 0;
    src = next + 1;
  }
  
  dest[token_count] = NULL;
  
  return token_count;
}

static unsigned char need_escaping[] = { ' ', '\t', '\n', '\\' };

char *mangle(unsigned char *s)
{ 
 char *ss, *sp; int n;

 n = strlen((char*)s);
 ss = sp = (char*)(g_malloc(4*n+1));
 
 while ( 1 )
 {
  for (n = 0; n < (int)(sizeof(need_escaping)); n++)
  {
    if (*s == need_escaping[n])
    {
     *sp++ = '\\';
     *sp++ = '0' + ((*s & 0300) >> 6);
     *sp++ = '0' + ((*s & 070) >> 3);
     *sp++ = '0' + (*s & 07);
     goto next;
    }
  }
  *sp++ = *s;
  if ( *s == 0 )
  break;
  next: s++;
 }
 return ss;
}

#define isoctal(a) (((a) & ~7) == '0')

/* returns malloced pointer - no more strdup required */

void unmangle (char *dest, char *s, int size)
{
  char *ss, *sp;
  
  ss = skip_nonspaces(s);
  sp = dest;
  
  while ( s != ss )
  {
    if ( (*s == '\\') && isoctal(s[1]) && isoctal(s[2]) && isoctal(s[3]) )
    {
      *sp++ = 64*(s[1] & 7) + 8*(s[2] & 7) + (s[3] & 7);
      s += 4;
    }
    else
      *sp++ = *s++;
    
    if ( sp - dest >= size )
      break;
  }
  *sp = 0;
}

unsigned char is_empty_string (const char * str)
{
  int len;
  int i;

  if ( str )
  {
    len = strlen(str);
    if ( len )
    {
      for ( i = 0; i < len; i++ )
      {
        if ( !whitespace(str[i]) )
        {
          return 0;
        }
      }
    }
  }
  return 1;
}

/*--------------------------------------------------------------------------*/

unsigned char is_ipv4_address (const char * str)
{
  struct in_addr address;
  
#ifdef SUNOS5
  if ( inet_addr(str) != INADDR_NONE )
#else
  if ( inet_aton(str, &address) )
#endif
    return 1;                 /* string is ipv4 address */
  else
    return 0;
}

/*--------------------------------------------------------------------------*/

/* 1 = 'file exists', 0 = 'not exist' */
unsigned char file_exist (const char * filename)
{
  FILE *fd;
  
  fd = fopen(filename, "r");
  if ( fd > 0 ) {
    fclose(fd);
    return 1;
  }
  else
    return 0;
}

/* 1 = 'path exists', 0 = 'not exists' */
unsigned char path_exist (const char *path)
{
  DIR *dir;
  
  dir = opendir(path);
  closedir(dir);
  return ( dir != NULL ) ? 1 : 0;
}

/* creates a path/directory, 1 = 'success' */
unsigned char path_create (const char *path, mode_t modus)
{
  char temp[PATH_LEN+1];
  char *slashilein;
  struct stat fstat;
  unsigned char ok;
  
  string_ncopy(temp, path, PATH_LEN);
  ok = 1;
  slashilein = temp;
  
  while ( (slashilein = index(slashilein, '/')) )
  {
    *slashilein = 0;
    if ( *temp != 0 )
    {
      if ( !stat(temp, &fstat) )
      {
        if ( !S_ISDIR(fstat.st_mode) )
        {
          ok = 0;
          break;
        }
      }
      else
      {
        if ( mkdir(temp, modus) != 0 )
        {
          ok = 0;
          break;
        }
      }
    }
    *slashilein = '/';
    slashilein++;
  }
  return ok;
}

/* looks if the path exists, if not -> create */
unsigned char path_check_create (const char *path, mode_t modus)
{
  if ( !path_exist(path) )
  {
    return path_create(path, modus);
  }
  else
  {
    return 1;
  }
}

/*--------------------------------------------------------------------------*/

static unsigned char compare_smb_name (const char *name1, const char *name2, int maxlen)
{
  int i;
  unsigned char ret;
  int len1, len2;
  static char str1[MASTER_LEN+1];
  static char str2[MASTER_LEN+1];
  
  /* if any empty compare string, abort */
  if ( is_empty_string(name1) )
    return 0;
  if ( is_empty_string(name2) )
    return 0;
  
  if ( maxlen > MASTER_LEN )
    maxlen = MASTER_LEN;
  string_ncopy(str1, name1, maxlen);
  string_ncopy(str2, name2, maxlen);
  len1 = strlen(name1);
  len2 = strlen(name2);
  for ( i = 0; i < len1; i++ )
  {
    if ( str1[i] == ' ' )
      str1[i] = '_';
  }
  for ( i = 0; i < len2; i++ )
  {
    if ( str2[i] == ' ' )
      str2[i] = '_';
  }
  ret = ( !strcasecmp(str1, str2) ) ? 1 : 0;

  return ret;
}

/* 1 = 'groupname equal', 0 = 'groupname not equal' */

unsigned char compare_smb_groupname (const char *group1, const char *group2)
{
  return compare_smb_name(group1, group2, MAXGROUPNAMEL);
}

unsigned char compare_smb_sharename (const char *share1, const char *share2)
{
  return compare_smb_name(share1, share2, MAXSHRNAMEL);
}

unsigned char compare_smb_machinename (const char *mach1, const char *mach2)
{
  return compare_smb_name(mach1, mach2, MAXMACHNAMEL);
}

/*--------------------------------------------------------------------------*/

/* 1 = 'path equal', 0 = 'path not equal' */

unsigned char compare_path (const char * path1, const char * path2)
{
  unsigned char ret;
  int len1, len2;
  static char str1[PATH_LEN+1];
  static char str2[PATH_LEN+1];
  
  string_ncopy(str1, path1, PATH_LEN);
  string_ncopy(str2, path2, PATH_LEN);
  len1 = strlen(str1);
  len2 = strlen(str2);
  /* handle path with ending '/' as same as without */
  if ( len1 > 1 )
  {
    if ( str1[len1-1] == '/' ) str1[len1-1] = 0;
  }
  if ( len2 > 1 )
  {
    if ( str2[len2-1] == '/' ) str2[len2-1] = 0;
  }
  ret = ( !strcmp(str1, str2) ) ? 1 : 0;

  return ret;
}

/*--------------------------------------------------------------------------*/

void merge_smb_service (char **service, char *machine, char *share,  size_t len)
{
  string_ncopy(*service, "//", len);
  string_ncat(*service, machine, len);
  string_ncat(*service, "/", len);
  string_ncat(*service, share, len);
}

/* extracts the machine and share name from an service
   e.g. service = "//mozart/aveverum"
    ->  machine = "mozart", share = "aveverum"
    attention: function does change the source buffer */
void extract_smb_service (char *service, char **machine, char **share)
{
  char *ctemp;
  size_t len;
  
  *machine = cempty;
  *share = cempty;
  ctemp = service;

  while ( *ctemp == '/' )
    ctemp++;
  /* beginning of machine name */
  *machine = ctemp;
  
  if ( *machine != 0 )
  {
    len = strcspn(ctemp, "/");
    if ( len > 0 )
    {
      ctemp[len] = 0;
      *share = ctemp + len + 1;
      /* if there is a '/' as last character, delete it */
      len = strlen(*share);
      len--;
      if ( len > 0 )
      {
        if ( (*share)[len] == '/' )
        {
          (*share)[len] = 0;
        }
      }
    }
  }
}

void merge_prefhost (char **host, char *name, char *group, char *ip, size_t len)
{
  string_ncopy(*host, name, len);
  string_ncat(*host, "/", len);
  string_ncat(*host, group, len);
  string_ncat(*host, "/", len);
  string_ncat(*host, ip, len);
}

void extract_prefhost (char *host, char **name, char **group, char **ip)
{
  char *ctok;

  *name = cempty;
  *group = cempty;
  *ip = cempty;

  if ( !is_empty_string(host) )
  {
    *name = host;

    ctok = read_until_char(host, '/');
    if ( *ctok )
    {
      *ctok = 0;
      ctok++;
      *group = ctok;
      
      ctok = read_until_char(ctok, '/');
      if ( *ctok )
      {
        *ctok = 0;
        *ip = ctok + 1;
      }
    }
  }
}

void merge_mount (char **cmount, mem_mount_struct *mount, size_t len, int first_arg)
{
  GSList *templist;
  char *arg;
  char *marg;
  int argnum;

  /*mangle((unsigned char*)share);
  string_ncopy(*cmount, share, len);*/
  argnum = 0;
  if ( argnum >= first_arg )
  {
    string_ncopy(*cmount, " ", len);
    marg = mangle(mount->mountpoint);
    string_ncat(*cmount, marg, len);
    g_free(marg);
  }
  
  templist = mount->arglist;
  argnum++;
  while ( templist != NULL )
  {
    arg = (char*)(templist->data);
    if ( (arg != NULL) && (argnum >= first_arg) )
    {
      string_ncat(*cmount, " ", len);
      marg = mangle(arg);
      string_ncat(*cmount, marg, len);
      g_free(marg);
    }
    argnum++;
    templist = templist->next;
  }
}

void extract_mount (char *cmount, mem_mount_struct *mount)
{
  char *ctok;
  char *arg;
  char *newarg;

  mount->type = mem_mount_none;
  strcpy(mount->mountpoint, cempty);
  mount->arglist = NULL;

  if ( !is_empty_string(cmount) )
  {
    /* extract mountpoint */
    ctok = read_until_char(cmount, ' ');
    if ( *ctok )
    {
      *ctok = 0;
      ctok++;
    }
    unmangle(cmount, cmount, strlen(cmount));
    string_ncopy(mount->mountpoint, cmount, PATH_LEN);
    /* extract arguments */
    arg = ctok;
    ctok = read_until_char(ctok, ' ');
    while ( *ctok )
    {
      *ctok = 0;
      ctok++;
      if ( !is_empty_string(arg) )
      {
        unmangle(arg, arg, strlen(arg));
        newarg = (char*)(g_malloc(strlen(arg)+1));
        strcpy(newarg, arg);
        mount->arglist = g_slist_append(mount->arglist, newarg);
      }
      arg = ctok;
      ctok = read_until_char(ctok, ' ');
      if ( !(*ctok) && arg )
      {
        /* last parameter */
        unmangle(arg, arg, strlen(arg));
        newarg = (char*)(g_malloc(strlen(arg)+1));
        strcpy(newarg, arg);
        mount->arglist = g_slist_append(mount->arglist, newarg);
      }
    }
  }
}

void merge_groupmaster (char **cmaster, group_master_struct *master, size_t len)
{
  string_ncopy(*cmaster, master->group, len);
  string_ncat(*cmaster, "/", len);
  string_ncat(*cmaster, master->name, len);
}

void extract_groupmaster (char *cmaster, char **group, char **name)
{
  char *ctok;

  *group = cempty;
  *name = cempty;

  if ( !is_empty_string(cmaster) )
  {
    *group = cmaster;

    ctok = read_until_char(cmaster, '/');
    if ( *ctok )
    {
      *ctok = 0;
      string_trim(group);
      *name = ctok + 1;
      string_trim(name);
    }
  }
}

#define MOUNT_MAX_ARG                29

void utility_do_mount (mem_mount_struct *mount)
{
  char *arg;
  char *argv[MOUNT_MAX_ARG+1];
  int argc;
  GSList *templist;
  unsigned char already_mounted = 0;

  argc = 1;
  templist = mount->arglist;
  argv[0] = pref.v.smbmount_exe;
  while ( templist != NULL )
  {
    arg = templist->data;
    if ( (arg != NULL) && (argc < MOUNT_MAX_ARG) )
    {
      argv[argc] = arg;
    }
    templist = templist->next;
    argc++;
  }
  argv[argc] = NULL;
  
  /* makes an entry in the memorized mounts list */
  data_mem_mount_add(mem_mount_user, mount->mountpoint, &argv[1]);
  
  /* share already mounted ? */
  if ( mount_list_search_by_mountpoint(mount->mountpoint) != NULL )
  {
    already_mounted = 1;
  }

  /* do mount action */
  if ( !already_mounted )
  {
    smb_mount_memorized(argv[0], &argv[1], 0);
  }
}

/*
 *  use this function to do any changes in the mount path
 *  it will be executed on smbmount execution
 */
void prepare_mount_path (char *path)
{
  /* if 'Use default path' + 'Replace spaces by underscores', do this */
  if ( pref.v.replace_space_mount && pref.v.root_mnt_enable )
  {
    string_replace_char_by_char(path, ' ', '_');
  }
}

/*--------------------------------------------------------------------------*/

/* get row in GSList with specified text */
/* -1 = error, >0 = ok */

int get_textcase_in_gslist (GSList *list, char *text)
{
  char *entry;
  unsigned char ok;
  int ireturn;
  
  ok = 0;
  ireturn = 0;
  while ( list )
  {
    if ( list->data != NULL )
    {
      entry = (char*)list->data;
      if ( !strcasecmp(text, entry) )
      {
        ok = 1;
        break;
      }
      ireturn++;
    }
    list = list->next;
  }
  if ( !ok )
    ireturn = -1;
  
  return ireturn;
}

/*--------------------------------------------------------------------------*/

#define MAX_MAKRO                2

#define MAKRO_MOUNTPOINT         0
#define MAKRO_ENVIRONMENT        1

makro_struct makro_tab[MAX_MAKRO] = {
        { "MOUNTPOINT", (char*)NULL },
        { "ENV", (char*)NULL }
};

static void makro_set_mountpoint (char *mountpoint)
{
  makro_tab[MAKRO_MOUNTPOINT].replace_string = mountpoint;
}

void makro_replace (char *target, char *source, int maxlen)
{
  char *pmakro;
  int len;
  int i;
  int found;
  void *mem;
  char *src, *env, *smangle;
  
  mem = g_malloc(strlen(source)+1);
  if ( !mem )
    return;
  
  src = (char*)mem;
  strcpy(src, source);
  pmakro = strchr(src, '$');
  
  if ( !pmakro )
  {
    /* no makro */
    string_ncopy(target, src, maxlen);
  }
  else
  {
    target[0] = 0;
    while ( pmakro )
    {
      len = pmakro - src;
      len++;
      *pmakro = 0;
      pmakro++;
      string_ncat(target, src, maxlen);
      src += len;
      found = 0;
      i = 0;
      while ( i < MAX_MAKRO )
      {
        if ( !strncmp(pmakro, makro_tab[i].identifier, strlen(makro_tab[i].identifier)) )
        {
          switch ( i )
          {
            /* "$MOUNTPOINT" */
            case MAKRO_MOUNTPOINT:
                src += strlen(makro_tab[i].identifier);
                if ( makro_tab[i].replace_string != NULL )
                {
                  /* maybe there are spaces */
                  smangle = mangle(makro_tab[i].replace_string);
                  string_ncat(target, smangle, maxlen);
                  g_free(smangle);
                }
                break;
            
            /* "$ENV(...)" */
            case MAKRO_ENVIRONMENT:
                src += strlen(makro_tab[i].identifier);
                if ( *src == '(' )
                {
                  src++;
                  pmakro = strchr(src, ')');
                  if ( pmakro )
                  {
                    *pmakro = 0;
                    env = getenv(src);
                    if ( env != NULL )
                    {
                      string_ncat(target, env, maxlen);
                    }
                    src = pmakro + 1;
                  }
                  else
                  {
                    pmakro = src;
                  }
                }
                break;
                
          }
          found = 1;
          break;
        }
        if ( found )
        {
          break;
        }
        i++;
      }
      pmakro = strchr(pmakro, '$');
      if ( !pmakro )
      {
        string_ncat(target, src, maxlen);
      }
    }
  }
  g_free(mem);
}

void postmount_execute (char *mountpoint)
{
  char *exe;
  char *argv[12];
  char *token;
  int argc = 0;
  pid_t pid;
  /*makro_struct makro_list[] = { {"MOUNTPOINT", mountpoint},
                                { NULL, NULL} };*/
  
  exe = (char*)g_malloc(PATH_LEN+1);
  makro_set_mountpoint(mountpoint);
  string_ncopy(exe, pref_get_filemanager_exe(), PATH_LEN);

  token = strtok(exe, " ");
  
  while ( token )
  {
    if ( argc >= 11 )
    {
      g_error(_("too many arguments !\n"));
      g_free(exe);
      return;
    }

    argv[argc] = token;
    /* necessary because of possible spaces in arguments */
    unmangle(token, token, strlen(token));
    argc++;
    token = strtok((char*)NULL, " ");
  }
  argv[argc] = (char*)NULL;
  argc++;
  
  /* use this to avoid zombies */
  if ( (pid = fork()) < 0 )
  {
    g_error(_("fork error !\n"));
  }
  else if ( pid == 0 )
  {
    /* child process */
    if ( (pid = fork()) < 0 )
    {
      g_error(_("fork error !\n"));
    }
    else if ( pid > 0 )
    {
      /* delete child process */
      _exit(0);
    }
    /* grandchild process */
    execvp(argv[0], argv);
    g_error(_("error running program !\n"));
    _exit(1);
  }
  /* parent is waiting for child */
  if ( waitpid(pid, (int*)NULL, 0) != pid )
  {
    g_error(_("waitpid error !\n"));
  }

  g_free(exe);
}

/*--------------------------------------------------------------------------*/

/* returns the root mount directory with all makros expanded */
char *pref_get_root_mnt_dir (void)
{
  static char dir[PATH_LEN+1];
  
  makro_replace(dir, pref.v.root_mnt_dir, PATH_LEN);
  return dir;
}

char *pref_get_filemanager_exe (void)
{
  static char exe[PATH_LEN+1];
  
  makro_replace(exe, pref.v.filemanager_exe, PATH_LEN);
  return exe;
}

char *pref_get_smbclient_exe (void)
{
  static char exe[PATH_LEN+1];
  
  makro_replace(exe, pref.v.smbclient_exe, PATH_LEN);
  return exe;
}

char *pref_get_nmblookup_exe (void)
{
  static char exe[PATH_LEN+1];
  
  makro_replace(exe, pref.v.nmblookup_exe, PATH_LEN);
  return exe;
}

char *pref_get_smbmount_exe (void)
{
  static char exe[PATH_LEN+1];
  
  makro_replace(exe, pref.v.smbmount_exe, PATH_LEN);
  return exe;
}

char *pref_get_smbumount_exe (void)
{
  static char exe[PATH_LEN+1];
  
  makro_replace(exe, pref.v.smbumount_exe, PATH_LEN);
  return exe;
}

/*--------------------------------------------------------------------------*/

static unsigned char network_image_first_folder;
static unsigned char network_image_first_printer;

static void network_image_share_enumerate (share_struct *share, gpointer data)
{
  FILE *fd;
  
  fd = (FILE*)data;
  if ( (fd != NULL) && (share != NULL) )
  {
    switch ( share->st )
    {
      case shtype_folder:
          if ( network_image_first_folder )
          {
            io_write_network_image_shareheader(fd);
          }
          network_image_first_folder = 0;
          io_write_network_image_share(fd, share->name);
          break;
      
      case shtype_printer:
          if ( network_image_first_printer )
          {
            io_write_network_image_printerheader(fd);
          }
          network_image_first_printer = 0;
          io_write_network_image_share(fd, share->name);
          break;
    }
  }
}

static void network_image_machine_enumerate (machine_struct *machine, gpointer data)
{
  FILE *fd;
  
  fd = (FILE*)data;
  if ( (fd != NULL) && (machine != NULL) )
  {
    io_write_network_image_machine(fd, machine->name);
    /* enumerate all shares of machine */
    network_image_first_folder = 1;
    network_image_first_printer = 1;
    share_list_enumerate(machine->group->name, machine->name, network_image_share_enumerate, (gpointer)fd);
  }
}

static void network_image_group_enumerate (group_struct *group, gpointer data)
{
  FILE *fd;
  
  fd = (FILE*)data;
  if ( (fd != NULL) && (group != NULL) )
  {
    io_write_network_image_workgroup(fd, group->name);
    /* enumerate all machines of workgroup */
    machine_list_enumerate(group->name, network_image_machine_enumerate, (gpointer)fd);
  }
}

/* reads all the groups, machines and shares stored in data area
   and writes it to 'filename' as text */

void network_image_export (char *filename)
{
  FILE *fd;
  
  fd = io_write_open_zero(filename, 1);
  if ( !fd )
  {
    return;
  }
    
  io_write_network_image_header(fd);
  
  /* enumerate all groups */
  group_list_enumerate(network_image_group_enumerate, (gpointer)fd);
  
  fclose(fd);
}

/*--------------------------------------------------------------------------*/
