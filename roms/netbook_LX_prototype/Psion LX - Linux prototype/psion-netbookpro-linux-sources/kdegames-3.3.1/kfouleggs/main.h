#ifndef FE_MAIN_H
#define FE_MAIN_H

#include "common/main.h"
#include "common/factory.h"
#include "board.h"
#include "ai.h"
#include "field.h"


//-----------------------------------------------------------------------------
class FEFactory : public CommonFactory
{
 public:
    FEFactory();

 protected:
    virtual BaseBoard *createBoard(bool graphic, QWidget *parent)
        { return new FEBoard(graphic, parent); }
    virtual BaseField *createField(QWidget *parent)
        { return new FEField(parent); }
    virtual BaseInterface *createInterface(QWidget *parent);
    virtual AI *createAI() { return new FEAI; }
};

//-----------------------------------------------------------------------------
class FEMainWindow : public MainWindow
{
 Q_OBJECT
 public:
    FEMainWindow() { init(); }
};

#endif
