#include "factory.h"

#include <kaboutdata.h>
#include <kapplication.h>
#include <kcmdlineargs.h>
#include <kglobal.h>
#include <klocale.h>

#include "settings.h"


BaseFactory *BaseFactory::_self = 0;

BaseFactory::BaseFactory(const MainData &md, const BaseBoardInfo &bi)
    : mainData(md), bbi(bi)
{
    Q_ASSERT( _self==0 );
    _self = this;
    _aboutData =
        new KAboutData(md.appName, md.trName, md.longVersion, md.description,
                       KAboutData::License_GPL,
                       "(c) 1995, Eirik Eng\n(c) 1996-2004, Nicolas Hadacek",
                       0, md.homepage);
    _aboutData->addAuthor("Nicolas Hadacek", 0, "hadacek@kde.org");
    _aboutData->addCredit("Eirik Eng", I18N_NOOP("Core engine"));
}

void BaseFactory::init(int argc, char **argv)
{
    KCmdLineArgs::init(argc, argv, _aboutData);
    (void)new KApplication;
    KGlobal::locale()->insertCatalogue("libkdegames");
    KGlobal::locale()->insertCatalogue("libksirtet");
}

BaseFactory::~BaseFactory()
{
    delete kapp;
    delete _aboutData;
    Q_ASSERT(_self);
    _self = 0;
}

QWidget *BaseFactory::createAppearanceConfig()
{
    return new BaseAppearanceConfig;
}

QWidget *BaseFactory::createColorConfig()
{
    return new ColorConfig;
}
