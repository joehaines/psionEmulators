/* Yo Emacs, this -*- C++ -*-

  Copyright (C) 1999-2001 Jens Hoefkens
  jens@hoefkens.com

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
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include <kapplication.h>
#include <qstring.h>
#include <klocale.h>
#include <kaboutdata.h>
#include <kcmdlineargs.h>

#include "kbg.h"
#include "version.h"

static const char description[] = I18N_NOOP("A Backgammon program for KDE");
static const char notice[]      = I18N_NOOP("This is a graphical backgammon program. It supports "
					   "backgammon games\nwith other players, games against "
					   "computer engines like GNU bg and even\n"
					   "on-line games on the 'First Internet Backgammon Server'.");

/*
 * Main program doesn't do much - more or less standard stuff. Right
 * after creating a KBg object, control is passed to it.
 */
int main(int argc, char *argv[])
{
	KAboutData about(PROG_NAME, I18N_NOOP("KBackgammon"), PROG_VERSION, description,
			 KAboutData::License_GPL, "(C) 1999-2001 Jens Hoefkens", notice,
			 PROG_HOME, "jens@hoefkens.com");

	about.addAuthor ("Jens Hoefkens", I18N_NOOP("Author & maintainer"),
			 "jens@hoefkens.com", "http://www.hoefkens.com/");

	about.addCredit ("Bo Thorsen", I18N_NOOP("Initial anti-aliasing of the board"),
			 "gobo@imada.sdu.dk");

	KCmdLineArgs::init(argc, argv, &about);

	KApplication app;
	KGlobal::locale()->insertCatalogue("libkdegames");

	if (app.isRestored())
		RESTORE(KBg)
	else {
		KBg* widget = new KBg;
		app.setMainWidget(widget);
		widget->readConfig();
		widget->show();
	}
	return app.exec();
}

