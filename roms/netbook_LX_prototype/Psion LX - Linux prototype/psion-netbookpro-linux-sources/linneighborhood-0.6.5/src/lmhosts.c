/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   name query routines
   Copyright (C) Andrew Tridgell 1994-1998
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
   
*/


/*
   Using code from Samba 2.0.5a to import lmhosts file,
   modified by Richard Stemmer and Hans Schmid
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <glib.h>
#include "samba.h"


/****************************************************************************
expand a pointer to be a particular size
****************************************************************************/
static void *Realloc(void *p,size_t size)
{
  void *ret=NULL;

  if (size == 0) {
    if (p) free(p);
    return NULL;
  }

  if (!p)
    ret = (void *)malloc(size);
  else
    ret = (void *)realloc(p,size);

  return(ret);
}

/****************************************************************************
read a line from a file with possible \ continuation chars. 
Blanks at the start or end of a line are stripped.
The string will be allocated if s2 is NULL
****************************************************************************/
static char *fgets_slash(char *s2,int maxlen,FILE *f)
{
  char *s=s2;
  int len = 0;
  int c;
  unsigned char start_of_line = 1;

  if (feof(f))
    return(NULL);

  if (!s2)
    {
      maxlen = MIN(maxlen,8);
      s = (char *)Realloc(s,maxlen);
    }

  if (!s || maxlen < 2) return(NULL);

  *s = 0;

  while (len < maxlen-1)
    {
      c = getc(f);
      switch (c)
	{
	case '\r':
	  break;
	case '\n':
	  while (len > 0 && s[len-1] == ' ')
	    {
	      s[--len] = 0;
	    }
	  if (len > 0 && s[len-1] == '\\')
	    {
	      s[--len] = 0;
	      start_of_line = 1;
	      break;
	    }
	  return(s);
	case EOF:
	  if (len <= 0 && !s2) 
	    free(s);
	  return(len>0?s:NULL);
	case ' ':
	  if (start_of_line)
	    break;
	default:
	  start_of_line = 0;
	  s[len++] = c;
	  s[len] = 0;
	}
      if (!s2 && len > maxlen-3)
	{
	  maxlen *= 2;
	  s = (char *)Realloc(s,maxlen);
	  if (!s) return(NULL);
	}
    }
  return(s);
}

/*******************************************************************
safe string copy into a known length string. maxlength does not
include the terminating zero.
********************************************************************/
static char *safe_strcpy(char *dest,const char *src, size_t maxlength)
{
    size_t len;

    if (!dest) {
        return NULL;
    }

    if (!src) {
        *dest = 0;
        return dest;
    }  

    len = strlen(src);

    if (len > maxlength) {
	    len = maxlength;
    }
      
    memcpy(dest, src, len);
    dest[len] = 0;
    return dest;
}  

/****************************************************************************
  Get the next token from a string, return False if none found
  handles double-quotes. 
Based on a routine by GJC@VILLAGE.COM. 
Extensively modified by Andrew.Tridgell@anu.edu.au
****************************************************************************/

static char *last_ptr = NULL;

static unsigned char next_token(char **ptr,char *buff,char *sep, size_t bufsize)
{
  char *s;
  unsigned char quoted;
  size_t len=1;

  if (!ptr) ptr = &last_ptr;
  if (!ptr) return(0);

  s = *ptr;

  /* default to simple separators */
  if (!sep) sep = " \t\n\r";

  /* find the first non sep char */
  while(*s && strchr(sep,*s)) s++;

  /* nothing left? */
  if (! *s) return(0);

  /* copy over the token */
  for (quoted = 0; len < bufsize && *s && (quoted || !strchr(sep,*s)); s++)
    {
	    if (*s == '\"') {
		    quoted = !quoted;
	    } else {
		    len++;
		    *buff++ = *s;
	    }
    }

  *ptr = (*s) ? s+1 : s;  
  *buff = 0;
  last_ptr = *ptr;

  return(1);
}

/*******************************************************************
  convert a string to lower case
********************************************************************/
static void strlower(char *s)
{
  while (*s)
  {
    if (isupper(*s))
      *s = tolower(*s);
    s++;
  }
}

/*******************************************************************
  convert a string to upper case
********************************************************************/
static void strupper(char *s)
{
  while (*s)
  {
    if (islower(*s))
      *s = toupper(*s);
    s++;
  }
}

/****************************************************************************
a wrapper for gethostbyname() that tries with all lower and all upper case 
if the initial name fails
****************************************************************************/
static struct hostent *Get_Hostbyname(const char *name)
{
  char *name2 = strdup(name);
  struct hostent *ret;

  if (!name2)
    {
      exit(0);
    }

   
  /* 
   * This next test is redundant and causes some systems (with
   * broken isalnum() calls) problems.
   * JRA.
   */

#if 0
  if (!isalnum(*name2))
    {
      free(name2);
      return(NULL);
    }
#endif /* 0 */

  ret = gethostbyname(name2);
  if (ret != NULL)
    {
      free(name2);
      return(ret);
    }

  /* try with all lowercase */
  strlower(name2);
  ret = gethostbyname(name2);
  if (ret != NULL)
    {
      free(name2);
      return(ret);
    }

  /* try with all uppercase */
  strupper(name2);
  ret = gethostbyname(name2);
  if (ret != NULL)
    {
      free(name2);
      return(ret);
    }
  
  /* nothing works :-( */
  free(name2);
  return(NULL);
}

/*******************************************************************
copy an IP address from one buffer to another
********************************************************************/
static void putip(void *dest,void *src)
{
  memcpy(dest,src,4);
}

/****************************************************************************
interpret an internet address or name into an IP address in 4 byte form
****************************************************************************/
static uint32 interpret_addr(const char *str)
{
  struct hostent *hp;
  uint32 res;
  int i;
  unsigned char pure_address = 1;

  if (strcmp(str,"0.0.0.0") == 0) return(0);
  if (strcmp(str,"255.255.255.255") == 0) return(0xFFFFFFFF);

  for (i=0; pure_address && str[i]; i++)
    if (!(isdigit((int)str[i]) || str[i] == '.')) 
      pure_address = 0;

  /* if it's in the form of an IP address then get the lib to interpret it */
  if (pure_address) {
    res = inet_addr(str);
  } else {
    /* otherwise assume it's a network name of some sort and use 
       Get_Hostbyname */
    if ((hp = Get_Hostbyname(str)) == 0) {
      return 0;
    }
    if(hp->h_addr == NULL) {
      return 0;
    }
    putip((char *)&res,(char *)hp->h_addr);
  }

  if (res == (uint32)-1) return(0);

  return(res);
}

/*******************************************************************
  a convenient addition to interpret_addr()
  ******************************************************************/
static struct in_addr *interpret_addr2(const char *str)
{
  static struct in_addr ret;
  uint32 a = interpret_addr(str);
  ret.s_addr = a;
  return(&ret);
}

/********************************************************
 Start parsing the lmhosts file.
*********************************************************/

FILE *startlmhosts(const char *fname)
{
  FILE *fp = fopen(fname,"r");
  if (!fp) {
    return NULL;
  }
  return fp;
}

/********************************************************
 Parse the next line in the lmhosts file.
*********************************************************/

BOOL getlmhostsent( FILE *fp, pstring name, int *name_type, struct in_addr *ipaddr)
{
  pstring line;

  while(!feof(fp) && !ferror(fp)) {
    pstring ip,flags,extra;
    char *ptr;
    int count = 0;

    *name_type = -1;

    if (!fgets_slash(line,sizeof(pstring),fp))
      continue;

    if (*line == '#')
      continue;

    safe_strcpy(ip,"",STRLEN);
    safe_strcpy(name,"",STRLEN);
    safe_strcpy(flags,"",STRLEN);

    ptr = line;

    if (next_token(&ptr,ip   ,NULL,sizeof(ip)))
      ++count;
    if (next_token(&ptr,name ,NULL, sizeof(pstring)))
      ++count;
    if (next_token(&ptr,flags,NULL, sizeof(flags)))
      ++count;
    if (next_token(&ptr,extra,NULL, sizeof(extra)))
      ++count;

    if (count <= 0)
      continue;

    if (count > 0 && count < 2)
    {
      continue;
    }

    if (count >= 4)
    {
      continue;
    }

    if (strchr(flags,'G') || strchr(flags,'S'))
    {
      continue;
    }

    *ipaddr = *interpret_addr2(ip);

    /* Extra feature. If the name ends in '#XX', where XX is a hex number,
       then only add that name type. */
    if((ptr = strchr(name, '#')) != NULL)
    {
      char *endptr;

      ptr++;
      *name_type = (int)strtol(ptr, &endptr, 16);

      if(!*ptr || (endptr == ptr))
      {
        continue;
      }

      *(--ptr) = '\0'; /* Truncate at the '#' */
    }

    return True;
  }

  return False;
}

/********************************************************
 Finish parsing the lmhosts file.
*********************************************************/

void endlmhosts(FILE *fp)
{
  fclose(fp);
}
