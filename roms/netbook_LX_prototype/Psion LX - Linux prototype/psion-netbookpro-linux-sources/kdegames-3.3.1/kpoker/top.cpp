/*
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */


// QT includes
#include <qkeycode.h>
#include <qcursor.h>

// KDE includes
#include <kapplication.h>
#include <kmenubar.h>
#include <kstatusbar.h>
#include <kmessagebox.h>
#include <kconfig.h>
#include <klocale.h>
#include <kstdaccel.h>
#include <kstdaction.h>
#include <kstdgameaction.h>
#include <kaction.h>
#include <kshortcut.h>

// own includes
#include "top.h"
#include "kpoker.h"
#include "defines.h"
#include "version.h"


PokerWindow::PokerWindow()
{
  m_kpok = new kpok(this, 0);
  setCentralWidget( m_kpok );
  m_kpok->show();

  clickToHoldIsShown = false;

  LHLabel = new QLabel(statusBar());
  LHLabel->adjustSize();

  connect(m_kpok, SIGNAL(changeLastHand(QString, bool)), 
	  this,   SLOT(setHand(QString, bool)));
  connect(m_kpok, SIGNAL(showClickToHold(bool)), 
	  this,   SLOT(showClickToHold(bool)));
  connect(m_kpok, SIGNAL(statusBarMessage(QString)), 
	  this,   SLOT(statusBarMessage(QString)));

  statusBar()->addWidget(LHLabel, 0, true);
  m_kpok->updateLHLabel();
  //FIXME: LHLabel is shown twize until the bar is repainted!

  initKAction();
  readOptions();
}


PokerWindow::~PokerWindow()
{
}


// ----------------------------------------------------------------


void PokerWindow::initKAction()
{
  //Game
  KStdGameAction::gameNew(m_kpok, SLOT(newGame()), actionCollection());
  KStdGameAction::save(m_kpok, SLOT(saveGame()), actionCollection());
  KStdGameAction::quit(this, SLOT(close()), actionCollection());

  //Settings
  showMenubarAction =
    KStdAction::showMenubar(this, SLOT(toggleMenubar()), actionCollection());
   
  soundAction = new KToggleAction(i18n("Soun&d"), 0, m_kpok,
				  SLOT(toggleSound()), actionCollection(), "options_sound");
  if (m_kpok->getSound())
    m_kpok->toggleSound();
  blinkingAction = new KToggleAction(i18n("&Blinking Cards"), 0, m_kpok,
				     SLOT(toggleBlinking()), actionCollection(), "options_blinking");
  if (m_kpok->getBlinking())
    m_kpok->toggleBlinking();
  adjustAction = new KToggleAction(i18n("&Adjust Bet is Default"), 0,
				   m_kpok, SLOT(toggleAdjust()), actionCollection(), "options_adjust");
  if (m_kpok->getAdjust())
    m_kpok->toggleAdjust();

  showStatusbarAction =
    KStdAction::showStatusbar(this, SLOT(toggleStatusbar()), actionCollection());

  KStdAction::saveOptions(this,     SLOT(saveOptions()), actionCollection());
  KStdGameAction::carddecks(m_kpok, SLOT(slotCardDeck()), actionCollection());
  KStdAction::preferences(m_kpok,   SLOT(slotPreferences()), actionCollection());

  // Keyboard shortcuts.
  (void)new KAction(i18n("Draw"), KShortcut(Qt::Key_Return), m_kpok,
		    SLOT(drawClick()), actionCollection(), "draw");
  (void)new KAction(i18n("Exchange Card 1"), KShortcut(Qt::Key_1), m_kpok,
		    SLOT(exchangeCard1()), actionCollection(), "exchange_card_1");
  (void)new KAction(i18n("Exchange Card 2"), KShortcut(Qt::Key_2), m_kpok,
		    SLOT(exchangeCard2()), actionCollection(), "exchange_card_2");
  (void)new KAction(i18n("Exchange Card 3"), KShortcut(Qt::Key_3), m_kpok,
		    SLOT(exchangeCard3()), actionCollection(), "exchange_card_3");
  (void)new KAction(i18n("Exchange Card 4"), KShortcut(Qt::Key_4), m_kpok,
		    SLOT(exchangeCard4()), actionCollection(), "exchange_card_4");
  (void)new KAction(i18n("Exchange Card 5"), KShortcut(Qt::Key_5), m_kpok,
		    SLOT(exchangeCard5()), actionCollection(), "exchange_card_5");

  setupGUI( KMainWindow::Save | StatusBar | Keys | Create);
}


void PokerWindow::readOptions()
{
  KConfig* conf = kapp->config();
  conf->setGroup("General");

  if (m_kpok->getSound() != conf->readBoolEntry("Sound", true))
    soundAction->activate();
  if (m_kpok->getBlinking() != conf->readBoolEntry("Blinking", true))
    blinkingAction->activate();
  if (m_kpok->getAdjust() != conf->readBoolEntry("Adjust", true))
    adjustAction->activate();
  if ( showMenubarAction->isChecked() !=
       conf->readBoolEntry("ShowMenubar", true))
    showMenubarAction->activate();
  if ( showStatusbarAction->isChecked() !=
       conf->readBoolEntry("ShowStatusbar", true))
    showStatusbarAction->activate();
}


void PokerWindow::toggleMenubar()
{
  if (!menuBar()->isHidden())
    menuBar()->hide();
  else
    menuBar()->show();
}


void PokerWindow::toggleStatusbar()
{
  if (!statusBar()->isHidden())
    statusBar()->hide();
  else
    statusBar()->show();
}


/* Ask the user if he/she wants to save the game.  This virtual method
 * is called from the Quit KAction (I think).
 */

bool PokerWindow::queryClose()
{
  if (!m_kpok->isDirty())
    return true;

  // Only ask if the game is changed in some way.
  switch(KMessageBox::warningYesNoCancel(this, i18n("Do you want to save this game?"))) {
  case KMessageBox::Yes :
    m_kpok->saveGame();
    return true;
  case KMessageBox::No :
    return true;
  default :
    return false;
  }
}


/* Show the hand or winner in the status bar at the lower right.
 *
 * FIXME: This doesn't seem to be used as setHand any more.  It is always
 *        used as showWinner() instead, and should be renamed accordingly.
 */

void PokerWindow::setHand(QString newHand, bool lastHand)
{
  if (lastHand)
    LHLabel->setText(i18n("Last hand: ") + newHand);
  else
    LHLabel->setText(i18n("Last winner: ") + newHand);
  LHLabel->adjustSize();
}


void PokerWindow::showClickToHold(bool show)
{
  if (show) {
    statusBar()->clear();
    statusBar()->message(i18n("Click a card to hold it"));
    clickToHoldIsShown = true;
  } else if (clickToHoldIsShown) {
    statusBar()->clear();
    clickToHoldIsShown = false;
  }
}


void PokerWindow::statusBarMessage(QString s)
{
  clearStatusBar();
  statusBar()->message(s);
  clickToHoldIsShown = false;
}


void PokerWindow::clearStatusBar()
{
  if (!clickToHoldIsShown)
    statusBar()->clear();
}


void PokerWindow::saveOptions()
{
  KConfig* conf = kapp->config();
  conf->setGroup("General");

  conf->writeEntry("Sound",         soundAction->isChecked());
  conf->writeEntry("Blinking",      blinkingAction->isChecked());
  conf->writeEntry("Adjust",        adjustAction->isChecked());
  conf->writeEntry("ShowMenubar",   showMenubarAction->isChecked());
  conf->writeEntry("ShowStatusbar", showStatusbarAction->isChecked());
}


bool PokerWindow::eventFilter(QObject*, QEvent* e)
{
  if (e->type() == QEvent::MouseButtonPress) {

    if (((QMouseEvent*)e)->button() == RightButton) {
      QPopupMenu*  popup = (QPopupMenu*) factory()->container("popup", this);
      if (popup)
	popup->popup(QCursor::pos());
      return true;
    } else
      return false;
  }

  return false;
}

#include "top.moc"

