#ifndef COMMON_MAIN_H
#define COMMON_MAIN_H

#include "base/main.h"


class MainWindow : public BaseMainWindow
{
    Q_OBJECT
public:
    MainWindow() {}

protected:
    void init();
    void addConfig(KConfigDialog *);
    void addKeys(KKeyDialog &);
    void saveKeys();
};

#endif
