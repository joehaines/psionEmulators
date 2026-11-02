/*
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <kapplication.h>
#include <klocale.h>
#include <kcmdlineargs.h>
#include <kaboutdata.h>

#include "version.h"
#include "top.h"

static const char description[] = I18N_NOOP("KDE Poker Game");

int main( int argc, char *argv[] )
{
  KAboutData aboutData("kpoker", I18N_NOOP("KPoker"),
		       KPOKER_VERSION, description, KAboutData::License_GPL,
		       "(c) 1997-2003 Jochen Tuchbreiter, Andreas Beckermann\n"
		       "(c) 2004 Jochen Tuchbreiter, Andreas Beckermann, Inge Wallin",
		       I18N_NOOP("For a full list of credits see helpfile\nAny suggestions, bug reports etc. are welcome"));

  aboutData.addAuthor("Inge Wallin",
		      I18N_NOOP("Code for poker rules"), "inge@lysator.liu.se");
  aboutData.addAuthor("Andreas Beckermann",
		      I18N_NOOP("Current maintainer"), "b_mann@gmx.de");
  aboutData.addAuthor("Jochen Tuchbreiter",
		      I18N_NOOP("Original author"), "whynot@mabi.de");
  KCmdLineArgs::init( argc, argv, &aboutData );

  KApplication myApp;
  KGlobal::locale()->insertCatalogue("libkdegames");

  if (myApp.isRestored())
    RESTORE(PokerWindow)
  else {
    PokerWindow  *MainScreen = new PokerWindow;
    myApp.setMainWidget( MainScreen );
    MainScreen->show();
  }
  return myApp.exec();
}
