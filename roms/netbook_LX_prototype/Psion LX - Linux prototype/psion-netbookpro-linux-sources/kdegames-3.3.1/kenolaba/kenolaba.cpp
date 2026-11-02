/* Start point */

#include <qobject.h>

#include <kapplication.h>
#include <klocale.h>
#include <kglobal.h>
#include <kcmdlineargs.h>

#include <kaboutdata.h>

#include "version.h"
#include "AbTop.h"


static const char description[] =
	I18N_NOOP("Board game inspired by Abalone");

static KCmdLineOptions options[] =
{
  { "h", 0, 0},
  { "host <host>", I18N_NOOP("Use 'host' for network game"), 0 },
  { "p", 0, 0},
  { "port <port>", I18N_NOOP("Use 'port' for network game"), 0 },
  KCmdLineLastOption
};

AbTop *create(KCmdLineArgs *args)
{
    AbTop* top = new AbTop;
	if (args->isSet("port"))
	    top->netPort( args->getOption("port").toInt() );
	if (args->isSet("host"))
	    top->netHost( args->getOption("host").data() );
    top->readConfig();
    return top;
}

int main(int argc, char *argv[])
{
	KAboutData aboutData( "kenolaba", I18N_NOOP("Kenolaba"),
		KENOLABA_VERSION, description, KAboutData::License_GPL,
		"(c) 1997-2000, Josef Weidendorfer");
	aboutData.addAuthor("Josef Weidendorfer",0, "Josef.Weidendorfer@gmx.de");
	aboutData.addAuthor("Robert Williams");
	KCmdLineArgs::init( argc, argv, &aboutData );
	KCmdLineArgs::addCmdLineOptions( options ); // Add our own options.

	KApplication app;
	KGlobal::locale()->insertCatalogue("libkdegames");

	/* command line handling */
	KCmdLineArgs *args = KCmdLineArgs::parsedArgs();

	/* session management */
	if ( app.isRestored() ) {
        uint n = 1;
        while ( KMainWindow::canBeRestored(n) ) {
            AbTop *top = create(args);
            top->restore(n);
            n++;
        }
    } else {
        AbTop *top = create(args);
        app.setMainWidget(top);
        top->show();
    }
    args->clear();
	return app.exec();
}

