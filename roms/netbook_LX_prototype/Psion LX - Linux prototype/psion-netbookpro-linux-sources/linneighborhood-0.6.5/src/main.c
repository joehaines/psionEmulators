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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "guiwrap.h"
#include "preferences.h"
#include "data.h"
#include "browsewrap.h"
#include "utility.h"
#include "io.h"
#include "smbif.h"
#include "libsmb.h"


/* ------------------------------------------------------------------------------- */

/* realtime timer handler for periodic updates */

static void timer_interval_handler (gpointer data)
{
  /* do an update for the mounted share list */
  if ( pref.v.mount_retrigger_enable )
#ifndef NO_SMBMOUNT
    mount_list_retrigger();
#else
    ;
#endif
}

/* ------------------------------------------------------------------------------- */

static void print_help (void)
{
  fprintf(stdout, "\nLinNeighborhood version "VERSION"\n");
  fprintf(stdout, "This free software distributed under under the terms of the\n"
                  "GNU General Public License\n\n");
  fprintf(stdout, "Optional Parameters:\n");
  fprintf(stdout, " -h --help                = print this help text\n");
  fprintf(stdout, " -v --version             = print program version\n");
#ifndef NO_SMBMOUNT
  fprintf(stdout, " -m --mountshares         = mount memorized shares without starting the GUI\n"); /*74*/
#endif
  fprintf(stdout, " -i --iconify             = start program iconified\n");
  /*fprintf(stdout, " -n --networkimage [file] = does an entire network scan, writing the image\n"
                  "                            to a file ( no GUI )\n");*/
  fprintf(stdout, " -u --user [username]     = does the network scan as user\n");
  fprintf(stdout, " -p --password [userpswd] = does the user network scan with password\n");
  fprintf(stdout, " -j --job [number]        = number of max simultaneous browse jobs\n");
  fprintf(stdout, "\n");
}

/* ------------------------------------------------------------------------------- */

static void cleanup ()
{
  gui_share_window_clean();
  browse_clean();
  data_clean();
}

static void save_exit (int exitcode)
{
  cleanup();

#ifndef NO_LIBSMB
  /* unload libsmbclient.so */
  lib_smb_close();
#endif
  
  exit(exitcode);
}

/* ------------------------------------------------------------------------------- */

/*static unsigned char browse_network_ready;*/

#ifndef NO_SMBMOUNT

static void restore_mounts (void)
{
  GSList *mplist, *templist;
  mem_mount_struct *mount;
  
  mplist = mountpoint_load();
  if ( mplist != NULL )
  {
    /* do mount action */
    templist = mplist;
    while ( templist != NULL )
    {
      mount = (mem_mount_struct*)(templist->data);
      if ( mount != NULL )
      {
        utility_do_mount(mount);
        if ( mount->arglist != NULL )
        {
          slist_free_with_data(&(mount->arglist));
        }
      }
      templist = templist->next;
    }
    
    /* delete list */
    slist_free_with_data(&mplist);
  }
}

#endif


/*static void browse_network_callback (void)
{
  if ( startup.b.netimage_file == YES )
  {
    network_image_export(startup.v.netimage_file);
  }
  else
  {
    network_image_export(SMBNETWORK_FILE);
  }
  browse_network_ready = 1;
}*/

static void execute_command_line ()
{
  unsigned char do_exit;
  /*BROWSE_MODE mode;
  unsigned int maxsleep;*/
  
  do_exit = 0;
  
  /* print help output */
  if ( startup.v.show_help )
  {
    print_help();
    do_exit = 1;
  }
  
  /* print program version */
  if ( startup.v.print_version )
  {
    fprintf(stdout, VERSION"\n");
    do_exit = 1;
  }
  
#ifndef NO_SMBMOUNT

  /* restore memorized shares */
  if ( startup.v.restore_shares )
  {
    restore_mounts();
    do_exit = 1;
  }

#endif
  
  /* do a complete network scan */
  if ( startup.v.network_image )
  {
    /*mode = BROWSE_MODE_NO_GUI;
    if ( startup.v.is_user )
      mode |= BROWSE_MODE_USER;
    browse_network_ready = 0;
    browse_network(mode);
    browse_network_wait_done(browse_network_callback);
    while ( 1 )
    {
      sleep(1);
      if ( browse_network_ready )
        break;
    }
    do_exit = 1;*/
  }
  
  /* store predefined username */
  if ( startup.v.is_user )
  {
    string_ncopy(globals.browse_user, startup.v.user, USER_LEN);
  }
  
  /* store predefined password */
  if ( startup.v.is_password )
  {
    string_ncopy(globals.browse_password, startup.v.password, PASSWORD_LEN);
  }
  
  if ( do_exit )
  {
    save_exit(0);
  }
}

/* ------------------------------------------------------------------------------- */

int main (int argc, char *argv[])
{
  BROWSE_MODE mode;

#ifndef NO_LIBSMB  
  /* initialize libsmbclient.so */
  lib_smb_init("libsmbclient.so");
#endif

  /* data initializations */
  data_init();                      /* browse data */
  browse_init();                    /* browse wrapper stuff */
  
  /* global variable initializations */
  globals_init();
  startup_parse_command_line(argc, argv, &startup);
  preferences_load(&pref);
#ifndef NO_SMBMOUNT
  init_smbmount();
#endif
  
  /* other stored information */
  add_master_load();                /* load additional master browser */
  groupmaster_load();               /* load additional group masters */
  prefhost_load();                  /* load preferred hosts */
  password_load();                  /* load default password */
  
#ifndef NO_SMBMOUNT
  /* scan mtab for existing mounts */
  mount_list_retrigger();
#endif
  /* execute command line parameters */
  execute_command_line();

#ifndef NO_SMBMOUNT
  if ( pref.v.memorize_mounts )
  {
    restore_mounts();
  }
#endif

  /* -------------- */
  /* -- GUI part -- */
  /* -------------- */
  
  /* GUI internationalization */
  gui_set_locale();
  
  /* localization */
  bindtextdomain(PACKAGE, LOCALEDIR);
  textdomain(PACKAGE);
  
  /* --- GUI initialization --- */
  gui_init(argc, argv);
#ifndef NO_SMBMOUNT
  gui_mount_dialog_init();
#endif
	
  /* initial browse on startup */
  if ( pref.v.startup_browse == YES )
  {
    mode = BROWSE_MODE_NONE;
    if ( pref.v.quick_browse == YES )
      mode |= BROWSE_MODE_QUICK;
    if ( pref.v.groups_scan_user == YES )
      mode |= BROWSE_MODE_USER;
    gui_set_cursor(cursor_watch);
    browse_groups(mode | BROWSE_MODE_EXPAND);
  }
  
  /* --- handler for periodic timer intervall --- */
  gui_timer_start(TIMER_INTERVAL_UPDATE, timer_interval_handler, NULL);

  /* -- GUI main loop -- */
  gui_start ();
  gui_timer_stop();
  
  /* -------------------------- */
  /* -- end of GUI main loop -- */
  /* -------------------------- */
  
  /* save any informations */
#ifndef NO_SMBMOUNT
  if ( pref.v.memorize_mounts )
  {
    mountpoint_save();
  }
#endif
  password_save();                  /* save default password */
  prefhost_save();                  /* save preferred hosts */
  groupmaster_save();               /* save additional group masters */
  add_master_save();                /* save additional master browsers */
  
  /* data + memory cleanup */
  cleanup();
  
  save_exit(0);
  return 0;
}
