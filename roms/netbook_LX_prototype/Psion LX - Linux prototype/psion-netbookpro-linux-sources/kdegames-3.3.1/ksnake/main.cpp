/**
 * This file is part of the ksnake package
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "game.h"
#include "version.h"
#include <kapplication.h>
#include <kaboutdata.h>
#include <kcmdlineargs.h>

static const char description[] = I18N_NOOP("KDE Snake Race Game");

int main( int argc, char **argv )
{
  KAboutData aboutData( "ksnake", I18N_NOOP("KSnakeRace"),
    KSNAKE_VERSION, description, KAboutData::License_GPL,
			I18N_NOOP("(c) 1997-2000, Your Friendly KSnake Developers"));
  aboutData.addAuthor("Michel Filippi",0, "mfilippi@sade.rhein-main.de");
  aboutData.addAuthor("Robert Williams");
  aboutData.addAuthor("Andrew Chant",0, "andrew.chant@utoronto.ca");
	aboutData.addCredit("André Luiz dos Santos", I18N_NOOP("AI stuff"), "andre@netvision.com.br");
	aboutData.addCredit("Benjamin Meyer", I18N_NOOP("Improvements"), "ben+ksnake@meyerhome.net");
  KCmdLineArgs::init( argc, argv, &aboutData );

  KApplication app;
  KGlobal::locale()->insertCatalogue("libkdegames");

  Game *ksnake = new Game();
  app.setMainWidget( ksnake );
  ksnake->show();
  return app.exec();
}

