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

#include <string.h>
#include <stdio.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <limits.h>
#include "smbif.h"
#include "define.h"
#include "browsewrap.h"
#include "guiwrap.h"
#include "utility.h"
#include "preferences.h"

#include <stdlib.h>

#define MAXCOMMANDL 100 /*should be enough for user/passwd*/

char logstr[PATH_MAX+1];

typedef enum _GBROSWE_STATE {
        search_head,
	search_entryhead,
        search_dashes,
        search_entry
} GBROSWE_STATE;


typedef struct _GBROWSE_STRUCT{
  int fd[2];  
  pid_t childpid;
  gint input_tag;
  char linebuf[MAXSMBLINEL+1];
  char masterbrowser[MAXSMBLINEL+1];
  char userpasswdstr[MAXLOGSTRLEN+1],userdummystr[MAXLOGSTRLEN+1];
  GROUPMASTER_STRUCT group_master;
  int port;
  int  linepos,linenum,firstbrowse,foundsecondentries;
  int  group1pos,master1pos; 
  GSList *groupmasterlist;
  GBROSWE_STATE browse_state;
  GROUPS_BROWSE_INFO *browse_info;
  char lasttab;
} GBROWSE_STRUCT;


typedef struct _HBROWSE_STRUCT{
  int fd[2];  
  pid_t childpid;
  gint input_tag;
  char linebuf[MAXSMBLINEL+1];
  GROUPMASTER_STRUCT group_master;
  int port;
  int  linepos,linenum;
  int  machine1pos,comment1pos; 
  GSList *machinecommentlist;
  GBROSWE_STATE browse_state;
  MACHINES_BROWSE_INFO *browse_info;
  char lasttab;
} HBROWSE_STRUCT;

typedef struct _SBROWSE_STRUCT{
  int fd[2];  
  pid_t childpid;
  gint input_tag;
  char linebuf[MAXSMBLINEL+1];
  char group_name[MAXGROUPNAMEL+1],machine_name[MAXMACHNAMEL+1];
  char user[USER_LEN+1], password[PASSWORD_LEN+1];
  int port;
  int  linepos,linenum;
  int  share1pos,shtype1pos,comment1pos;
  GSList *sharecommenttypelist;
/*  char ipaddr[MAXMACHNAMEL+1]; */
  MACHINEINFO_STRUCT machineinfo;
  GBROSWE_STATE browse_state;
  SHARES_BROWSE_INFO *browse_info;
  char lasttab;
} SBROWSE_STRUCT;

typedef struct _LOOKUP_STRUCT{
  int fd;
  pid_t childpid;
  gint input_tag;
  char linebuf[MAXSMBLINEL+1];
  int  linepos,linenum;
  char ipaddr[MAXMACHNAMEL+1];
  char group_name[MAXGROUPNAMEL+1],machine_name[MAXMACHNAMEL+1];
  GBROSWE_STATE browse_state;
}LOOKUP_STRUCT;


static GSList *gbrowsedatlist=NULL,
              *hbrowsedatlist=NULL,
              *sbrowsedatlist=NULL,
              *lookupbynamelist=NULL,
              *lookupbyiplist=NULL;


char nomaster[] = "?nomaster?";

unsigned  char sorted_sharelist = TRUE;


void log_execvp_str(const char *path, char *const argv[])
{
  char logstr[MAXLOGSTRLEN+1];
  logstr[MAXLOGSTRLEN]=0;
  strncpy(logstr,path,MAXLOGSTRLEN);
  while(*(++argv))
  {
    strncat(logstr," ",MAXLOGSTRLEN-strlen(logstr));
    strncat(logstr,*argv,MAXLOGSTRLEN-strlen(logstr));
  }
  gui_log_window_insert_string(logstr,1);
}


static void copybrackstr(char *dest,char *src,int maxstrlen)
{
  char *posi1,*posi2;
  int copylen;
  if ((posi1=strchr(src,'[')) &&(posi2=strchr(src,']')))
  {
    if ((copylen=posi2-posi1-1)>maxstrlen) copylen=maxstrlen;
    strncpy(dest,++posi1,copylen);
    *(dest+copylen)=0;
  }
  else
   *dest=0;
}


static void copyentrystr(char *dest,char *src,int posi1,int posi2,int maxstrlen)
{
  int n,slen;
  char *endchar;
  slen=strlen(src);
  if (posi1>=slen)
   { 
    *dest=0;
    return;
   }
  n=(posi2-posi1)+1;
  if (n>maxstrlen) 
    n=maxstrlen;
  if (n>slen-posi1) 
    n=slen-posi1;    
  memcpy(dest,src+posi1,n);
  endchar=dest+n;
  *endchar--=0;
  while((n--)>0 && *endchar==' ')
   {
    *endchar--=0;
   }
}


#define  GROUPE_IGNORE_NUM 1
/* ignore group lines beginning with:*/
static char *ignore_group_lines[GROUPE_IGNORE_NUM] = {
 "iNtEl.nBcBaGr" /*Intels Landesk*/
 };

static char group_ignored(char *group)
{
 int i;
 for(i=0;i<GROUPE_IGNORE_NUM;i++)
 {
   if (!strncmp(group,ignore_group_lines[i],strlen(ignore_group_lines[i])))
     return(TRUE);
 }
 return(FALSE);
}


static int getnodename(char *dest,char *linebuf,int *nodetype,int *isgroup,int *isactive,int maxstrlen)
{
  char *posi1,*typeposi,found=FALSE;
  typeposi=linebuf;
  while(typeposi<linebuf+strlen(linebuf)-5)
  {
    if (*typeposi=='<' && *(typeposi+3)=='>'&& *(typeposi+4)==' ' && *(typeposi+5)=='-')
    {
      found=TRUE;
      break;
    } 
    typeposi++;
  }
  if (!found) return(FALSE);
  posi1=linebuf;
  while(posi1<typeposi)
    {
     if ((*posi1)!=' ' && (*posi1)!='\t') break;
     posi1++;
    }
  if (typeposi-posi1<=1) return(FALSE);
  copyentrystr(dest,posi1,0,typeposi-posi1-1,maxstrlen);
  sscanf(typeposi,"<%2xd",nodetype);
  *isgroup= strstr(typeposi,"<GROUP>")!=NULL;
  *isactive=strstr(typeposi,"<ACTIVE>")!=NULL;
  return(TRUE);
}

#define EXPANDTABS /*don't know why sometimes tabs (except col 1) are in smbclient output,
                    I can't find them in samba's client.c */
#define TABSPACING 8

static int fdread_line_ok2(int fd,int *linec,int *linepos,char *linebuf,
                           char search_entry,char *lasttab)
{  /*Always read whole line, this can block if application stops
    output and goes into background, but this does not happen with smbclient*/
 char c,c2;
 int  numread,ok=0;
 do{
   if (lasttab&&(*lasttab))
   {
     c=*lasttab;
     *lasttab=0;
     numread=1;
   }
   else
     numread=read(fd,&c,1);
   if (numread==0) 
     *linec=EOF;
   else
     *linec=c;  
   if (numread && c!='\n')
    {
#ifdef EXPANDTABS
     if (c=='\t'){
        int tabspaces=TABSPACING-(*linepos) % TABSPACING;
        if (*linepos<= (MAXSMBLINEL-tabspaces))
        {  
           memset(linebuf+(*linepos),' ',tabspaces);
           (*linepos)+=tabspaces;
        }
     }
     else 
#endif
      if (*linepos< MAXSMBLINEL)
        *(linebuf+(*linepos)++)=c;
     ok=0;  
    } 
    else
    {
      if (!search_entry)
      {
        *(linebuf+*linepos)=0;
        ok=*linepos>0 || *linec=='\n';
      }
      else
      {
        numread=read(fd,&c2,1);
        if (numread==0)
        {
          *(linebuf+*linepos)=0;
          ok=*linepos>0 || *linec=='\n';
          *linec=EOF;
        }
        else
        {
          if (c2=='\t'||c2=='\n')
          {
            *lasttab=c2;
            *(linebuf+*linepos)=0;
            ok=*linepos>0 || *linec=='\n';
          }
          else
          {
           if (*linepos< (MAXSMBLINEL-1))
           {
             *(linebuf+(*linepos)++)=' ';/*c*/
             *(linebuf+(*linepos)++)=c2;
           }
           ok=0;
          }
        }
      }
    } 
 }while(!((*linec==EOF)||ok));
 return(ok);
}


static void GetSMBGroups_inputcallback(GBROWSE_STRUCT *browsedat,gint source, GdkInputCondition cond)
{
  int   linec,pipe_nok;
  char *posi1,*posi2;
  GROUPMASTER_STRUCT *groupmasterentry;   
  GetSMBStateType smbstate;
  GSList *gmlist;
  char logstr[MAXLOGSTRLEN+1];
  char port_str[16];
  char *cport = NULL;
  char *cportkey = NULL;
  char *cArg[20];
  char *cAddArg[10];
  int argcount = 0;
  char add_arg_string[ARG_LEN+1];
  int add_argcount = 0;
  int loop;
  
  if ( (browsedat->port >= 0) && (browsedat->port < 65536) )
  {
    cportkey = "-p";
    sprintf(port_str, "%d", browsedat->port);
    cport = port_str;
  }
  
  if(cond & GDK_INPUT_READ_)
  {
   if (fdread_line_ok2(source,&linec,&(browsedat->linepos),browsedat->linebuf,
       browsedat->browse_state==search_entry,&(browsedat->lasttab)))
    {
     gui_log_window_insert_string(browsedat->linebuf,1);
     browsedat->linenum++;
/*      g_print("groups: %s\n",browsedat->linebuf); */ 
     switch(browsedat->browse_state) 
     {
      case search_head:
        if(browsedat->firstbrowse &&
           (posi1=strstr(browsedat->linebuf,"Server=[")) &&    /*Works only with smbclient prior 2.0*/
           (posi2=strstr(browsedat->linebuf,"Workgroup=[")))   /*but does not hurt*/
         {
          copybrackstr(browsedat->group_master.master_browser,posi1,MAXMACHNAMEL);
          copybrackstr(browsedat->group_master.group_name,posi2,MAXGROUPNAMEL);
          browsedat->browse_state=search_entryhead;
          break; 
         } 
      case search_entryhead:
        if((posi1=strstr(browsedat->linebuf,"Workgroup")) &&
           (posi2=strstr(browsedat->linebuf,"Master")) && posi2>posi1)
         {
          browsedat->group1pos=posi1-browsedat->linebuf;
          browsedat->master1pos=posi2-browsedat->linebuf;
          browsedat->browse_state=search_dashes;
         }  
        break;
      case search_dashes:
        if(browsedat->linepos>browsedat->master1pos &&
           (!strncmp(browsedat->linebuf+browsedat->group1pos,"---",3))&&
           (!strncmp(browsedat->linebuf+browsedat->master1pos,"---",3)))
         {
          browsedat->browse_state=search_entry;
         }  
         else
         {
          if(browsedat->group_master.group_name[0]==0)
            browsedat->browse_state=search_head;
          else
            browsedat->browse_state=search_entryhead;
         }
        break;
      case search_entry: 
        if(browsedat->linepos>=browsedat->master1pos &&  /*no master allowed*/
#ifdef EXPANDTABS
           !strncmp(browsedat->linebuf,"    ",4))
#elif
           browsedat->linebuf[0]=='\t')
#endif
        {
        groupmasterentry=g_malloc(sizeof(GROUPMASTER_STRUCT));
        copyentrystr(groupmasterentry->group_name,browsedat->linebuf,
                     browsedat->group1pos,browsedat->master1pos-1,MAXGROUPNAMEL);
        copyentrystr(groupmasterentry->master_browser,browsedat->linebuf,
                     browsedat->master1pos,browsedat->linepos-1,MAXMACHNAMEL);
        if(groupmasterentry->group_name[0]==0 /*||
           groupmasterentry->master_browser[0]==0*/)
         {  
          g_free(groupmasterentry);
          linec=EOF; /*finished*/
         }   
         else
         {
          if (!browsedat->firstbrowse && !browsedat->foundsecondentries)
          { /*found masterbrowser has entries, now destroy old ones*/
            browsedat->foundsecondentries=TRUE;
            slist_free_with_data(&(browsedat->groupmasterlist));
          }
          if (!groupmasterentry->master_browser[0])
            strcpy(groupmasterentry->master_browser,nomaster);
          browsedat->groupmasterlist=g_slist_append(browsedat->groupmasterlist,groupmasterentry);
         }
        }
/*        else
         linec=EOF;*/
        break;
     }
     browsedat->linepos=0;
    }
    if (linec==EOF)
    {
/*     g_print("Ready!\n"); */ 
     close(source);
     kill(browsedat->childpid,SIGINT);
     waitpid(browsedat->childpid,NULL,0);
     browsedat->childpid = 0;
     gdk_input_remove(browsedat->input_tag); 
     gmlist=browsedat->groupmasterlist;  
     if(browsedat->firstbrowse)
     {
      if(browsedat->group_master.group_name[0]==0 ||
              browsedat->group_master.master_browser[0]==0)
      {
       browsedat->firstbrowse=FALSE;
      }
      else
      {
       if(gmlist)
       {
        while(gmlist)
        {
         if (strcasecmp(((GROUPMASTER_STRUCT*)(gmlist->data))->group_name,
	                   browsedat->group_master.group_name))
          gmlist=gmlist->next;
         else
         {/* found  wourkgroup of configured masterbrowser, is it really masterbrowser? */
          if(!strcasecmp(((GROUPMASTER_STRUCT*)(gmlist->data))->master_browser,
              browsedat->group_master.master_browser)|| 
             !strcmp(((GROUPMASTER_STRUCT*)(gmlist->data))->master_browser,nomaster))
            browsedat->firstbrowse=FALSE; 
          break;
         }
         if (!gmlist) browsedat->firstbrowse=FALSE; 
        } 
       }else browsedat->firstbrowse=FALSE;
      }
      if(browsedat->firstbrowse)  /*Configured masterbrowser is not masterbrowser */
      {                           /*browse again with found masterbrowser */
       if ( !is_empty_string(pref.v.smbclient_arg) )
       {
         string_ncopy(add_arg_string, pref.v.smbclient_arg, ARG_LEN);
         add_argcount = tokenize_string(cAddArg, add_arg_string, 10);
       }
       browsedat->linepos=0;
       browsedat->linenum=0;
       browsedat->browse_state=search_head;
       browsedat->group_master.group_name[0]=0;
       if((pipe_nok=pipe(browsedat->fd))|| (browsedat->childpid =fork())== -1)   
         {
           if (!pipe_nok)
             {close(browsedat->fd[0]);close(browsedat->fd[1]);}
           g_print(_("GetSMBGroups pipe,fork error\n"));
           browsedat->firstbrowse=FALSE;
           browsedat->linenum=1;
           browsedat->childpid=0;
         } 
       else 
         {
          if (browsedat->childpid ==0)  
          {
           close(browsedat->fd[0]);   
           dup2(browsedat->fd[1],STDOUT_FILENO);
           
           cArg[argcount++] = "smbclient";
           cArg[argcount++] = "-L";
           cArg[argcount++] = ((GROUPMASTER_STRUCT*)(gmlist->data))->master_browser;
           cArg[argcount++] = "-W";
           cArg[argcount++] = ((GROUPMASTER_STRUCT*)(gmlist->data))->group_name;
           cArg[argcount++] = browsedat->userpasswdstr;
           cArg[argcount++] = "-d1";
           if ( (cportkey != NULL) && (cport && NULL) ) {
            cArg[argcount++] = cportkey;
            cArg[argcount++] = cport;
           }
           for ( loop = 0; loop < add_argcount; loop++ )
           {
             cArg[argcount++] = cAddArg[loop];
           }
           cArg[argcount++] = NULL;
           
           execvp(pref_get_smbclient_exe(), cArg);
         
           /*execlp(pref_get_smbclient_exe(),
                  "smbclient",
                  "-L",((GROUPMASTER_STRUCT*)(gmlist->data))->master_browser,
                  "-W",((GROUPMASTER_STRUCT*)(gmlist->data))->group_name,
                  browsedat->userpasswdstr,
                  "-d1",
                  cportkey,
                  cport,
                  NULL);*/
           _exit(0);
          }
          sprintf(logstr,"GetSMBGroups2:%s -L %s -W %s %s -d1",pref_get_smbclient_exe(),
                    ((GROUPMASTER_STRUCT*)(gmlist->data))->master_browser,
                    ((GROUPMASTER_STRUCT*)(gmlist->data))->group_name,browsedat->userdummystr);
          if ( (cport != NULL) && (cportkey != NULL) )
          {
            string_ncat(logstr, " ", MAXLOGSTRLEN);
            string_ncat(logstr, cportkey, MAXLOGSTRLEN);
            string_ncat(logstr, " ", MAXLOGSTRLEN);
            string_ncat(logstr, cport, MAXLOGSTRLEN);
          }
          for ( loop = 0; loop < add_argcount; loop++ )
          {
            string_ncat(logstr, " ", MAXLOGSTRLEN);
            string_ncat(logstr, cAddArg[loop], MAXLOGSTRLEN);
          }
          gui_log_window_insert_string(logstr,1);
          close(browsedat->fd[1]);    
          browsedat->lasttab=0;
          browsedat->input_tag = gdk_input_add(browsedat->fd[0], GDK_INPUT_READ_, 
	 		    (GdkInputFunction)GetSMBGroups_inputcallback, browsedat);
         }                   
      }
     } 
     if(!browsedat->firstbrowse)/*No second browsing*/
     { 
      gmlist=browsedat->groupmasterlist;  
      if (gmlist || browsedat->browse_state>search_entryhead) /*always ok if we have entries,*/
        smbstate=SMB_STATE_OK;         /*even second browsing was not successful*/
      else if(browsedat->linenum==0)
        smbstate=SMB_STATE_EXEERR;
      else 
        smbstate=SMB_STATE_SMBERR;
      GetSMBGroups_done(gmlist,browsedat->browse_info,smbstate);
      slist_free_with_data(&(browsedat->groupmasterlist));
      gbrowsedatlist=g_slist_remove(gbrowsedatlist,browsedat);
      g_free(browsedat);
     } 
     else browsedat->firstbrowse=FALSE;  
    }
  }
} 


static void GetSMBGroups_nmbinputcallback(GBROWSE_STRUCT *browsedat,gint source, GdkInputCondition cond)
{
  int   linec,pipe_nok,found_master=FALSE;
  char master_browser[MAXMACHNAMEL+1];
  GetSMBStateType smbstate;
  int nodetype,isgroup,isactive;
  char port_str[16];
  char *cport = NULL;
  char *cportkey = NULL;
  char *cArg[20];
  char *cAddArg[10];
  int argcount = 0;
  char add_arg_string[ARG_LEN+1];
  int add_argcount = 0;
  int loop;
   
  if ( (browsedat->port >= 0) && (browsedat->port < 65536) )
  {
    cportkey = "-p";
    sprintf(port_str, "%d", browsedat->port);
    cport = port_str;
  }
  
  if(cond & GDK_INPUT_READ_)
  {
   if (fdread_line_ok2(source,&linec,&(browsedat->linepos),browsedat->linebuf,0,NULL))
   {
     browsedat->linenum++;
     gui_log_window_insert_string(browsedat->linebuf,1);
     switch(browsedat->browse_state) 
     {
      case search_head:
        if(strstr(browsedat->linebuf,"Looking") && strstr(browsedat->linebuf,"status"))
         {
          browsedat->browse_state=search_entry;
         } 
        break; 
      case search_entry:
        if (getnodename(master_browser,browsedat->linebuf,&nodetype,&isgroup,&isactive,MAXMACHNAMEL))
         {
          if (nodetype==0 && !isgroup && isactive)
          {
           found_master=TRUE;
           linec=EOF;
          }
         }
      default:;   
     }
     browsedat->linepos=0;
   }
   if (linec==EOF)
   {
     close(source);
     kill(browsedat->childpid,SIGINT);
     waitpid(browsedat->childpid,NULL,0);
     browsedat->childpid = 0;
     gdk_input_remove(browsedat->input_tag); 
     if (found_master)
     {
       if ( !is_empty_string(pref.v.smbclient_arg) )
       {
         string_ncopy(add_arg_string, pref.v.smbclient_arg, ARG_LEN);
         add_argcount = tokenize_string(cAddArg, add_arg_string, 10);
       }
       if((pipe_nok=pipe(browsedat->fd))|| (browsedat->childpid =fork())== -1)   
         {
           if (!pipe_nok)
             {close(browsedat->fd[0]);close(browsedat->fd[1]);}
           g_print(_("GetSMBGroups pipe,fork error\n"));
           browsedat->childpid=0;
           return;
         } 
       else 
         {
          if (browsedat->childpid == 0)  
          {
           close(browsedat->fd[0]);   
           dup2(browsedat->fd[1],STDOUT_FILENO); 
           
           cArg[argcount++] = "smbclient";
           cArg[argcount++] = "-L";
           cArg[argcount++] = master_browser;
           cArg[argcount++] = "-W";
           cArg[argcount++] = browsedat->group_master.group_name;
           cArg[argcount++] = browsedat->userpasswdstr;
           cArg[argcount++] = "-d1";
           if ( (cportkey != NULL) && (cport && NULL) ) {
            cArg[argcount++] = cportkey;
            cArg[argcount++] = cport;
           }
           for ( loop = 0; loop < add_argcount; loop++ )
           {
             cArg[argcount++] = cAddArg[loop];
           }
           cArg[argcount] = NULL;
           
           execvp(pref_get_smbclient_exe(), cArg);
           
           /*execlp(pref_get_smbclient_exe(),
                  "smbclient",
                  "-L",master_browser,
                  "-W",browsedat->group_master.group_name,browsedat->userpasswdstr,
                  "-d1",
                  cportkey,
                  cport,
                  NULL);*/
           _exit(0);
          }
          sprintf(logstr,"GetSMBGroups:%s -L %s -W %s %s -d1",pref_get_smbclient_exe(),
                    master_browser,browsedat->group_master.group_name,browsedat->userdummystr);
          if ( (cport != NULL) && (cportkey != NULL) )
          {
            string_ncat(logstr, " ", MAXLOGSTRLEN);
            string_ncat(logstr, cportkey, MAXLOGSTRLEN);
            string_ncat(logstr, " ", MAXLOGSTRLEN);
            string_ncat(logstr, cport, MAXLOGSTRLEN);
          }
          for ( loop = 0; loop < add_argcount; loop++ )
          {
            string_ncat(logstr, " ", MAXLOGSTRLEN);
            string_ncat(logstr, cAddArg[loop], MAXLOGSTRLEN);
          }
          gui_log_window_insert_string(logstr,1);
          browsedat->linepos=0;
          browsedat->linenum=0;
          browsedat->browse_state=search_head;
          browsedat->group_master.group_name[0]=0;
          browsedat->firstbrowse=FALSE;
          browsedat->foundsecondentries=FALSE;
          close(browsedat->fd[1]);
          browsedat->lasttab=0;
          browsedat->input_tag = gdk_input_add(browsedat->fd[0],GDK_INPUT_READ_, 
	 		    (GdkInputFunction)GetSMBGroups_inputcallback, browsedat);
         }
     }
     else
     {
      if (browsedat->linenum==0)
        smbstate=SMB_STATE_EXEERR;
      else  
        smbstate=SMB_STATE_SMBERR;
      GetSMBGroups_done(NULL,browsedat->browse_info,smbstate);
      gbrowsedatlist=g_slist_remove(gbrowsedatlist,browsedat);
      g_free(browsedat);
     }  
   }
  } 
}
 

void GetSMBGroups ( const char * group_name, const char * master_browser, const char * user, const char * passwd, const char * wins_name,char first_master,int port, GROUPS_BROWSE_INFO *browse_info)
{
  GBROWSE_STRUCT *browsedat;
  int pipe_nok;
  char logstr[MAXLOGSTRLEN+1];
  char group_type[MAXGROUPNAMEL+1+3];
  char port_str[16];
  char *cport = NULL;
  char *cportkey = NULL;
  const char *cArg[20];
  char *cAddArg[10];
  int argcount = 0;
  char add_arg_string[ARG_LEN+1];
  int add_argcount = 0;
  int loop;

  browsedat=g_malloc(sizeof(GBROWSE_STRUCT));
  browsedat->groupmasterlist=NULL;
  browsedat->masterbrowser[MAXSMBLINEL]=0;
  strncpy(browsedat->masterbrowser,master_browser,MAXSMBLINEL);
  browsedat->port = port;
  browsedat->linepos=0;
  browsedat->linenum=0;
  browsedat->browse_state=search_head;
  browsedat->lasttab=0;
  if (*user==0){    
    strcpy(browsedat->userpasswdstr,"-U%"); 
    strcpy(browsedat->userdummystr,"-U%"); 
  }
  else
  {
    sprintf(browsedat->userpasswdstr,"-U%s%%%s",user,passwd);
    strcpy(browsedat->userdummystr,browsedat->userpasswdstr); 
    memset(browsedat->userdummystr+strlen(browsedat->userdummystr)-strlen(passwd),'*',strlen(passwd));
  }
  if ( (port >= 0) && (port < 65536) )
  {
    cportkey = "-p";
    sprintf(port_str, "%d", port);
    cport = port_str;
  }
  if ( !is_empty_string(pref.v.smbclient_arg) )
  {
    string_ncopy(add_arg_string, pref.v.smbclient_arg, ARG_LEN);
    add_argcount = tokenize_string(cAddArg, add_arg_string, 10);
  }
  if((pipe_nok=pipe(browsedat->fd))|| (browsedat->childpid =fork())== -1)   
    {
      if (!pipe_nok)
        {close(browsedat->fd[0]);close(browsedat->fd[1]);}
      g_free(browsedat);
      g_print(_("GetSMBGroups pipe,fork error\n"));
      return;
    } 
  if (*master_browser) /*use configured masterbrowser if not nullstring*/
  {
    if (!browsedat->childpid)  
    {
     close(browsedat->fd[0]);   
     dup2(browsedat->fd[1],STDOUT_FILENO); 
     
     cArg[argcount++] = "smbclient";
     cArg[argcount++] = "-L";
     cArg[argcount++] = master_browser;
     cArg[argcount++] = "-W";
     cArg[argcount++] = group_name;
     cArg[argcount++] = browsedat->userpasswdstr;
     cArg[argcount++] = "-d1";
     if ( (cportkey != NULL) && (cport && NULL) ) {
      cArg[argcount++] = cportkey;
      cArg[argcount++] = cport;
     }
     for ( loop = 0; loop < add_argcount; loop++ )
     {
       cArg[argcount++] = cAddArg[loop];
     }
     cArg[argcount] = NULL;
     
     execvp(pref_get_smbclient_exe(), (char **)cArg);
     
     /*execlp(pref_get_smbclient_exe(),
            "smbclient",
            "-L",master_browser,
            "-W",group_name,browsedat->userpasswdstr,
            "-d1",
            cportkey,
            cport,
            NULL);*/
     _exit(0);
    }
    else
    {
     sprintf(logstr,"GetSMBGroups:%s -L %s -W %s %s -d1",pref_get_smbclient_exe(),master_browser,group_name,browsedat->userdummystr);
     if ( (cport != NULL) && (cportkey != NULL) )
     {
       string_ncat(logstr, " ", MAXLOGSTRLEN);
       string_ncat(logstr, cportkey, MAXLOGSTRLEN);
       string_ncat(logstr, " ", MAXLOGSTRLEN);
       string_ncat(logstr, cport, MAXLOGSTRLEN);
     }
     for ( loop = 0; loop < add_argcount; loop++ )
     {
       string_ncat(logstr, " ", MAXLOGSTRLEN);
       string_ncat(logstr, cAddArg[loop], MAXLOGSTRLEN);
     }
     gui_log_window_insert_string(logstr,1);
     close(browsedat->fd[1]);   
     browsedat->firstbrowse=first_master;/*TRUE*/
     browsedat->foundsecondentries=FALSE;
     browsedat->group_master.group_name[MAXGROUPNAMEL]=0;
     strncpy(browsedat->group_master.group_name,group_name,MAXGROUPNAMEL);
     browsedat->group_master.master_browser[MAXMACHNAMEL]=0;
     strncpy(browsedat->group_master.master_browser,master_browser,MAXMACHNAMEL);
     browsedat->browse_info =browse_info;
     browsedat->input_tag = gdk_input_add(browsedat->fd[0], GDK_INPUT_READ_, 
			      (GdkInputFunction)GetSMBGroups_inputcallback, browsedat);
    }
  }                          
  else
  {
    if (*wins_name) {
      group_type[MAXGROUPNAMEL]=0;
      strncpy(group_type,group_name,MAXGROUPNAMEL);
      strcat(group_type,"#1b"); /*#1b= Domain Masterbrowser*/
                                /*#1d= Local Masterbrowser*/
    }  
       
    if ( !is_empty_string(pref.v.nmblookup_arg) )
    {
      string_ncopy(add_arg_string, pref.v.nmblookup_arg, ARG_LEN);
      add_argcount = tokenize_string(cAddArg, add_arg_string, 10);
    }
     
    if (!browsedat->childpid)
    {
     close(browsedat->fd[0]);   
     dup2(browsedat->fd[1],STDOUT_FILENO);
     
     cArg[argcount++] = "nmblookup";
     cArg[argcount++] = "-S";
     cArg[argcount++] = "-d1";
     if ( *wins_name )
     {
       cArg[argcount++] = "-U";
       cArg[argcount++] = wins_name;
       cArg[argcount++] = "-R";
       cArg[argcount++] = group_type;
     }
     else
     {
       cArg[argcount++] = "-M";

       if (strcmp(group_name, "-") == 0)
         cArg[argcount++] = "--";
       cArg[argcount++] = group_name;
     }
     for ( loop = 0; loop < add_argcount; loop++ )
     {
       cArg[argcount++] = cAddArg[loop];
     }
     cArg[argcount++] = NULL;

     execvp(pref_get_nmblookup_exe(), (char **)cArg);
     
     /*if (*wins_name)
       execlp(pref_get_nmblookup_exe(),
              "nmblookup",
              "-U",wins_name,
              "-R",group_type,
              "-S",
              "-d1",
              NULL);
      else
       execlp(pref_get_nmblookup_exe(),
              "nmblookup",
              "-M",group_name,
              "-S",
              "-d1",
              NULL);*/
     _exit(0);
    }
    else
    {
     if (*wins_name)
       sprintf(logstr,"GetSMBGroups:%s -U %s -R %s -S -d1",pref_get_nmblookup_exe(),wins_name,group_type);
     else
       sprintf(logstr,"GetSMBGroups:%s -M %s -S -d1",pref_get_nmblookup_exe(),group_name);
     for ( loop = 0; loop < add_argcount; loop++ )
     {
       string_ncat(logstr, " ", MAXLOGSTRLEN);
       string_ncat(logstr, cAddArg[loop], MAXLOGSTRLEN);
     }
     gui_log_window_insert_string(logstr,1);
     close(browsedat->fd[1]);   
     browsedat->group_master.group_name[MAXGROUPNAMEL]=0;
     strncpy(browsedat->group_master.group_name,group_name,MAXGROUPNAMEL);
     browsedat->browse_info=browse_info;
     browsedat->input_tag = gdk_input_add(browsedat->fd[0], GDK_INPUT_READ_, 
			      (GdkInputFunction)GetSMBGroups_nmbinputcallback, browsedat);
    }
  }
}



static void GetSMBMachines_inputcallback(HBROWSE_STRUCT *browsedat,gint source, GdkInputCondition cond)
{
  int  linec;
  char *posi1,*posi2;
  MACHINECOMMENT_STRUCT *machine_comment;
  GSList *machinecommentlist;
  GetSMBStateType smbstate;
  char headergroupname[MAXGROUPNAMEL+1];
  
  if(cond & GDK_INPUT_READ_)
  {
   if (fdread_line_ok2(source,&linec,&(browsedat->linepos),browsedat->linebuf,
       browsedat->browse_state==search_entry,&(browsedat->lasttab)))
    {
     gui_log_window_insert_string(browsedat->linebuf,1);
     browsedat->linenum++;
      g_print("machines: %s\n",browsedat->linebuf); 
     switch(browsedat->browse_state) 
     {
      case search_head:
        if((posi1=strstr(browsedat->linebuf,"Domain=[")))
         {
          copybrackstr(headergroupname,posi1,MAXGROUPNAMEL);
          browsedat->browse_state=search_entryhead;
          if (strcasecmp(headergroupname,browsedat->group_master.group_name))
             linec=EOF;
          break; 
         } 
      case search_entryhead:
        if((posi1=strstr(browsedat->linebuf,"Server")) &&
           (posi2=strstr(browsedat->linebuf,"Comment")) && posi2>posi1)
         {
          browsedat->machine1pos=posi1-browsedat->linebuf;
          browsedat->comment1pos=posi2-browsedat->linebuf;
          browsedat->browse_state=search_dashes;
         }  
        break;
      case search_dashes:
        if(browsedat->linepos>browsedat->comment1pos &&
           (!strncmp(browsedat->linebuf+browsedat->machine1pos,"---",3))&&
           (!strncmp(browsedat->linebuf+browsedat->comment1pos,"---",3)))
          browsedat->browse_state=search_entry;
         else
          browsedat->browse_state=search_head;
        break;
      case search_entry: 
        if(browsedat->linepos>browsedat->machine1pos)
        {
        machine_comment=g_malloc(sizeof(MACHINECOMMENT_STRUCT));
        copyentrystr(machine_comment->entry_name,browsedat->linebuf,
                     browsedat->machine1pos,browsedat->comment1pos-1,MAXMACHNAMEL);
        copyentrystr(machine_comment->entry_comment,browsedat->linebuf,
                     browsedat->comment1pos,browsedat->linepos-1,MAXCOMMENTL);
                     
        if(machine_comment->entry_name[0]==0)
         {  
          g_free(machine_comment);
          linec=EOF;
         }   
         else
         {
          browsedat->machinecommentlist=g_slist_append(browsedat->machinecommentlist,
                                                                   machine_comment);
         }
        }
        else
         linec=EOF;
        break;
     }
     browsedat->linepos=0;
    }
    if (linec==EOF)
    {
/*     g_print("Ready!\n"); */ 
     close(source);
     kill(browsedat->childpid,SIGINT);
     waitpid(browsedat->childpid,NULL,0);
     gdk_input_remove(browsedat->input_tag); 
     if(browsedat->linenum==0)
       smbstate=SMB_STATE_EXEERR;
     else if (browsedat->browse_state==search_head)
       smbstate=SMB_STATE_SMBERR;
     else
       smbstate=SMB_STATE_OK;
     if (!browsedat->machinecommentlist && browsedat->browse_state>=search_dashes)
     {  /* if empty browse list add at least masterbrowser to list*/
        machine_comment=g_malloc(sizeof(MACHINECOMMENT_STRUCT));
        strcpy(machine_comment->entry_name,browsedat->group_master.master_browser);
        strcpy(machine_comment->entry_comment,"(master without browse list)"); /*Tell it to user*/
        browsedat->machinecommentlist=g_slist_append(browsedat->machinecommentlist,
                                                                   machine_comment);
     }
     machinecommentlist=browsedat->machinecommentlist;
     GetSMBMachines_done(browsedat->group_master.group_name,machinecommentlist,browsedat->browse_info,smbstate);
     slist_free_with_data(&(browsedat->machinecommentlist));
     hbrowsedatlist=g_slist_remove(hbrowsedatlist,browsedat);
     g_free(browsedat);
    }
  }
}



void GetSMBMachines (const char * group_name, const char *master_browser, const char * user, const char * passwd, const char *domain,int port,MACHINES_BROWSE_INFO *browse_info)
{
  HBROWSE_STRUCT *browsedat;
  int pipe_nok;
  char logstr[MAXLOGSTRLEN+1];
  char commandstr[MAXCOMMANDL+1];
  char port_str[16];
  char *cport = NULL;
  char *cportkey = NULL;
  const char *cArg[20];
  char *cAddArg[10];
  int argcount = 0;
  char add_arg_string[ARG_LEN+1];
  int add_argcount = 0;
  int loop;

  if ( (port >= 0) && (port < 65536) )
  {
    cportkey = "-p";
    sprintf(port_str, "%d", port);
    cport = port_str;
  }
  
  if (!strcmp(master_browser,nomaster))
  {
     GetSMBMachines_done(group_name,NULL,browse_info,SMB_STATE_SMBERR);

     sprintf(logstr,"GetSMBMachines: No Master for Group %s",group_name);
     gui_log_window_insert_string(logstr,1);
     return;
  }
  browsedat=g_malloc(sizeof(HBROWSE_STRUCT));
  if (*user==0)
    strcpy(commandstr,"-U%"); 
  else
    sprintf(commandstr,"-U%s%%%s",user,passwd);
   
  if ( !is_empty_string(pref.v.smbclient_arg) )
  {
    string_ncopy(add_arg_string, pref.v.smbclient_arg, ARG_LEN);
    add_argcount = tokenize_string(cAddArg, add_arg_string, 10);
  }
   
  if((pipe_nok=pipe(browsedat->fd))|| (browsedat->childpid =fork())== -1)   
    {
      if (!pipe_nok)
        {close(browsedat->fd[0]);close(browsedat->fd[1]);}
      g_free(browsedat);
      g_print(_("GetSMBMachines pipe,fork error\n"));
      return;
    } 
  if (!browsedat->childpid)
  {
   close(browsedat->fd[0]);   
   dup2(browsedat->fd[1],STDOUT_FILENO); 
   
   cArg[argcount++] = "smbclient";
   cArg[argcount++] = "-L";
   cArg[argcount++] = master_browser;
   cArg[argcount++] = commandstr;
   if (*domain) {
    cArg[argcount++] = "-W";
    cArg[argcount++] = domain;
   }
   cArg[argcount++] = "-d1";
   if ( (cportkey != NULL) && (cport && NULL) ) {
    cArg[argcount++] = cportkey;
    cArg[argcount++] = cport;
   }
   for ( loop = 0; loop < add_argcount; loop++ )
   {
     cArg[argcount++] = cAddArg[loop];
   }
   
   cArg[argcount] = NULL;
     
   execvp(pref_get_smbclient_exe(), (char **)cArg);
   _exit(0);
  }
  else
  {
   if (*user!=0) memset(commandstr+strlen(commandstr)-strlen(passwd),'*',strlen(passwd));
   if(*domain)
     sprintf(logstr,"GetSMBMachines:%s -L %s %s -W %s -d1",pref_get_smbclient_exe(),master_browser,commandstr,domain);
   else
     sprintf(logstr,"GetSMBMachines:%s -L %s %s -d1",pref_get_smbclient_exe(),master_browser,commandstr);
   if ( (cport != NULL) && (cportkey != NULL) )
   {
     string_ncat(logstr, " ", MAXLOGSTRLEN);
     string_ncat(logstr, cportkey, MAXLOGSTRLEN);
     string_ncat(logstr, " ", MAXLOGSTRLEN);
     string_ncat(logstr, cport, MAXLOGSTRLEN);
   }
   for ( loop = 0; loop < add_argcount; loop++ )
   {
     string_ncat(logstr, " ", MAXLOGSTRLEN);
     string_ncat(logstr, cAddArg[loop], MAXLOGSTRLEN);
   }
   gui_log_window_insert_string(logstr,1);
   close(browsedat->fd[1]);   
   hbrowsedatlist=g_slist_append(hbrowsedatlist,browsedat);
   browsedat->linepos=0;
   browsedat->linenum=0;
   browsedat->browse_state=search_head;
/*   strcpy(browsedat->group_name,group_name); */
   strcpy(browsedat->group_master.master_browser,master_browser);
   strcpy(browsedat->group_master.group_name,group_name);
   browsedat->port = port;
   browsedat->browse_info=browse_info;
   browsedat->machinecommentlist=NULL;
   browsedat->lasttab=0;
   browsedat->input_tag = gdk_input_add(browsedat->fd[0], GDK_INPUT_READ_, 
			    (GdkInputFunction)GetSMBMachines_inputcallback, browsedat);
  }                          
}

gint CompareSHARECOMMENT(SHARECOMMENTTYPE_STRUCT *a, SHARECOMMENTTYPE_STRUCT *b)
{
 if (a->share_type==b->share_type)
   return(strcasecmp(a->entry_name,b->entry_name)>0);
 else
   return(a->share_type>b->share_type);
}


static void GetSMBShares_inputcallback(SBROWSE_STRUCT *browsedat,gint source, GdkInputCondition cond)
{
  int  linec;
  char *posi1,*posi2,*posi3;
  char connectstr[]="Connecting to";
  SHARECOMMENTTYPE_STRUCT  *sharecommenttype;
  GSList *sharecommenttypelist;
  char share_str[5];
  GetSMBStateType smbstate;
  int shtypeok;
  
  if(cond & GDK_INPUT_READ_)
  {
   if (fdread_line_ok2(source,&linec,&(browsedat->linepos),browsedat->linebuf,
                       browsedat->browse_state==search_entry,&(browsedat->lasttab)))
    {
     gui_log_window_insert_string(browsedat->linebuf,1);
     browsedat->linenum++;
/*      g_print("shares: %s\n",browsedat->linebuf); */
     switch(browsedat->browse_state) 
     {
      case search_head:
      case search_entryhead:
        if (strstr(browsedat->linebuf,connectstr))
        {
         posi1=browsedat->linebuf+sizeof(connectstr)-1;
         while (*posi1==' ' && *posi1) posi1++;/*find first number*/
         posi2=posi1;
         while (*posi2!=' ' && *posi2) posi2++;/*find last number*/
         if (posi2-1>posi1)
         { 
          copyentrystr(browsedat->machineinfo.ipaddr,
                       browsedat->linebuf,
                       posi1-browsedat->linebuf,
                       posi2-browsedat->linebuf,
                       MAXMACHNAMEL);
         }
/*         browsedat->browse_state=search_entryhead;  */
         break;
        }
         /*Get some Machineinfo*/
        if((posi1=strstr(browsedat->linebuf,"Domain=[")) &&
           (posi2=strstr(browsedat->linebuf,"OS=[")) &&
           (posi3=strstr(browsedat->linebuf,"Server=[")))
         {
          copybrackstr(browsedat->machineinfo.Domain,posi1,MAXGROUPNAMEL);
          copybrackstr(browsedat->machineinfo.OS,posi2,MAXCOMMENTL);
          copybrackstr(browsedat->machineinfo.Server,posi3,MAXCOMMENTL);
          break; 
         } 
        if((posi1=strstr(browsedat->linebuf,"Sharename")) &&
           (posi2=strstr(browsedat->linebuf,"Type")) &&
           (posi3=strstr(browsedat->linebuf,"Comment")) &&
            posi3>posi2 && posi2>posi1)
         {
          browsedat->share1pos=posi1-browsedat->linebuf;
          browsedat->shtype1pos=posi2-browsedat->linebuf;
          browsedat->comment1pos=posi3-browsedat->linebuf;
          browsedat->browse_state=search_dashes;
         }  
        break;
      case search_dashes:
        if(browsedat->linepos>browsedat->comment1pos &&
           (!strncmp(browsedat->linebuf+browsedat->share1pos,"---",3))&&
           (!strncmp(browsedat->linebuf+browsedat->shtype1pos,"---",3))&&
           (!strncmp(browsedat->linebuf+browsedat->comment1pos,"---",3)))
          browsedat->browse_state=search_entry;
         else
          browsedat->browse_state=search_head;
        break;
      case search_entry: 
        if(browsedat->linepos>browsedat->shtype1pos)
        {
         sharecommenttype=g_malloc(sizeof(SHARECOMMENTTYPE_STRUCT));
        copyentrystr(sharecommenttype->entry_name,browsedat->linebuf,
                     browsedat->share1pos,browsedat->shtype1pos-1,MAXSHRNAMEL);
        copyentrystr(share_str,browsedat->linebuf,browsedat->shtype1pos,
                 browsedat->shtype1pos+sizeof(share_str)-2,sizeof(share_str)-1);
        copyentrystr(sharecommenttype->entry_comment,browsedat->linebuf,
                     browsedat->comment1pos,browsedat->linepos-1,MAXCOMMENTL);
        if(sharecommenttype->entry_name[0]==0)
         {  
          g_free(sharecommenttype);
          linec=EOF; /*may be finished*/
         }   
         else
         {
          shtypeok=TRUE;
          if (!strcmp(share_str,"Disk"))
            sharecommenttype->share_type=shtype_folder;
          else if (!strcmp(share_str,"Prin"))
            sharecommenttype->share_type=shtype_printer;
          else  {g_free(sharecommenttype);shtypeok=FALSE;}
          /*is IPC needed?*/  
          if(shtypeok){
            if (sorted_sharelist)
              browsedat->sharecommenttypelist=
               g_slist_insert_sorted(browsedat->sharecommenttypelist,sharecommenttype,
                 (GCompareFunc)CompareSHARECOMMENT);
            else            
              browsedat->sharecommenttypelist=
                g_slist_append(browsedat->sharecommenttypelist,sharecommenttype);
           }
         }
        }
/*        else
         linec=EOF;*/
        break;
     }
     browsedat->linepos=0;
    }
    if (linec==EOF)
    {
/*     g_print("Ready!\n"); */ 
     close(source);
     kill(browsedat->childpid,SIGINT);
     waitpid(browsedat->childpid,NULL,0);
     gdk_input_remove(browsedat->input_tag);
     if(browsedat->linenum==0)
       smbstate=SMB_STATE_EXEERR;
     else if (browsedat->browse_state==search_head)
       smbstate=SMB_STATE_SMBERR;
     else
       smbstate=SMB_STATE_OK;
     sharecommenttypelist=browsedat->sharecommenttypelist;
     GetSMBShares_done(browsedat->group_name,browsedat->machine_name,
                browsedat->user, browsedat->password, &(browsedat->machineinfo),
              	sharecommenttypelist,browsedat->browse_info,smbstate);
     slist_free_with_data(&(browsedat->sharecommenttypelist));
/*     GetSMBShare_done(browsedat->group_name,browsedat->machine_name,smbstate); */
     sbrowsedatlist=g_slist_remove(sbrowsedatlist,browsedat);
     g_free(browsedat);
    }
  }
}


void GetSMBShares (const char *group_name,const char *machine_name,const char *domain,const char *ipaddr,const char *user,const char *passwd,int port,SHARES_BROWSE_INFO *browse_info )
{
  char commandstr[MAXCOMMANDL+1];
  SBROWSE_STRUCT *browsedat;
  int pipe_nok;
  int argnr=-1;
  const char *argv[22];
  char port_str[16];
  char *cAddArg[10];
  char add_arg_string[ARG_LEN+1];
  int add_argcount = 0;
  int loop;

  if ( !is_empty_string(pref.v.smbclient_arg) )
  {
    string_ncopy(add_arg_string, pref.v.smbclient_arg, ARG_LEN);
    add_argcount = tokenize_string(cAddArg, add_arg_string, 10);
  }
  
  if (*user==0)
    strcpy(commandstr,"-U%"); 
  else
    sprintf(commandstr,"-U%s%%%s",user,passwd);
  argv[++argnr]="smbclient";
  argv[++argnr]="-L";
  argv[++argnr]=machine_name;
  if (*ipaddr)
  {
    argv[++argnr]="-I";
    argv[++argnr]=ipaddr;
  }
  argv[++argnr]=commandstr;
  if (*domain)
  { 
    argv[++argnr]="-W";
    argv[++argnr]=domain;
  }
  argv[++argnr]="-d3";/*We want "Connecting to" message*/
  if ( (port >= 0) && (port < 65536) )
  {
    argv[++argnr] = "-p";
    sprintf(port_str, "%d", port);
    argv[++argnr] = port_str;
  }
  for ( loop = 0; loop < add_argcount; loop++ )
  {
    argv[++argnr] = cAddArg[loop];
  }
  argv[++argnr]=NULL;
  browsedat=g_malloc(sizeof(SBROWSE_STRUCT));
  if((pipe_nok=pipe(browsedat->fd))|| (browsedat->childpid =fork())== -1)   
    {
      if (!pipe_nok)
        {close(browsedat->fd[0]);close(browsedat->fd[1]);}
      g_free(browsedat);
      g_print(_("GetSMBShare pipe,fork error\n"));
      return;
    } 
  if (!browsedat->childpid)
  {
   close(browsedat->fd[0]);   
   dup2(browsedat->fd[1],STDOUT_FILENO); 
   execvp(pref_get_smbclient_exe(),(char **)argv);
   _exit(0);
  }
  else
  {
  if (*user!=0) memset(commandstr+strlen(commandstr)-strlen(passwd),'*',strlen(passwd));
  gui_log_window_insert_string("GetSMBShare:",0);
  log_execvp_str(pref_get_smbclient_exe(), (char **)argv);
  close(browsedat->fd[1]);   
  sbrowsedatlist=g_slist_append(sbrowsedatlist,browsedat);
  browsedat->linepos=0;
  browsedat->linenum=0;
  browsedat->browse_state=search_head;
  strcpy(browsedat->machine_name,machine_name);
  strcpy(browsedat->group_name,group_name);
  strncpy(browsedat->user, user, USER_LEN);
  strncpy(browsedat->password, passwd, PASSWORD_LEN);
  browsedat->port = port;
  browsedat->browse_info=browse_info;
  browsedat->sharecommenttypelist=NULL;
  browsedat->machineinfo.ipaddr[0]=0;
  browsedat->machineinfo.Domain[0]=0;
  browsedat->machineinfo.OS[0]=0;
  browsedat->machineinfo.Server[0]=0;
  browsedat->lasttab=0;
  browsedat->input_tag = gdk_input_add(browsedat->fd[0],GDK_INPUT_READ_, 
			    (GdkInputFunction)GetSMBShares_inputcallback, browsedat);
  }                          
}


static void LookupByName_inputcallback(LOOKUP_STRUCT *lookupdat,gint source, GdkInputCondition cond)
{
  GetSMBStateType smbstate;
  int   linec;
  int nodetype,isgroup,isactive;
  char *posi1,*posi2;
  char connectstr[]="Looking up status of";
  if(cond & GDK_INPUT_READ_)
  {
   if (fdread_line_ok2(source,&linec,&(lookupdat->linepos),lookupdat->linebuf,0,NULL))
   {
     lookupdat->linenum++;
     gui_log_window_insert_string(lookupdat->linebuf,1);
     switch(lookupdat->browse_state) 
     {
      case search_head:
        if (strstr(lookupdat->linebuf,connectstr))
        {
         posi1=lookupdat->linebuf+sizeof(connectstr)-1;
         while (*posi1==' ' && *posi1) posi1++;/*find first number*/
         posi2=posi1;
         while (*posi2!=' ' && *posi2) posi2++;/*find last number*/
         if (posi2-1>posi1)
         { 
          copyentrystr(lookupdat->ipaddr,lookupdat->linebuf,posi1-lookupdat->linebuf,
                       posi2-lookupdat->linebuf,MAXMACHNAMEL);
         }
         lookupdat->browse_state=search_entry;//search_entryhead; 
        }
        break;
/*      case search_entryhead:
        if(strstr(lookupdat->linebuf,"received") && strstr(lookupdat->linebuf,"names"))
         {
          lookupdat->browse_state=search_entry;
         } 
        break; */
      case search_entry: /*Get Group*/
         {
          if (getnodename(lookupdat->group_name,lookupdat->linebuf,&nodetype,&isgroup,&isactive,MAXGROUPNAMEL))
          {
            if(nodetype==0 && isgroup &&isactive && !group_ignored(lookupdat->group_name))
              linec=EOF;
            else 
              lookupdat->group_name[0]=0;
          }
/*          else linec=EOF;*/
         }
      default:;   
     }
     lookupdat->linepos=0;
   }
   if (linec==EOF){
     close(source);
     kill(lookupdat->childpid,SIGINT);
     waitpid(lookupdat->childpid,NULL,0);
     gdk_input_remove(lookupdat->input_tag);
     if(lookupdat->linenum==0)
       smbstate=SMB_STATE_EXEERR;
     else if (!lookupdat->group_name[0])
       smbstate=SMB_STATE_SMBERR;
     else
       smbstate=SMB_STATE_OK;
     LookupByName_done (lookupdat->machine_name,lookupdat->group_name,lookupdat->ipaddr,smbstate);
     lookupbynamelist=g_slist_remove(lookupbynamelist,lookupdat);
     g_free(lookupdat);
   }
  }
}



void LookupByName ( const char * machine_name, const char * wins_name)
{
LOOKUP_STRUCT *lookupdat;
GSList *lookuplist;
int pipe_nok;
int fd[2];
char logstr[MAXLOGSTRLEN+1];
const char *cArg[20];
char *cAddArg[10];
int argcount = 0;
char add_arg_string[ARG_LEN+1];
int add_argcount = 0;
int loop;
/*  g_print("LookupByName(%s,%s)\n",machine_name, wins_name); */

  if (lookupbynamelist)
  {
    lookuplist= lookupbynamelist;
    while(lookuplist)
    {
     if(strcmp(((LOOKUP_STRUCT*)(lookuplist->data))->machine_name,machine_name))
      {     
       lookuplist=lookuplist->next;
      } 
     else
     {/*if LookupByName with same ip is running, stop it */
      lookupdat=lookuplist->data;
      close(lookupdat->fd);
      usleep(50000);
      kill(lookupdat->childpid,SIGINT);
     waitpid(lookupdat->childpid,NULL,0);
      gdk_input_remove(lookupdat->input_tag); 
      lookupbynamelist=g_slist_remove(lookupbynamelist,lookupdat);
      g_free(lookupdat);
      break;
     }  
    }
  }
  lookupdat=g_malloc(sizeof(LOOKUP_STRUCT));
  
  if ( !is_empty_string(pref.v.nmblookup_arg) )
  {
    string_ncopy(add_arg_string, pref.v.nmblookup_arg, ARG_LEN);
    add_argcount = tokenize_string(cAddArg, add_arg_string, 10);
  }
  
  if((pipe_nok=pipe(fd))|| (lookupdat->childpid =fork())== -1)   
    {
      if (!pipe_nok)
        {close(fd[0]);close(fd[1]);}
      g_free(lookupdat);
      g_print(_("LookupByName pipe,fork error\n"));
      return;
    } 
  if (!lookupdat->childpid)
  {
   close(fd[0]);   
   dup2(fd[1],STDOUT_FILENO); 
   
   cArg[argcount++] = "nmblookup";
   if ( *wins_name )
   {
     cArg[argcount++] = "-U";
     cArg[argcount++] = wins_name;
     cArg[argcount++] = "-R";
     cArg[argcount++] = machine_name;
   }
   else
   {
     cArg[argcount++] = machine_name;
   }
   cArg[argcount++] = "-S";
   cArg[argcount++] = "-d1";
   for ( loop = 0; loop < add_argcount; loop++ )
   {
     cArg[argcount++] = cAddArg[loop];
   }
   cArg[argcount++] = NULL;
           
   execvp(pref_get_nmblookup_exe(), (char **)cArg);
   
   /*if (*wins_name)
     execlp(pref_get_nmblookup_exe(),
           "nmblookup",
            "-U",wins_name,
            "-R",machine_name,
            "-S",
            "-d1",
            NULL);
   else
     execlp(pref_get_nmblookup_exe(),
           "nmblookup",
            machine_name,
            "-S",
            "-d1",
            NULL);*/
   _exit(0);
  }
  else
  {
  if (*wins_name)
    sprintf(logstr,"LookupByName:%s -U %s -R %s -S -d1",pref_get_nmblookup_exe(),wins_name,machine_name);
  else
    sprintf(logstr,"LookupByName:%s %s -S -d1",pref_get_nmblookup_exe(),machine_name);
  for ( loop = 0; loop < add_argcount; loop++ )
  {
    string_ncat(logstr, " ", MAXLOGSTRLEN);
    string_ncat(logstr, cAddArg[loop], MAXLOGSTRLEN);
  }
  gui_log_window_insert_string(logstr,1);
  close(fd[1]);   
  lookupbynamelist=g_slist_append(lookupbynamelist,lookupdat);
  lookupdat->linepos=0;
  lookupdat->linenum=0;
  lookupdat->browse_state=search_head;
  lookupdat->fd=fd[0];
  strcpy(lookupdat->machine_name,machine_name);
  lookupdat->group_name[0]=0;
  lookupdat->ipaddr[0]=0;
  lookupdat->input_tag = gdk_input_add(fd[0],GDK_INPUT_READ_, 
			    (GdkInputFunction)LookupByName_inputcallback,lookupdat);
  }                          
}




static void LookupByIP_inputcallback(LOOKUP_STRUCT *lookupdat,gint source, GdkInputCondition cond)
{
  GetSMBStateType smbstate;
  int   linec;
  int nodetype,isgroup,isactive;
  char nodename[MAXMACHNAMEL+10];

  if(cond & GDK_INPUT_READ_)
  {
   if (fdread_line_ok2(source,&linec,&(lookupdat->linepos),lookupdat->linebuf,0,NULL))
   {
     lookupdat->linenum++;
     gui_log_window_insert_string(lookupdat->linebuf,1);
     switch(lookupdat->browse_state) 
     {
      case search_head:
        if(strstr(lookupdat->linebuf,"Looking") && strstr(lookupdat->linebuf,"status"))
         {
          lookupdat->browse_state=search_entry;
         } 
        break; 
      case search_entry: /*Get Name and Group*/
         {
          if (getnodename(nodename,lookupdat->linebuf,&nodetype,&isgroup,&isactive,
                               MAXMACHNAMEL>MAXGROUPNAMEL?MAXMACHNAMEL:MAXGROUPNAMEL))
          {
            if(nodetype==0 && isgroup && isactive && !group_ignored(nodename))
            {
              lookupdat->group_name[MAXGROUPNAMEL]=0;
              strncpy(lookupdat->group_name,nodename,MAXGROUPNAMEL);
              if (lookupdat->machine_name[0]) linec=EOF;
            } 
            else if(nodetype==0 && !isgroup && isactive)
            {
              lookupdat->machine_name[MAXMACHNAMEL]=0;
              strncpy(lookupdat->machine_name,nodename,MAXMACHNAMEL);
              if (lookupdat->group_name[0]) linec=EOF;
            }
          }
/*          else linec=EOF;*/
         }
      default:;   
     }
     lookupdat->linepos=0;
   }
   if (linec==EOF){
     close(source);
     kill(lookupdat->childpid,SIGINT);
     waitpid(lookupdat->childpid,NULL,0);
     gdk_input_remove(lookupdat->input_tag);
     if(lookupdat->linenum==0)
       smbstate=SMB_STATE_EXEERR;
     else if (!lookupdat->machine_name[0] || !lookupdat->group_name[0])
       smbstate=SMB_STATE_SMBERR;
     else
       smbstate=SMB_STATE_OK;
     LookupByIP_done (lookupdat->ipaddr,lookupdat->machine_name,lookupdat->group_name,smbstate);
     lookupbyiplist=g_slist_remove(lookupbyiplist,lookupdat);
     g_free(lookupdat);
   }
  }
}


void LookupByIP(const char *ipaddr)
{
LOOKUP_STRUCT *lookupdat;
GSList *lookuplist;
int pipe_nok;
int fd[2];
char logstr[MAXLOGSTRLEN+1];
const char *cArg[20];
char *cAddArg[10];
int argcount = 0;
char add_arg_string[ARG_LEN+1];
int add_argcount = 0;
int loop;
/*  g_print("LookupByIP(%s)\n",ipaddr); */
  
  if (lookupbyiplist)
  {
    lookuplist= lookupbyiplist;
    while(lookuplist)
    {
     if(strcmp(((LOOKUP_STRUCT*)(lookuplist->data))->ipaddr,ipaddr))
      {     
       lookuplist=lookuplist->next;
      } 
     else
     {/*if LookupByIP with same ip is running, stop it */
      lookupdat=lookuplist->data;
      close(lookupdat->fd);
      usleep(50000);
      kill(lookupdat->childpid,SIGINT);
     waitpid(lookupdat->childpid,NULL,0);
      gdk_input_remove(lookupdat->input_tag); 
      lookupbyiplist=g_slist_remove(lookupbyiplist,lookupdat);
      g_free(lookupdat);
      break;
     }  
    }
  }
  lookupdat=g_malloc(sizeof(LOOKUP_STRUCT));
  
  if ( !is_empty_string(pref.v.nmblookup_arg) )
  {
    string_ncopy(add_arg_string, pref.v.nmblookup_arg, ARG_LEN);
    add_argcount = tokenize_string(cAddArg, add_arg_string, 10);
  }
  
  if((pipe_nok=pipe(fd))|| (lookupdat->childpid =fork())== -1)   
    {
      if (!pipe_nok)
        {close(fd[0]);close(fd[1]);}
      g_free(lookupdat);
      g_print(_("LookupByIP pipe,fork error\n"));
      return;
    } 
  if (!lookupdat->childpid)
  {
   close(fd[0]);   
   dup2(fd[1],STDOUT_FILENO); 
   
   cArg[argcount++] = "nmblookup";
   cArg[argcount++] = "-A";
   cArg[argcount++] = ipaddr;
   cArg[argcount++] = "-d1";
   
   for ( loop = 0; loop < add_argcount; loop++ )
   {
     cArg[argcount++] = cAddArg[loop];
   }
   cArg[argcount++] = NULL;
           
   execvp(pref_get_nmblookup_exe(), (char **)cArg);
   
   /*execlp(pref_get_nmblookup_exe(),
         "nmblookup",
          "-A",ipaddr,
          "-d1",
          NULL);*/
   _exit(0);
  }
  else
  {
  sprintf(logstr,"LookupByIP:%s -A %s -d1",pref_get_nmblookup_exe(),ipaddr);
  for ( loop = 0; loop < add_argcount; loop++ )
  {
    string_ncat(logstr, " ", MAXLOGSTRLEN);
    string_ncat(logstr, cAddArg[loop], MAXLOGSTRLEN);
  }
  gui_log_window_insert_string(logstr,1);
  close(fd[1]);   
  lookupbyiplist=g_slist_append(lookupbyiplist,lookupdat);
  lookupdat->linepos=0;
  lookupdat->linenum=0;
  lookupdat->browse_state=search_head;
  lookupdat->fd=fd[0];
  strcpy(lookupdat->ipaddr,ipaddr);
  lookupdat->group_name[0]=0;
  lookupdat->machine_name[0]=0;
  lookupdat->input_tag = gdk_input_add(fd[0],GDK_INPUT_READ_, 
			    (GdkInputFunction)LookupByIP_inputcallback,lookupdat);
  }                          
}



void stop_all_browse(void) /*Stop all unfinished browsing actions*/
{
  GSList *slist;
  GBROWSE_STRUCT *gbrowsedat;
  HBROWSE_STRUCT *hbrowsedat;
  SBROWSE_STRUCT *sbrowsedat;
  LOOKUP_STRUCT  *lookupdat;

  usleep(50000);

  slist= gbrowsedatlist;
  while(slist)    /*Kill GetSMBGroups*/
   {
     gbrowsedat=slist->data;
     close(gbrowsedat->fd[0]);
     kill(gbrowsedat->childpid,SIGINT);
     waitpid(gbrowsedat->childpid,NULL,0);
     gdk_input_remove(gbrowsedat->input_tag); 
     slist_free_with_data(&(gbrowsedat->groupmasterlist));
     slist=slist->next;
   }
  slist_free_with_data(&gbrowsedatlist);

  slist= hbrowsedatlist;
  while(slist)    /*Kill GetSMBMachines*/
   {
     hbrowsedat=slist->data;
     close(hbrowsedat->fd[0]);
     kill(hbrowsedat->childpid,SIGINT);
     waitpid(hbrowsedat->childpid,NULL,0);
     gdk_input_remove(hbrowsedat->input_tag); 
     slist_free_with_data(&(hbrowsedat->machinecommentlist));
     slist=slist->next;
   }
  slist_free_with_data(&hbrowsedatlist);

  slist= sbrowsedatlist;
  while(slist) /*Kill GetSMBShare*/
  {
    sbrowsedat=slist->data;
    close(sbrowsedat->fd[0]);
    kill(sbrowsedat->childpid,SIGINT);
     waitpid(sbrowsedat->childpid,NULL,0);
    gdk_input_remove(sbrowsedat->input_tag); 
    slist_free_with_data(&(sbrowsedat->sharecommenttypelist));
    slist = slist->next;
  }  
  slist_free_with_data(&sbrowsedatlist);

  slist=lookupbynamelist;
  while(slist) /*Kill LookupByName*/
  {
    lookupdat=slist->data;
    close(lookupdat->fd);
    kill(lookupdat->childpid,SIGINT);
     waitpid(lookupdat->childpid,NULL,0);
    gdk_input_remove(lookupdat->input_tag); 
    slist = slist->next;
  }  
  slist_free_with_data(&lookupbynamelist);

  slist=lookupbyiplist;
  while(slist) /*Kill LookupByIp*/
  {
    lookupdat=slist->data;
    close(lookupdat->fd);
    kill(lookupdat->childpid,SIGINT);
     waitpid(lookupdat->childpid,NULL,0);
    gdk_input_remove(lookupdat->input_tag); 
    slist = slist->next;
  }  
  slist_free_with_data(&lookupbyiplist);

}

