#ifndef COMMON_SETTINGS_H
#define COMMON_SETTINGS_H

#include "base/settings.h"


//-----------------------------------------------------------------------------
class AppearanceConfig : public BaseAppearanceConfig
{
    Q_OBJECT
public:
    AppearanceConfig();
};

//-----------------------------------------------------------------------------
class GameConfig : public QWidget
{
    Q_OBJECT
public:
    GameConfig();

protected:
    QGridLayout *_grid;
};

#endif
