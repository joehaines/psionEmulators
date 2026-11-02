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
#include <stdlib.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/utsname.h>
#include "smbif.h"
#include "define.h"
#include "utility.h"
#include "guiwrap.h"
#include "preferences.h"
#include "data.h"

#define LVERSION(major,minor,patch) (((((major)<<8)+(minor))<<8)+(patch))

#define MAXMNTCOMMANDL (PATH_MAX+200)

#define MAXERRSTRLEN 200 /*smbumount errstr*/
#define MAXERRLEN   1023
#define MAX_ARG     21

typedef struct _MOUNT_STRUCT{
  int fdstdout;  
  int fdstderr;
  pid_t childpid;
  gint input_tag,input_tag2;
  char servicename[PATH_MAX+1];
  char mount_point[PATH_MAX+1];
  char *arglist[MAX_ARG+1]; 
  char interactiv_mode;
  char linebuf[MAXSMBLINEL+1],linebuf2[MAXSMBLINEL+1];
  char stderrbuf[MAXERRLEN+1];
  char stdoutbuf[MAXERRLEN+1];
  int  linepos,linepos2,linenum,oneeof,cleanup;
  int  sumount,passw_lf;
  int  port;
} MOUNT_STRUCT;
static GSList *mountlist = NULL;

typedef struct _UMOUNT_STRUCT{
  int fdstdout;  
  int fdstderr;
  pid_t childpid;
  gint input_tag;
  char mount_point[PATH_MAX+1];
  char linebuf[MAXSMBLINEL+1];
  int  linepos,linenum,tag;
  int  sumount;
} UMOUNT_STRUCT;
static GSList *umountlist = NULL;


char suprompt[] = "Passw";

unsigned char linux2_0=FALSE;  /*Get Version-Info at runtime, 1=kernel<2.1.70*/

char *escape_str(const char *s,GSList **strlist) /*Add escape characters to strings */
{                                          /*  and collect allocated space in strlist*/
static unsigned char need_escaping[] = { ' ','\r','\t','\n','\\','\'','"','&',
                                         '*','?','$','(',')','[',']','#','!','`'};
char *ss, *sp;
int n;
// if (*s==0) return s;
 n = strlen(s);
 ss=sp=g_malloc(2*n+1);
 while(1) {
   for (n = 0; n < sizeof(need_escaping); n++) {
     if (*s == need_escaping[n]) {
	*sp++ = '\\';
	*sp++ = *s;
	goto next;
	}
    }
    *sp++ = *s;
    if (*s == 0)
	break;
    next:
	s++;
 }
 *strlist=g_slist_append(*strlist,ss);
 return ss;
}

static void unsetlangenv(void)
{
  unsetenv("LANG");
  unsetenv("LANGUAGE");
}

void init_smbmount (void)
{
 struct utsname uts;
 int fdtty;
 unsigned short int major,minor,patch;
 if (!uname(&uts))
 {
  sscanf(uts.release,"%hd.%hd.%hd",&major,&minor,&patch);
  linux2_0=(((long)major<<16)+(minor<<8)+patch)<LVERSION(2,1,70);
 } 
 fdtty=open("/dev/tty",O_RDWR);
 if (fdtty>=0)
 {
  ioctl(fdtty,TIOCNOTTY); /*tty-input off, because of getpass() in su*/
  close(fdtty);
 }
}


int fdread_line_ok(int fd,int *linec,int *linepos,char *linebuf)
{
 char c;
 int  numread;
 numread=read(fd,&c,1);
 if (numread==0) 
   *linec=EOF;
 else
   *linec=c;  
 if (numread && c!='\n')
  {
   if (*linepos< MAXSMBLINEL)
     *(linebuf+(*linepos)++)=c;
   return(0);  
  } 
  else
  {
   *(linebuf+*linepos)=0;
   return(*linepos>0 || *linec=='\n');
  } 
}

#define  LINE_IGNORE_NUM 8
/* ignore smbmount stdout lines beginning with:*/
static char *ignore_smbmount_lines[LINE_IGNORE_NUM] = {
 "Server time",
 "Timezone",
 "Domain=",
 "security=",
 "connected as guest",
 "added interface ip=",
 "INFO:",
 "load_unicode_map"
 };

static char line_ignored(char *line)
{
 int i;
 for(i=0;i<LINE_IGNORE_NUM;i++)
 {
   if (!strncasecmp(line,ignore_smbmount_lines[i],strlen(ignore_smbmount_lines[i])))
     return(TRUE);
 }
 return(FALSE);
}

/* ------------------------------------------------------------------------- */

static void smbmount_done(char *servicename, char *mount_point,char ** arglist,
                          smbmountStateType state,char *errstr,char interactiv_mode)
{
  if (!interactiv_mode)
  {
   if (arglist[0] && state==MOUNT_STATE_OK) 
   {
    data_mount_add(servicename, mount_point);
    data_mem_mount_add(mem_mount_user, mount_point, arglist);
   }
   return;
  }
  if (state==MOUNT_STATE_OK) {
    data_mount_add(servicename, mount_point);
    if (arglist[0])
      data_mem_mount_add(mem_mount_user, mount_point, arglist);
    /* start file manager ? */
    /*if ( pref.v.run_file_manager ) {*/
    if ( globals.do_file_manager ) {
      postmount_execute(mount_point);
    }
  }
  else  
   gui_message_box2(errstr);
}

static void smbmount_inputcallback(MOUNT_STRUCT *mountdat,gint source, GdkInputCondition cond)
/*read  stderr and stdout!*/
{
 int linec,read_line_ok,*linepos,removed=FALSE,oneeof,i;
 gint *input_tag;
 smbmountStateType mountstate;
 char *colonpos,*linebuf;

 if(cond & GDK_INPUT_READ_)
 {
  if (!mountdat->cleanup)
  {
    oneeof=mountdat->oneeof;
    mountstate=MOUNT_STATE_OK;
    if (source==mountdat->fdstderr)
    {
     linepos=&(mountdat->linepos);
     linebuf=mountdat->linebuf;
     input_tag=&(mountdat->input_tag);
    }
    else
    {
     linepos=&(mountdat->linepos2);
     linebuf=mountdat->linebuf2;
     input_tag=&(mountdat->input_tag2);
    }
    read_line_ok=fdread_line_ok(source,&linec,linepos,linebuf);
    if (linec==EOF)
     {  
        if (!mountdat->oneeof)
        {
          mountdat->oneeof=TRUE;
          gdk_input_remove(*input_tag);
          *input_tag=0;
          if (*linepos==0) 
            return;
          removed=TRUE;  
        }
     }
    if (read_line_ok)
    {
     gui_log_window_insert_string(linebuf,1);
     mountdat->linenum++;
     *linepos=0;
     if (source==mountdat->fdstderr)
     { 
       if ((mountdat->sumount||mountdat->passw_lf) && !strncmp(linebuf,suprompt,sizeof(suprompt)-1) && (colonpos=index(linebuf,':')))
       {
         memmove(linebuf,colonpos+1,strlen(linebuf)-(colonpos-linebuf));
         if (mountdat->sumount && mountdat->passw_lf && !strncmp(linebuf,suprompt,sizeof(suprompt)-1) && (colonpos=index(linebuf,':')))
           memmove(linebuf,colonpos+1,strlen(linebuf)-(colonpos-linebuf));
       }
       if (strlen(linebuf)>2){
         strncat(mountdat->stderrbuf,linebuf,MAXERRLEN-strlen(mountdat->stderrbuf));
         strncat(mountdat->stderrbuf,"\n",MAXERRLEN-strlen(mountdat->stderrbuf));
       }
     }
     else
     {
       if (linux2_0 || !line_ignored(linebuf))
       {
         strncat(mountdat->stdoutbuf,linebuf,MAXERRLEN-strlen(mountdat->stdoutbuf));
         strncat(mountdat->stdoutbuf,"\n",MAXERRLEN-strlen(mountdat->stdoutbuf));
       }
     }       
    }
    if (linec==EOF && oneeof)
    {
     strncat(mountdat->stderrbuf,mountdat->stdoutbuf,MAXERRLEN-strlen(mountdat->stderrbuf));
     if (strlen(mountdat->stderrbuf)>4) mountstate=MOUNT_STATE_ERR;

     smbmount_done(mountdat->servicename,mountdat->mount_point,
                   mountdat->arglist,mountstate,mountdat->stderrbuf,mountdat->interactiv_mode);
     i=0;
     while(mountdat->arglist[i]) 
     {
      g_free(mountdat->arglist[i]);
      i++;
     }
     mountdat->arglist[0]=NULL;
     mountdat->cleanup=TRUE;
    }
  }
  if (mountdat->cleanup)
  { /*it seems only one input_tag may be removed in one call of this inputcallback
      otherwise a SIGBUS error occurs! (bug?)*/
   if (removed && (mountdat->input_tag || mountdat->input_tag2))
     return;
   if (mountdat->input_tag && mountdat->input_tag2)
   {
     gdk_input_remove(mountdat->input_tag2);
     mountdat->input_tag2=0;
     return;
   }
   close(mountdat->fdstderr);
   close(mountdat->fdstdout);
   if (mountdat->input_tag)
     gdk_input_remove(mountdat->input_tag);
   if (mountdat->input_tag2)
     gdk_input_remove(mountdat->input_tag2);
/*   kill(mountdat->childpid,SIGINT); */
     waitpid(mountdat->childpid,NULL,0);
   mountlist=g_slist_remove(mountlist,mountdat);
   g_free(mountdat);
  }
 }
}



void smbmount(const char *servicename, const char *mount_point, const char *clientname, const char *ipaddr,
              const char *group_name, const char *smbuser, const char *smbpasswd,uid_t uid,gid_t gid,
              mode_t filemode,mode_t dirmode,int port,const char *supasswd)
{
 char notfound[]="smbmount not found\n";
 int fdstdin[2],i;
 int fdstdout[2];  
 int fdstderr[2];  
 int  pipe2_nok,pipe3_nok=1;
 int  sumount,argnr=-1,passw_lf;
 char commandstr[MAXMNTCOMMANDL],commandstr1[MAXMNTCOMMANDL];
 char opt_group[MAXMNTCOMMANDL];
 char dummysmbpasswd[64];
/*#ifdef LINUX2_0 */
 char filemodestr[15],
      dirmodestr[15],
      uidstr[15],
      gidstr[15];
/*#else */
  char userpasswd[MAXMNTCOMMANDL];
/*#endif       */
 MOUNT_STRUCT *mountdat;
 const char *argv[MAX_ARG+1];
 GSList *strlist=NULL;
 char port_str[16];
 
 sprintf(port_str, "%d", port);
 
 memset(dummysmbpasswd,'*',sizeof(dummysmbpasswd)-1);
 if (strlen(smbpasswd)<sizeof(dummysmbpasswd)) dummysmbpasswd[strlen(smbpasswd)]=0;
 else dummysmbpasswd[sizeof(dummysmbpasswd)-1]=0;
 sumount=(supasswd!=NULL); 
 passw_lf=(*smbpasswd==0) && !linux2_0; /*smbmount 2.0.3 always waits for password, 
                                    even if called with -Uuser% or -N if server is in user level security*/
 if (sumount|| passw_lf)
  if (pipe(fdstdin))
   {
    g_print(_("smbmount pipe fdstdin error\n"));
    return;   
   }
 if (sumount)
  {
    argv[0]="su";
    argv[1]="-c";
    argv[2]=commandstr;
    argv[3]=NULL;
    if (*group_name)
    {
      if (linux2_0) strcpy(opt_group,"-D ");
      else 
      {
        if (pref.v.smbmount_version>SMBMOUNT_205) strcpy(opt_group,",workgroup=");
        else strcpy(opt_group,"-W ");
      }
      strcat(opt_group,escape_str(group_name,&strlist));
    }
    else *opt_group=0;
    if (linux2_0)
    {
      /* linux 2.0 */
      if (*smbpasswd==0)
       sprintf(commandstr,"%s %s %s -c %s -I %s %s -U %s -f %d -d %d -u %d -g %d -n",
         escape_str(pref_get_smbmount_exe(),&strlist),escape_str(servicename,&strlist),escape_str(mount_point,&strlist),
         escape_str(clientname,&strlist),ipaddr,opt_group,escape_str(smbuser,&strlist),filemode,dirmode,uid,gid);
      else
       sprintf(commandstr,"%s %s %s -c %s -I %s %s -U %s -f %d -d %d -u %d -g %d -P %s",
        escape_str(pref_get_smbmount_exe(),&strlist),escape_str(servicename,&strlist),escape_str(mount_point,&strlist),escape_str(clientname,&strlist),
        ipaddr,opt_group,escape_str(smbuser,&strlist),filemode,dirmode,uid,gid,escape_str(smbpasswd,&strlist));
    }
    else
    { 
       /* linux 2.2/2.4 */
       switch (pref.v.smbmount_version ){
       case SMBMOUNT_205:
        sprintf(commandstr,"%s %s %s -I %s %s -U%s%%%s -d0",
         escape_str(pref_get_smbmount_exe(),&strlist),escape_str(servicename,&strlist),escape_str(mount_point,&strlist),ipaddr,opt_group,
         escape_str(smbuser,&strlist),escape_str(smbpasswd,&strlist));
        break;
       case SMBMOUNT_206:
        sprintf(commandstr,"%s %s %s -o username=%s%%%s,fmask=%d,dmask=%d,uid=%d,gid=%d,ip=%s,debug=0%s",
         escape_str(pref_get_smbmount_exe(),&strlist),escape_str(servicename,&strlist),escape_str(mount_point,&strlist),
         escape_str(smbuser,&strlist),escape_str(smbpasswd,&strlist),
         filemode,dirmode,uid,gid,ipaddr,opt_group);
         if ( (port >= 0) && (port < 65536) )
         {
          sprintf(commandstr, "%s,port=%d", commandstr, port);
         }
         if ( !is_empty_string(pref.v.smbmount_arg) )
         {
           if ( pref.v.smbmount_arg[0] != ',' )
            string_ncat(commandstr, ",", MAXMNTCOMMANDL);
           string_ncat(commandstr, escape_str(pref.v.smbmount_arg,&strlist), MAXMNTCOMMANDL);
         }
        break;
       default: /*SMBMOUNT_204*/
        sprintf(commandstr1,"mount %s -f %d -d %d -u %d -g %d",/*escape_str_smbmnt(*/mount_point/*,&strlist)*/,filemode,dirmode,uid,gid);
       /*There will be problems with {' ','\r','\t','\n','\\','\'','"','&','*','(',')','`'}; in mountpoint but it cannot be solved here*/
        sprintf(commandstr,"%s %s -I %s %s -U%s%%%s -d0 -c %s",
         escape_str(pref_get_smbmount_exe(),&strlist),escape_str(servicename,&strlist),ipaddr,opt_group,escape_str(smbuser,&strlist),
         escape_str(smbpasswd,&strlist),escape_str(commandstr1,&strlist));
        break;
      }
    }
  }
  else
  {
    /* no sumount */
    if(linux2_0)
    {
      argv[++argnr]="smbmount";
      argv[++argnr]=servicename;
      argv[++argnr]=mount_point;
      argv[++argnr]="-c";
      argv[++argnr]=clientname;
      argv[++argnr]="-I";
      argv[++argnr]=ipaddr;
      if (*group_name)
      {
        argv[++argnr]="-D";
        argv[++argnr]=group_name;
      }
      argv[++argnr]="-U";
      argv[++argnr]=smbuser;
      argv[++argnr]="-f";sprintf(filemodestr,"%d",filemode);
      argv[++argnr]=filemodestr;
      argv[++argnr]="-d";sprintf(dirmodestr,"%d",dirmode);
      argv[++argnr]=dirmodestr;
      argv[++argnr]="-u";sprintf(uidstr,"%d",uid);
      argv[++argnr]=uidstr;
      argv[++argnr]="-g";sprintf(gidstr,"%d",gid);
      argv[++argnr]=gidstr;
      if (*smbpasswd==0)
        argv[++argnr]="-n";
      else
      {
        argv[++argnr]="-P";
        argv[++argnr]=smbpasswd;
      }
      argv[++argnr]=NULL;
    }
    else
    {
      /* linux 2.2/2.4 */
      argv[++argnr]=pref_get_smbmount_exe();
      argv[++argnr]=servicename;
      if (pref.v.smbmount_version>SMBMOUNT_204) argv[++argnr]=mount_point;
      if (pref.v.smbmount_version<SMBMOUNT_206)
      {
        /* 2.04/2.05 */
        argv[++argnr]="-I";
        argv[++argnr]=ipaddr;
        if (*group_name)
        {
          argv[++argnr]="-W";
          argv[++argnr]=group_name;
        }
        sprintf(userpasswd,"-U%s%%%s",smbuser,smbpasswd);
        argv[++argnr]=userpasswd;
        argv[++argnr]="-d0";
        if (pref.v.smbmount_version <SMBMOUNT_205)
        {
         argv[++argnr]="-c";
         argv[++argnr]=commandstr;
         sprintf(commandstr,"mount %s -f %d -d %d -u %d -g %d",
                /*escape_str_smbmnt(*/mount_point/*,&strlist)*/,filemode,dirmode,uid,gid);
        }

        if ( !is_empty_string(pref.v.smbmount_arg) )
        {
        }
      }
      else /*>=SMBMOUNT_206*/
      {
        argv[++argnr]="-o";
        argv[++argnr]=commandstr;
        sprintf(commandstr,"username=%s%%%s,fmask=%d,dmask=%d,uid=%d,gid=%d,debug=0",
           smbuser,smbpasswd,filemode,dirmode,uid,gid);
//        sprintf(commandstr,"username=%s%%%s,fmask=%d,dmask=%d,uid=%d,gid=%d,ip=%s,debug=0",
//           smbuser,smbpasswd,filemode,dirmode,uid,gid,ipaddr);
        if (*group_name)
        {
          strcat(commandstr,",workgroup=");
          strcat(commandstr,escape_str(group_name,&strlist));
        }
        if ( (port >= 0) && (port < 65536) )
        {
          string_ncat(commandstr, ",port=", MAXMNTCOMMANDL);
          string_ncat(commandstr, port_str, MAXMNTCOMMANDL);
        }
        if ( !is_empty_string(pref.v.smbmount_arg) )
        {
          if ( pref.v.smbmount_arg[0] != ',' )
           string_ncat(commandstr, ",", MAXMNTCOMMANDL);
          string_ncat(commandstr, escape_str(pref.v.smbmount_arg,&strlist), MAXMNTCOMMANDL);
        }
       }
       argv[++argnr]=NULL;
      }
  }
  slist_free_with_data(&strlist);
  mountdat=g_malloc(sizeof(MOUNT_STRUCT));
  if((pipe2_nok=pipe(fdstdout))|| (pipe3_nok=pipe(fdstderr))||
        (mountdat->childpid =fork())== -1)   
  {
    if (sumount|| passw_lf){close(fdstdin[0]);close(fdstdin[1]);}
    if (!pipe2_nok) 
      {close(fdstdout[0]);close(fdstdout[1]);}
    if (!pipe3_nok)
      {close(fdstderr[0]);close(fdstderr[1]);}
    g_free(mountdat);
    g_print(_("smbmount pipe,fork error\n"));
    return;
  } 
  
  if (!mountdat->childpid)
  {
   close(fdstdout[0]);   
   close(fdstderr[0]);   
   dup2(fdstdout[1],STDOUT_FILENO); 
   dup2(fdstderr[1],STDERR_FILENO); 
   if (sumount || passw_lf) {
     close(fdstdin[1]);   
     dup2(fdstdin[0],STDIN_FILENO); 
   }  
   unsetlangenv();

   if (sumount)
     execvp("su",(char **)argv);
   else
     execvp(pref_get_smbmount_exe(),(char **)argv);
   write(STDERR_FILENO,notfound,sizeof(notfound)-1); 
   _exit(0);
  }
  else
  {
  if (sumount)
     mountdat->arglist[0]=NULL;
  else
  {
    i=0;
    while(argv[i+1]) 
    {
      mountdat->arglist[i]=g_strdup(argv[i+1]);
      i++;
    }
    mountdat->arglist[i]=NULL;
  }
  close(fdstdout[1]); 
  close(fdstderr[1]); 

  if (sumount)
  {
    if(linux2_0)
    {
    if (*smbpasswd!=0)
     sprintf(commandstr,"'%s' '%s' '%s' -c '%s' -I %s %s -U '%s' -f %d -d %d -u %d -g %d -P '%s'",
       pref_get_smbmount_exe(),servicename,mount_point,clientname,ipaddr,opt_group,smbuser,filemode,dirmode,uid,gid,dummysmbpasswd);
    }
    else
    {
    switch (pref.v.smbmount_version ){
      case SMBMOUNT_205:
        sprintf(commandstr,"'%s' '%s' '%s' -I %s %s '-U%s%%%s' -d0",
          pref_get_smbmount_exe(),servicename,mount_point,ipaddr,opt_group,smbuser,dummysmbpasswd);
        break;
      case SMBMOUNT_206:
        sprintf(commandstr,"%s %s %s -o username=%s%%%s,fmask=%d,dmask=%d,uid=%d,gid=%d,ip=%s,debug=0%s",
         pref_get_smbmount_exe(),servicename,mount_point,smbuser,dummysmbpasswd,
         filemode,dirmode,uid,gid,ipaddr,opt_group);
        if ( (port >= 0) && (port < 65536) )
        {
          string_ncat(commandstr, ",port=", MAXMNTCOMMANDL);
          string_ncat(commandstr, port_str, MAXMNTCOMMANDL);
        }
        if ( !is_empty_string(pref.v.smbmount_arg) )
        {
          if ( pref.v.smbmount_arg[0] != ',' )
           string_ncat(commandstr, ",", MAXMNTCOMMANDL);
          string_ncat(commandstr, escape_str(pref.v.smbmount_arg,&strlist), MAXMNTCOMMANDL);
        }
        break;
      default:
        sprintf(commandstr,"'%s' '%s' -I %s %s '-U%s%%%s' -d0 -c 'mount %s -f %d -d %d -u %d -g %d'",
          pref_get_smbmount_exe(),servicename,ipaddr,opt_group,smbuser,dummysmbpasswd,mount_point,filemode,dirmode,uid,gid);  
        break;
     }
    }
    log_execvp_str("su",(char **)argv);
    close(fdstdin[0]);
    write(fdstdin[1], supasswd, strlen(supasswd));
    write(fdstdin[1], "\n",1);
    if (passw_lf)
    {
      usleep(50000); 
      if (!waitpid(mountdat->childpid,NULL,WNOHANG)) /*child still alive?*/
        write(fdstdin[1], "\n",1);
    }
    close(fdstdin[1]);
    if (*supasswd) 
      for(i=0;i<strlen(supasswd);i++)
        gui_log_window_insert_string("*",0);
  } 
  else
  {
    if(linux2_0)
    {
      if (*smbpasswd!=0) argv[argnr-1]=dummysmbpasswd;
    }
    else
    { 
      if (pref.v.smbmount_version<SMBMOUNT_206)
        sprintf(userpasswd,"-U%s%%%s",smbuser,dummysmbpasswd);
      else
      {
       sprintf(commandstr,"username=%s%%%s,fmask=%d,dmask=%d,uid=%d,gid=%d,ip=%s,debug=0",
          smbuser,dummysmbpasswd,filemode,dirmode,uid,gid,ipaddr);
       if (*group_name){
         strcat(commandstr,",workgroup=");
         strcat(commandstr,group_name);
       }
       if ( (port >= 0) && (port < 65536) )
       {
         string_ncat(commandstr, ",port=", MAXMNTCOMMANDL);
         string_ncat(commandstr, port_str, MAXMNTCOMMANDL);
       }
       if ( !is_empty_string(pref.v.smbmount_arg) )
       {
         if ( pref.v.smbmount_arg[0] != ',' )
          string_ncat(commandstr, ",", MAXMNTCOMMANDL);
         string_ncat(commandstr, escape_str(pref.v.smbmount_arg,&strlist), MAXMNTCOMMANDL);
       }
      }
      if (passw_lf)
      {
       usleep(50000);
       close(fdstdin[0]);
       if (!waitpid(mountdat->childpid,NULL,WNOHANG))
          write(fdstdin[1], "\n",1);
       close(fdstdin[1]);
      }
    }
    
    log_execvp_str(pref_get_smbmount_exe(),(char **)argv);
  }
  mountlist=g_slist_append(mountlist,mountdat);
  mountdat->linepos=0;  mountdat->linepos2=0;
  mountdat->linenum=0;
  mountdat->sumount=sumount;      
  mountdat->passw_lf=passw_lf;
  memset(mountdat->stderrbuf,0,MAXERRLEN+1);
  memset(mountdat->stdoutbuf,0,MAXERRLEN+1);
  mountdat->fdstdout=fdstdout[0];
  mountdat->fdstderr=fdstderr[0];
  mountdat->oneeof=FALSE;
  mountdat->cleanup=FALSE;
  mountdat->servicename[PATH_MAX]=0;
  mountdat->port = port;
  strncpy(mountdat->servicename,servicename,PATH_MAX);
  mountdat->mount_point[PATH_MAX]=0;
  strncpy(mountdat->mount_point,mount_point,PATH_MAX);

  mountdat->interactiv_mode=TRUE;
  mountdat->input_tag = gdk_input_add(fdstderr[0], GDK_INPUT_READ_, 
			    (GdkInputFunction)smbmount_inputcallback, mountdat);
  mountdat->input_tag2 = gdk_input_add(fdstdout[0],GDK_INPUT_READ_, 
			    (GdkInputFunction)smbmount_inputcallback, mountdat);  
  }                          
}

void smb_mount_memorized(char *mount_point,char ** arglist,char gui_mode)
{
 char notfound[]="smbmount not found\n";
 int fdstdin[2],i;
 int fdstdout[2];  
 int fdstderr[2];  
 int  pipe2_nok,pipe3_nok=1,
      passw_lf,fdnull;
 MOUNT_STRUCT *mountdat;
 char *argv[MAX_ARG+1];
 pid_t childpid;

 passw_lf=!linux2_0;
 if (passw_lf && pipe(fdstdin))
 {
   g_print(_("smb_mount_memorized pipe fdstdin error\n"));
   return;   
 }
 argv[0]="smbmount";
 i=0;
 while(arglist[i]) 
  {
   argv[i+1]=arglist[i];
   i++;
   if (i==(MAX_ARG-1)) break;
  }
 argv[i+1]=NULL;
 if (!gui_mode)
 {
  if ((childpid =fork())== -1)   
  {
   if (passw_lf){close(fdstdin[0]);close(fdstdin[1]);}
   return;
  } 
  if (!childpid)
  {
   fdnull=open("/dev/null",O_WRONLY);
   if (fdnull>=0)
   {
     dup2(fdnull,STDOUT_FILENO);
     dup2(fdnull,STDERR_FILENO);
   }
   if (passw_lf) {
     close(fdstdin[1]);   
     dup2(fdstdin[0],STDIN_FILENO); 
   }  
   unsetlangenv();
   execvp(pref_get_smbmount_exe(),argv);
   _exit(0);
  }
  else
  {
   if (passw_lf)
   {
    usleep(50000);
    close(fdstdin[0]);
    if (!waitpid(childpid,NULL,WNOHANG))
      write(fdstdin[1], "\n",1);
      close(fdstdin[1]);
   }
  }
  return;
 }
 /*gui_mode=TRUE!*/
  mountdat=g_malloc(sizeof(MOUNT_STRUCT));
  if((pipe2_nok=pipe(fdstdout))|| (pipe3_nok=pipe(fdstderr))||
       (mountdat->childpid =fork())== -1)   
  {
   if (passw_lf){close(fdstdin[0]);close(fdstdin[1]);}
   if (!pipe2_nok) 
     {close(fdstdout[0]);close(fdstdout[1]);}
   if (!pipe3_nok)
      {close(fdstderr[0]);close(fdstderr[1]);}
   g_free(mountdat);
   g_print(_("smbmount pipe,fork error\n"));
  return;
  } 
  if (!mountdat->childpid)
  {
   close(fdstdout[0]);
   close(fdstderr[0]);   
   dup2(fdstdout[1],STDOUT_FILENO); 
   dup2(fdstderr[1],STDERR_FILENO); 
   if (passw_lf) {
     close(fdstdin[1]);   
     dup2(fdstdin[0],STDIN_FILENO); 
   }  
   unsetlangenv();
   execvp(pref_get_smbmount_exe(),argv);
   write(STDERR_FILENO,notfound,sizeof(notfound)-1); 
   _exit(0);
  }
  else
  {

  i=0;
  while(arglist[i]) 
  {
   mountdat->arglist[i]=g_strdup(arglist[i]);
   i++;
  }
  mountdat->arglist[i]=NULL;

  close(fdstdout[1]); 
  close(fdstderr[1]); 
/*  log_execvp_str(smbmountExe,arglist);*/
  if (passw_lf)
  {
    usleep(50000);
    close(fdstdin[0]);
    if (!waitpid(mountdat->childpid,NULL,WNOHANG))
      write(fdstdin[1], "\n",1);
    close(fdstdin[1]);
  }
  mountlist=g_slist_append(mountlist,mountdat);
  mountdat->linepos=0;  mountdat->linepos2=0;
  mountdat->linenum=0;
  mountdat->sumount=FALSE;      
  mountdat->passw_lf=passw_lf;
  memset(mountdat->stderrbuf,0,MAXERRLEN+1);
  memset(mountdat->stdoutbuf,0,MAXERRLEN+1);
  mountdat->fdstdout=fdstdout[0];
  mountdat->fdstderr=fdstderr[0];
  mountdat->oneeof=FALSE;
  mountdat->cleanup=FALSE;
  mountdat->servicename[PATH_MAX]=0;
  strncpy(mountdat->servicename,arglist[0],PATH_MAX);
  mountdat->mount_point[PATH_MAX]=0;
  strncpy(mountdat->mount_point,mount_point,PATH_MAX);

  mountdat->interactiv_mode=FALSE;
  mountdat->input_tag = gdk_input_add(fdstderr[0], GDK_INPUT_READ_, 
			    (GdkInputFunction)smbmount_inputcallback, mountdat);
  mountdat->input_tag2 = gdk_input_add(fdstdout[0],GDK_INPUT_READ_, 
			    (GdkInputFunction)smbmount_inputcallback, mountdat);  
  }
}


static void smbumount_inputcallback(UMOUNT_STRUCT *mountdat,gint source, GdkInputCondition cond)
{
  int linec;
  smbmountStateType mountstate=MOUNT_STATE_OK;
  char *colonpos;

  if(cond & GDK_INPUT_READ_)
  {
   if (fdread_line_ok(source,&linec,&(mountdat->linepos),mountdat->linebuf))
   {
    mountdat->linenum++;
    mountstate=MOUNT_STATE_OK;
    gui_log_window_insert_string(mountdat->linebuf,1);
    if (mountdat->sumount)
     {
      if(mountdat->linenum>1 || strncmp(mountdat->linebuf,suprompt,sizeof(suprompt)-1)||(mountdat->linepos>11))
      {
       if (!strncmp(mountdat->linebuf,suprompt,sizeof(suprompt)-1))
       {
        if ((colonpos=index(mountdat->linebuf,':')))
           memmove(mountdat->linebuf,colonpos+1,mountdat->linepos-(colonpos-mountdat->linebuf));
       }
       mountstate=MOUNT_STATE_ERR;
       linec=EOF;
      } 
     }
     else
     {
      mountstate=MOUNT_STATE_ERR;
      linec=EOF;
     }
     mountdat->linepos=0;
   }
   if (linec==EOF){
     close(source);
     close(mountdat->fdstdout);
     kill(mountdat->childpid,SIGINT);
     waitpid(mountdat->childpid,NULL,0);
     gdk_input_remove(mountdat->input_tag);
     gui_smbumount_done(mountdat->mount_point,mountdat->linebuf,mountstate,mountdat->tag);
     if ( pref.v.delete_mountpoints ) {
       rmdir(mountdat->mount_point);
     }
     umountlist=g_slist_remove(umountlist,mountdat);
     g_free(mountdat);
   }
  }
}



void smbumount(const char *mount_point,const char *supasswd,char smb,int tag)
{
 char notfound[]="umount not found\n";
 int fdstdin[2],i;
 int fdstdout[2];  
 int fdstderr[2];  
 int  pipe2_nok,pipe3_nok=1;
 pid_t childpid;
 int  sumount;
 char commandstr[MAXMNTCOMMANDL];
 char *argv[4];
 GSList *strlist=NULL;
 UMOUNT_STRUCT *mountdat;
 
 if ((sumount=(/*(geteuid()!=0) && */(supasswd!=NULL))))
  {
  if (pipe(fdstdin))
   {
    g_print(_("smbumount pipe fdstdin error\n"));
    return;   
   }
    argv[0]="su";
    argv[1]="-c";
    argv[2]=commandstr;
    argv[3]=NULL;
    sprintf(commandstr,"%s %s", escape_str(smb ? pref_get_smbumount_exe():"umount",&strlist),escape_str(mount_point,&strlist));
    if ( !is_empty_string(pref.v.smbumount_arg) )
    {
      string_ncat(commandstr, " ", MAXMNTCOMMANDL);
      string_ncat(commandstr, escape_str(pref.v.smbumount_arg,&strlist), MAXMNTCOMMANDL);
    }
    slist_free_with_data(&strlist);
  }
  else
  {
    argv[0]=smb ? "smbumount":"umount";
    argv[1]=(char *)mount_point;
    if ( !is_empty_string(pref.v.smbumount_arg) )
    {
      argv[2] = pref.v.smbumount_arg;
      argv[3] = NULL;
    }
    else
    {
      argv[2]=NULL;
    }
  }
  if((pipe2_nok=pipe(fdstdout))|| (pipe3_nok=pipe(fdstderr))||
        (childpid =fork())== -1)   
   {
    if (sumount){close(fdstdin[0]);close(fdstdin[1]);}
    if (!pipe2_nok) 
      {close(fdstdout[0]);close(fdstdout[1]);}
    if (!pipe3_nok)
      {close(fdstderr[0]);close(fdstderr[1]);}
    g_print(_("smbumount pipe,fork error\n"));
    return;   
   } 
  if (!childpid)
  {
   close(fdstdout[0]);   
   close(fdstderr[0]);   
   dup2(fdstdout[1],STDOUT_FILENO); 
   dup2(fdstderr[1],STDERR_FILENO); 
   unsetlangenv();
   if (sumount) {
/*     ioctl(STDIN_FILENO,TIOCNOTTY); */
     close(fdstdin[1]);   
     dup2(fdstdin[0],STDIN_FILENO); 
     execvp("su",argv);
   }  
   else
     execvp(smb ? pref_get_smbumount_exe():"umount",argv);
   write(STDERR_FILENO,notfound,sizeof(notfound)-1); 
   _exit(0);
  }
  else
  {
   close(fdstdout[1]); 
   close(fdstderr[1]); 
   if (sumount)
   {
    log_execvp_str("su",argv);
    close(fdstdin[0]);
    write(fdstdin[1], supasswd, strlen(supasswd));
    write(fdstdin[1], "\n",1);
    close(fdstdin[1]);
    if (*supasswd) 
      for(i=0;i<strlen(supasswd);i++)
        gui_log_window_insert_string("*",0);
   } 
   else
     log_execvp_str(smb ? pref_get_smbumount_exe():"umount",argv);
   mountdat=g_malloc(sizeof(UMOUNT_STRUCT));
   umountlist=g_slist_append(umountlist,mountdat);
   mountdat->childpid=childpid;
   mountdat->linepos=0; 
   mountdat->linenum=0;
   mountdat->sumount=sumount;      
   mountdat->fdstdout=fdstdout[0];
   mountdat->fdstderr=fdstderr[0];
   mountdat->mount_point[PATH_MAX]=0;
   strncpy(mountdat->mount_point,mount_point,PATH_MAX);
   mountdat->tag=tag;
   mountdat->input_tag = gdk_input_add(fdstderr[0], GDK_INPUT_READ_, 
			    (GdkInputFunction)smbumount_inputcallback, mountdat);

  }                          
}

void stop_all_mount(void) /*Stop all unfinished smbmount actions*/
{
  GSList *slist;
  MOUNT_STRUCT *mountdat;
  UMOUNT_STRUCT *umountdat;
  int i;
  usleep(50000);
  slist= mountlist;
  while(slist)    /*Kill smbmount*/
   {
     mountdat=slist->data;
     kill(mountdat->childpid,SIGINT);
     waitpid(mountdat->childpid,NULL,0);
     if (mountdat->input_tag)
       {close(mountdat->fdstderr);gdk_input_remove(mountdat->input_tag);}
     if (mountdat->input_tag2)
       {close(mountdat->fdstdout);gdk_input_remove(mountdat->input_tag2);}
     i=0;
     while(mountdat->arglist[i]) 
     {
      g_free(mountdat->arglist[i]);
      i++;
     }
     slist=slist->next;
   }
  slist_free_with_data(&mountlist);
  slist= umountlist;
  while(slist)    /*Kill smbmount*/
   {
     umountdat=slist->data;
     kill(umountdat->childpid,SIGINT);
     waitpid(umountdat->childpid,NULL,0);
     close(umountdat->fdstderr);gdk_input_remove(umountdat->input_tag);
     close(umountdat->fdstdout);
     slist=slist->next;
   }
  slist_free_with_data(&umountlist);
}




