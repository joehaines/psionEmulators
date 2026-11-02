#include "main.h"
#include "main.moc"

#include <klocale.h>
#include <kaction.h>
#include <kstdaction.h>
#include <kkeydialog.h>
#include <kconfigdialog.h>

#include "inter.h"
#include "factory.h"

void MainWindow::addConfig(KConfigDialog *dialog)
{
    QWidget *w = cfactory->createAIConfig();
    if (w) dialog->addPage(w, i18n("A.I."), "ai");
}

void MainWindow::init()
{
    Interface *inter = static_cast<Interface *>(_inter);
    inter->normalGame();

    // Modes
    bool ama = ( bfactory->bbi.nbArcadeStages!=0 );
    QString s = (ama ? i18n("&Single Human (Normal)") : i18n("&Single Human"));
    (void)new KAction(s, 0, inter, SLOT(normalGame()),
                      actionCollection(), "mp_single_human");
    if (ama) (void)new KAction(i18n("&Single Human (Arcade)"), 0,
                               inter, SLOT(arcadeGame()),
                               actionCollection(), "mp_arcade");
    (void)new KAction(i18n("Human vs &Human"), 0, inter, SLOT(humanVsHuman()),
                      actionCollection(), "mp_human_vs_human");
    (void)new KAction(i18n("Human vs &Computer"), 0,
                      inter, SLOT(humanVsComputer()),
                      actionCollection(), "mp_human_vs_computer");
    (void)new KAction(i18n("&More..."), 0, inter, SLOT(dialog()),
                      actionCollection(), "mp_more");

    buildGUI(inter);
    connect(this, SIGNAL(settingsChanged()),
            inter, SLOT(settingsChangedSlot()));
}

void MainWindow::addKeys(KKeyDialog &d)
{
    static_cast<Interface *>(_inter)->addKeys(d);
}

void MainWindow::saveKeys()
{
    static_cast<Interface *>(_inter)->saveKeys();
}
