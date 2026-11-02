#ifndef BASE_FACTORY_H
#define BASE_FACTORY_H

#include <qglobal.h>

struct MainData {
    const char *appName, *trName, *description, *homepage, *removedLabel,
        *version, *longVersion;
};

struct BaseBoardInfo {
    uint width, height;
    bool withPieces;

    uint beforeRemoveTime, afterRemoveTime;
    uint nbToggles, nbFallStages;

    uint nbArcadeStages;

    const uint *histogram;
    uint histogramSize;
    bool scoreBound;
};

class BaseBoard;
class BaseInterface;
class QWidget;
class KAboutData;

#define bfactory BaseFactory::self()

class BaseFactory
{
 public:
    BaseFactory(const MainData &, const BaseBoardInfo &);
    virtual ~BaseFactory();
    void init(int argc, char **argv);

    static BaseFactory *self() { return _self; }

    const MainData      &mainData;
    const BaseBoardInfo &bbi;

    virtual BaseBoard *createBoard(bool graphic, QWidget *parent) = 0;
    virtual BaseInterface *createInterface(QWidget *parent) = 0;

    virtual QWidget *createAppearanceConfig();
    virtual QWidget *createColorConfig();
    virtual QWidget *createGameConfig() { return 0; }
 
 protected:
    KAboutData *_aboutData;

 private:
    static BaseFactory *_self;
};

#endif
