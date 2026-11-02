#include "main.h"
#include "main.moc"

#include <kaction.h>
#include <kstdaction.h>
#include <kmenubar.h>
#include <kstdgameaction.h>
#include <kcmenumngr.h>
#include <kkeydialog.h>
#include <klocale.h>
#include <knotifyclient.h>
#include <knotifydialog.h>
#include <kexthighscore.h>
#include <kconfigdialog.h>

#include "inter.h"
#include "factory.h"
#include "settings.h"
#include "baseprefs.h"

BaseMainWindow::BaseMainWindow()
  : KZoomMainWindow(4, 100, 1, "main_window")
{
    KNotifyClient::startDaemon();

    // File & Popup
    KStdGameAction::gameNew(this, SLOT(start()), actionCollection());
    _pause = KStdGameAction::pause(this, SLOT(pause()), actionCollection());
    _pause->setEnabled(false);
    KStdGameAction::highscores(this, SLOT(showHighscores()),
                               actionCollection());
    KStdGameAction::quit(qApp, SLOT(quit()), actionCollection());

    // Settings
    KStdAction::preferences(this, SLOT(configureSettings()),
                            actionCollection());
    KStdAction::keyBindings(this, SLOT(configureKeys()), actionCollection());
    KStdAction::configureNotifications(this, SLOT(configureNotifications()),
                                       actionCollection());
    KStdGameAction::configureHighscores(this, SLOT(configureHighscores()),
                                        actionCollection());

    _inter = bfactory->createInterface(this);
}

void BaseMainWindow::buildGUI(QWidget *widget)
{
    createGUI();
    setCentralWidget(widget);
    init("popup");
}

BaseMainWindow::~BaseMainWindow()
{
    delete _inter;
}

void BaseMainWindow::showHighscores()
{
    _inter->showHighscores(this);
}

void BaseMainWindow::start()
{
    _inter->_start();
}

void BaseMainWindow::pause()
{
    _inter->_pause();
}

void BaseMainWindow::configureHighscores()
{
    KExtHighscore::configure(this);
}

void BaseMainWindow::configureSettings()
{
    if ( !_inter->_isPaused() ) _inter->_pause();
    if ( KConfigDialog::showDialog("settings") ) return;

    KConfigDialog *dialog = new KConfigDialog(this, "settings", BasePrefs::self() );
    QWidget *w = bfactory->createGameConfig();
    if (w) dialog->addPage(w, i18n("Game"), "package_system");
    w = bfactory->createAppearanceConfig();
    if (w) dialog->addPage(w, i18n("Appearance"), "style");
    w = bfactory->createColorConfig();
    if (w) dialog->addPage(w, i18n("Colors"), "colorize");
//    dialog->addPage(new BackgroundConfigWidget, i18n("Background"), "background");
    addConfig(dialog);
    connect(dialog, SIGNAL(settingsChanged()), SIGNAL(settingsChanged()));
    dialog->show();
}

void BaseMainWindow::configureKeys()
{
    KKeyDialog d(true, this);
    addKeys(d);
    d.insert(actionCollection());
    d.configure(false);
    actionCollection()->writeShortcutSettings();
    saveKeys();
}

void BaseMainWindow::configureNotifications()
{
    KNotifyDialog::configure(this);
}

void BaseMainWindow::writeZoomSetting(uint zoom)
{
  BasePrefs::setBlockSize(zoom);
  BasePrefs::writeConfig();
}

uint BaseMainWindow::readZoomSetting() const
{
  return BasePrefs::blockSize();
}

void BaseMainWindow::writeMenubarVisibleSetting(bool visible)
{
  BasePrefs::setMenubarVisible(visible);
  BasePrefs::writeConfig();
}

bool BaseMainWindow::menubarVisibleSetting() const
{
  return BasePrefs::menubarVisible();
}
