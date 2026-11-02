/****************************************************************
Copyright (c) 1998 Sandro Sigala <ssigala@globalnet.it>.
All rights reserved.

Permission to use, copy, modify, and distribute this software
and its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
copies and that both that the copyright notice and this
permission notice and warranty disclaimer appear in supporting
documentation, and that the name of the author not be used in
advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

The author disclaim all warranties with regard to this
software, including all implied warranties of merchantability
and fitness.  In no event shall the author be liable for any
special, indirect or consequential damages or any damages
whatsoever resulting from loss of use, data or profits, whether
in an action of contract, negligence or other tortious action,
arising out of or in connection with the use or performance of
this software.
****************************************************************/

#include "config.h"

#include <kapplication.h>
#include <klocale.h>
#include <kcmdlineargs.h>
#include <kaboutdata.h>

#include "gamewindow.h"


static const char description[] = I18N_NOOP("KDE SmileTris");


int main(int argc, char **argv)
{
	KAboutData aboutData( "ksmiletris", I18N_NOOP("KSmileTris"), 
		VERSION, description, KAboutData::License_GPL, 
		"(c) 1998, Sandro Sigala");
	aboutData.addAuthor("Sandro Sigala",0, "ssigala@globalnet.it");
	KCmdLineArgs::init( argc, argv, &aboutData );

	
	KApplication app;
	KGlobal::locale()->insertCatalogue("libkdegames");

	GameWindow *w = new GameWindow;
	app.setMainWidget(w);
	w->show();
	return app.exec();
}
