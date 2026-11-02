/*
 * KAsteroids - Copyright (c) Martin R. Jones 1997
 *
 * Part of the KDE project
 */
#include <config.h>

#include <kstandarddirs.h>
#include <kcmdlineargs.h>
#include <kapplication.h>
#include <kaboutdata.h>

#include "version.h"
#include "toplevel.h"

#ifdef KA_ENABLE_SOUND
#include <arts/dispatcher.h>
#endif


static const char description[] =
	I18N_NOOP("KDE Space Game");

int main( int argc, char *argv[] )
{
    KAboutData aboutData( "kasteroids", I18N_NOOP("KAsteroids"),
        KASTEROIDS_VERSION, description, KAboutData::License_GPL,
        "(c) 1997, Martin R. Jones");
    aboutData.addAuthor("Martin R. Jones",0, "mjones@kde.org");
    KCmdLineArgs::init( argc, argv, &aboutData );

    KApplication app;
    KGlobal::locale()->insertCatalogue("libkdegames");
    KGlobal::dirs()->addResourceType("sprite", KStandardDirs::kde_default("data") + "kasteroids/sprites/");
    KGlobal::dirs()->addResourceType("sounds", KStandardDirs::kde_default("data") + "kasteroids/sounds/");

#ifdef KA_ENABLE_SOUND
    // setup mcop communication
    Arts::Dispatcher dispatcher;
#endif

    if( app.isRestored() )
        RESTORE(KAstTopLevel)
    else {
        KAstTopLevel *w = new KAstTopLevel;
        app.setMainWidget(w);
        w->show();
    }
    return app.exec();
}

