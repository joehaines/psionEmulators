// -*- mode:C++; tab-width:2; c-basic-offset:2; indent-tabs-mode:nil -*-

/* Blackjack - game.cpp
 * Copyright (C) 2003 William Jon McCann <mccann@jhu.edu>
 *
 * This game is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 */

#include <config.h>

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

#include <gnome.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <dirent.h>
#include <ctype.h>

#include "blackjack.h"
#include "events.h"
#include "draw.h"
#include "chips.h"
#include "splash.h"

#include "player.h"
#include "hand.h"
#include "game.h"

#include <iostream>
using namespace std;

BJGameRules      *rules = NULL;
LoadablePlayer   *strategy = NULL;
Hand             *dealer = NULL;
Probabilities    *dealerProbabilities = NULL;
Shoe             *shoe = NULL;
BJShoe           *distribution = NULL;
PlayerHand       *player = NULL;

GList            *playerHands = NULL;
gfloat           lastWager = 5.0;
Card             tempCard;
PlayerHand       *tempHand = NULL;

gint             numDecks;
gint             dealerSpeed;

gint             numHands;

gchar            *game_file = "";
gchar            *game_name;

gboolean         allSettled = false;
gboolean         game_is_done = false;
gboolean         first_hand = true;

GList *rules_list = NULL;

BJGameRules::BJGameRules (bool hitSoft17, bool doubleAnyTotal, 
                          bool double9, bool doubleSoft, 
                          bool doubleAfterHit, bool doubleAfterSplit,
                          bool resplit, bool resplitAces, 
                          bool lateSurrender, int numDecks, int dealerSpeed)
  : BJRules (hitSoft17, doubleAnyTotal, 
             double9, doubleSoft, 
             doubleAfterHit, doubleAfterSplit,
             resplit, resplitAces, 
             lateSurrender)
{
  this->numDecks = numDecks;
  this->dealerSpeed = dealerSpeed;
}

int
BJGameRules::getDealerSpeed ()
{
  return dealerSpeed;
}

int
BJGameRules::getNumDecks ()
{
  return numDecks;
}

void
bj_game_show_hand_counts ()
{
  GList *temptr;
  for (temptr = playerHands; temptr; temptr = temptr->next)
    ((PlayerHand*)temptr->data)->showCount ();
  if (! bj_game_is_active ())
    dealer->showCount ();
}

gchar *
bj_game_file_to_name (const gchar* file)
{
  char *p, *buf = g_path_get_basename (file);

  if ((p = strrchr (buf, '.'))) *p = '\0';
  for (p = buf; p = strchr (p, '_'), p && *p;) *p = ' ';
  for (p = buf; p = strchr (p, '-'), p && *p;) *p = ' ';
  buf[0] = toupper (buf[0]);
  p = g_strdup (_(buf));

  g_free (buf);
  return p;
}

int
bj_is_ruleset (const gchar *file_name)
{
  return (!strcmp (g_extension_pointer (file_name), "rules"));
}

void
bj_game_find_rules (gchar *variation)
{
  GDir *dir;
  gchar *dname;
  G_CONST_RETURN gchar* file_name;
  gint n_games = 0;

  dname = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_APP_DATADIR,
                                     BJ_RULES_DIR, FALSE, NULL);

  dir = g_dir_open (dname, 0, NULL);
  if (dir == NULL)
    return;
  
  g_free (dname);

  while ((file_name = g_dir_read_name (dir)) != NULL) 
    {
      if (! bj_is_ruleset (file_name))
        continue;

      n_games++;
      rules_list = g_list_append (rules_list, g_strdup (file_name));
      if (! g_ascii_strcasecmp (variation, file_name)) 
        {
          bj_set_game_variation (file_name);
        }
    }

  g_dir_close (dir);
}

GList *
bj_game_get_rules_list ()
{
  return rules_list;
}

char *
bj_game_get_rules_name ()
{
  return game_name;
}

char *
bj_game_get_rules_file ()
{
  return game_file;
}

gboolean
bj_game_is_active ()
{
  return (! (game_is_done || first_hand));
}

gboolean
bj_game_is_first_hand ()
{
  return (first_hand);
}

void
bj_game_set_active (gboolean value)
{
  if (value)
    {
      game_is_done = false;
      first_hand = false;
    }
  else
    {
      game_is_done = true;
    }
}

gdouble
bj_get_deal_delay ()
{
  return (bj_get_quick_deal ()) ? 1 : dealerSpeed;
}


BJGameRules *
bj_game_read_rules (gchar *filename)
{
  BJGameRules *ruleset;
  gboolean used_default;
  gboolean hitSoft17,
    doubleAnyTotal,
    double9,
    doubleSoft,
    doubleAfterHit,
    doubleAfterSplit,
    resplit,
    resplitAces,
    lateSurrender;
  gint numDecks,
    dealerSpeed;
  
  gnome_config_pop_prefix ();
  gnome_config_push_prefix (g_strdup_printf ("=%s=", filename));

  numDecks = gnome_config_get_int_with_default 
    ("General/Number Of Decks=6",
     &used_default);
  hitSoft17 = gnome_config_get_bool_with_default 
    ("House Rules/Dealer Hits Soft 17=false",
     &used_default);
  doubleAnyTotal = gnome_config_get_bool_with_default 
    ("House Rules/Double Down Any Total=true",
     &used_default);
  double9 = gnome_config_get_bool_with_default 
    ("House Rules/Double Down 9=true",
     &used_default);
  doubleSoft = gnome_config_get_bool_with_default
    ("House Rules/Double Down Soft=true",
     &used_default);
  doubleAfterHit = gnome_config_get_bool_with_default
    ("House Rules/Double Down After Hit=false",
     &used_default);
  doubleAfterSplit = gnome_config_get_bool_with_default
    ("House Rules/Double Down After Split=true",
     &used_default);
  resplit = gnome_config_get_bool_with_default
    ("House Rules/Resplit Allowed=true",
     &used_default);
  resplitAces = gnome_config_get_bool_with_default
    ("House Rules/Resplit Aces Allowed=true",
     &used_default);
  lateSurrender = gnome_config_get_bool_with_default
    ("House Rules/Surrender Allowed=true",
     &used_default);
  dealerSpeed = gnome_config_get_int_with_default
    ("House Rules/Dealer Speed=500",
     &used_default);

  // Compute basic strategy.
  ruleset = new BJGameRules (hitSoft17, doubleAnyTotal, 
                             double9, doubleSoft, 
                             doubleAfterHit, doubleAfterSplit,
                             resplit, resplitAces, 
                             lateSurrender, numDecks, dealerSpeed);
  
  return ruleset;
}


BJGameRules *
bj_game_find_and_read_rules (gchar *filename)
{
  gchar *installed_filename;
  gchar *relative;
  BJGameRules *ruleset = NULL;

  relative = g_build_filename (BJ_RULES_DIR, filename, NULL);
  installed_filename = gnome_program_locate_file (NULL, 
                                                  GNOME_FILE_DOMAIN_APP_DATADIR,
                                                  relative,
                                                  FALSE, NULL);

  if (g_file_test (installed_filename, G_FILE_TEST_EXISTS))
    ruleset = bj_game_read_rules (installed_filename);

  g_free (installed_filename);
  g_free (relative);

  return ruleset;
}

static gchar *
bj_game_get_config_dir (void)
{
  gchar *conf_dir = NULL;

  conf_dir = g_build_filename (gnome_user_dir_get (),
                               "blackjack.d",
                               NULL);
  return conf_dir;
}

static void
bj_game_ensure_config_dir_exists (const char *dir)
{
  if (g_file_test (dir, G_FILE_TEST_IS_DIR) == FALSE)
    {
      if (g_file_test (dir, G_FILE_TEST_EXISTS) == TRUE)
        {
          // FIXME: use a dialog
          cerr << dir << " exists, please move it out of the way." << endl;
        }
      if (mkdir (dir, 488) != 0)
        {
          cerr << "Failed to create directory " << dir << endl;
        }
    }
}

static void
bj_game_eval_installed_file (gchar *file)
{
  char *installed_filename;
  char *relative;

   if (g_file_test (file, G_FILE_TEST_EXISTS))
     {
       return;
     }
  
  relative = g_build_filename (BJ_RULES_DIR, file, NULL);
  installed_filename = gnome_program_locate_file (NULL, 
                                                  GNOME_FILE_DOMAIN_APP_DATADIR,
                                                  relative,
                                                  FALSE, NULL);

  if (g_file_test (installed_filename, G_FILE_TEST_EXISTS))
    {
      rules = bj_game_read_rules (installed_filename);
      g_free (installed_filename);

      // set globals
      numDecks = rules->getNumDecks ();
      dealerSpeed = rules->getDealerSpeed ();

      BJStrategy maxValueStrategy;
      Progress progress;

      gchar *cache_filename = g_strdup_printf ("%s%s", file, ".dat");

      char *config_dir = bj_game_get_config_dir ();
      bj_game_ensure_config_dir_exists (config_dir);
      installed_filename = g_build_filename (config_dir, cache_filename, NULL);
      g_free (config_dir);

      gboolean use_cache = false;
      if (g_file_test (installed_filename, G_FILE_TEST_EXISTS))
        use_cache = true;

      if (! use_cache)
        splash_new ();
      strategy = new LoadablePlayer (numDecks, rules, 
                                     maxValueStrategy, 
                                     progress, 
                                     (use_cache) ? installed_filename : NULL);

      if (! use_cache)
        {
          strategy->saveXML (installed_filename);
          splash_destroy ();
        }

      g_free (cache_filename);
     } 
  else
    {
      gchar *message = g_strdup_printf ("%s\n %s", _("Blackjack can't load the requested file"),
                                         installed_filename);
      gchar *message2 = _("Please check your Blackjack installation");
      GtkWidget *w = gtk_message_dialog_new (GTK_WINDOW (app),
                                             GTK_DIALOG_DESTROY_WITH_PARENT,
                                             GTK_MESSAGE_ERROR,
                                             GTK_BUTTONS_CLOSE,
                                             "<span weight=\"bold\" size=\"larger\">%s</span>\n\n%s",
                                             message, message2);
      gtk_label_set_use_markup (GTK_LABEL (GTK_MESSAGE_DIALOG (w)->label), TRUE);
      gtk_dialog_run (GTK_DIALOG (w));
      gtk_widget_destroy (w);
      g_free (message);
      exit (1);
  }
  g_free (installed_filename);
  g_free (relative);
}

void 
bj_game_cancel ()
{
  bj_hand_cancel ();
  bj_game_set_active (FALSE);
}

void
bj_game_new (gchar* file, guint *seedp )
{
  gint min_w, min_h;

  bj_game_cancel ();

  first_hand = true;

  bj_show_balance (bj_get_balance ());

  if (file && strcmp (file, game_file))
    {
      game_file = file;

      bj_game_eval_installed_file (file);
      game_name = bj_game_file_to_name (file);
      
      if (option_dialog)
        {
          gtk_widget_destroy (option_dialog);
          option_dialog = NULL;
        }

      bj_set_game_variation (file);
    }

  if (seedp)
    seed = *seedp;
  else
    seed = g_random_int ();

  g_random_set_seed (seed);

  min_w = 600;
  min_h = 400;
  gtk_widget_set_size_request (playing_area, min_w, min_h);
  if (surface)
    bj_draw_refresh_screen ();

  // Prepare to play blackjack.

  if (dealer)
    delete dealer;
  dealer = new Hand;
  dealer->hslot = NULL;
  if (dealerProbabilities)
    delete dealerProbabilities;
  dealerProbabilities = new Probabilities (rules->getHitSoft17 ());
  if (shoe)
    delete shoe;
  shoe = new Shoe (numDecks);
  if (distribution)
    delete distribution;
  distribution = new BJShoe (numDecks);

  bj_clear_table ();

  bj_make_window_title (game_name, seed);
}

void
bj_clear_table ()
{
  GList* temptr;

  // delete all slots except 0,1
  delete_surface ();
  numHands = 1;

  for (temptr = playerHands; temptr; temptr = temptr->next)
    g_free (temptr->data);
  g_list_free (playerHands);
  playerHands = NULL;
  player = (PlayerHand*)g_malloc (sizeof (PlayerHand));
  playerHands = g_list_append (playerHands, player);
  
  // Create slots
  bj_slot_add (0, DEALER_SLOT_ORIGIN_X, DEALER_SLOT_ORIGIN_Y);
  bj_slot_add (1, PLAYER_SLOT_ORIGIN_X, PLAYER_SLOT_ORIGIN_Y);

  // Clear the table.
  dealer->hslot = (hslot_type) g_list_nth_data (slot_list, 0);
  player->hslot = (hslot_type) g_list_nth_data (slot_list, 1);

  bj_chip_stack_new_with_value (bj_get_wager (),
                                player->hslot->x - bj_chip_get_width () - 5,
                                player->hslot->y + bj_card_get_height () / 2);

  // Create source chip stacks 
  bj_chip_stack_create_sources ();

  player->nextHand = NULL;
  player->reset ();
  dealer->reset ();
  dealerProbabilities->reset ();

  first_hand = true;
}

