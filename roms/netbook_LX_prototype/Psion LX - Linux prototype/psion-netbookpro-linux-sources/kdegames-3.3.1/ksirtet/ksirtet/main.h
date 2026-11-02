#ifndef KS_MAIN_H
#define KS_MAIN_H

#include "common/main.h"
#include "common/factory.h"
#include "ai.h"
#include "settings.h"
#include "board.h"
#include "field.h"


//-----------------------------------------------------------------------------
class KSFactory : public CommonFactory
{
 public:
   KSFactory();

 protected:
    virtual BaseBoard *createBoard(bool graphic, QWidget *parent)
        { return new KSBoard(graphic, parent); }
    virtual BaseField *createField(QWidget *parent)
        { return new KSField(parent); }
    virtual BaseInterface *createInterface(QWidget *parent);
    virtual AI *createAI() { return new KSAI; }
    virtual QWidget *createGameConfig() { return new KSGameConfig; }
};

//-----------------------------------------------------------------------------
class KSMainWindow : public MainWindow
{
 Q_OBJECT
 public:
    KSMainWindow() { init(); }
};

#endif
