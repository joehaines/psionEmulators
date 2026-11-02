#ifndef BASE_SETTINGS_H
#define BASE_SETTINGS_H

#include <qwidget.h>
#include <kconfig.h>

class QGridLayout;


//-----------------------------------------------------------------------------
class BaseAppearanceConfig : public QWidget
{
    Q_OBJECT
public:
    BaseAppearanceConfig();

protected:
    QWidget     *_main;
    QGridLayout *_grid;
};

//-----------------------------------------------------------------------------
class ColorConfig : public QWidget
{
    Q_OBJECT
public:
    ColorConfig();

private:
    static QCString colorKey(uint i);
};

#endif
