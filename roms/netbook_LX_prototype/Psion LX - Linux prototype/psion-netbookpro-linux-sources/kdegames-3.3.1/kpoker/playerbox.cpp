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


#include <qtooltip.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qhbox.h>

#include <kglobal.h>
#include <klocale.h>
#include <kdebug.h>

#include "player.h"
#include "playerbox.h"
#include "defines.h"
#include "kpaint.h"


PlayerBox::PlayerBox(bool playerOne, QWidget* parent, const char* name)
  : QGroupBox(parent, name)
{
  QHBoxLayout* l = new QHBoxLayout(this, PLAYERBOX_BORDERS, 
				   PLAYERBOX_HDISTANCEOFWIDGETS);

  // The card and "held" label arrays.
  m_cardWidgets = new CardWidget *[PokerHandSize];
  m_heldLabels  = new QLabel *[PokerHandSize];

  QFont myFixedFont;
  myFixedFont.setPointSize(12);

  // Generate the 5 cards
  for (int i = 0; i < PokerHandSize; i++) {
    QVBoxLayout* vl = new QVBoxLayout(0);
    l->addLayout(vl, 0);

    QHBox* cardBox = new QHBox(this);
    vl->addWidget(cardBox, 0);
    cardBox->setFrameStyle(Box | Sunken);
    m_cardWidgets[i] = new CardWidget(cardBox);
    cardBox->setFixedSize(cardBox->sizeHint());

    // Only add the "held" labels if this is the first player (the human one).
    if (playerOne) {
      QHBox* b = new QHBox(this);
      m_heldLabels[i] = new QLabel(b);
      m_heldLabels[i]->setText(i18n("Held"));
      b->setFrameStyle(Box | Sunken);
      b->setFixedSize(b->sizeHint());
      m_cardWidgets[i]->heldLabel = m_heldLabels[i];

      QHBoxLayout* heldLayout = new QHBoxLayout(0);
      heldLayout->addWidget(b, 0, AlignCenter);
      vl->insertLayout(0, heldLayout, 0);
      vl->insertStretch(0, 1);
      vl->addStretch(1);
    }
  }

  // Add the cash and bet labels.
  {
    QVBoxLayout* vl = new QVBoxLayout;
    l->addLayout(vl);
    vl->addStretch();

    m_cashLabel = new QLabel(this);
    m_cashLabel->setFrameStyle(QFrame::WinPanel | QFrame::Sunken);
    m_cashLabel->setFont(myFixedFont);
    vl->addWidget(m_cashLabel, 0, AlignHCenter);
    vl->addStretch();

    m_betLabel = new QLabel(this);
    m_betLabel->setFrameStyle(QFrame::WinPanel | QFrame::Sunken);
    m_betLabel->setFont(myFixedFont);
    vl->addWidget(m_betLabel, 0, AlignHCenter);
    vl->addStretch();
  }

  QToolTip::add(m_cashLabel,
		i18n("Money of %1").arg("Player"));//change via showName()

  // Assume that we have a multiplayer game.
  m_singlePlayer = false;
}


PlayerBox::~PlayerBox()
{
  delete[] m_cardWidgets;
  delete[] m_heldLabels;
}


// ----------------------------------------------------------------



void PlayerBox::resizeEvent(QResizeEvent* e)
{
  QGroupBox::resizeEvent(e);

  showCash();
  showName();
}


void PlayerBox::showCash()
{
  // Show the amount of cash the player has.
  m_cashLabel->setText(i18n("Cash: %1")
     .arg(KGlobal::locale()->formatMoney(m_player->getCash())));

  // Show how much we have bet during this round.
  if (m_player->out()) 
    m_betLabel->setText(i18n("Out"));
  else {
    if (m_singlePlayer)
      m_betLabel->setText(i18n("Cash per round: %1")
	  .arg(KGlobal::locale()->formatMoney(m_cashPerRound)));
    else
      m_betLabel->setText(i18n("Bet: %1")
          .arg(KGlobal::locale()-> formatMoney(m_player->getCurrentBet())));
  }
}


// Sshow the name of the player.  Suppose that the players name has
// changed.

void PlayerBox::showName()
{
  setTitle(m_player->getName());
  QToolTip::remove(m_cashLabel);
  QToolTip::add(m_cashLabel, i18n("Money of %1").arg(m_player->getName()));
}


// Show or unshow all the held labels depending on the 'on' parameter.

void PlayerBox::showHelds(bool on) 
{
  for (int i = 0; i < PokerHandSize; i++) {
    if (on)
      m_cardWidgets[i]->heldLabel->show();
    else {
      m_cardWidgets[i]->heldLabel->hide();
      m_cardWidgets[i]->setHeld(on);
    }
  }
}


void PlayerBox::paintCard(int nr)
{
  m_cardWidgets[nr]->paintCard(m_player->getCard(nr));
  m_cardWidgets[nr]->show();
}


// Activate the held labels for this player (human player).

void PlayerBox::activateToggleHeld()
{
  for (int i = 0; i < PokerHandSize; i++) {
    connect(m_cardWidgets[i], SIGNAL(pClicked(CardWidget*)),
	    this, SLOT(cardClicked(CardWidget*)));
  }
}


void PlayerBox::cardClicked(CardWidget* MyCW)
{
  emit toggleHeld();
  if (m_enableHeldLabels && MyCW->toggleHeld()) 
    MyCW->heldLabel->show();
  else
    MyCW->heldLabel->hide();
}


void PlayerBox::paintDeck(int nr)
{
  m_player->giveCardBack(nr);
  paintCard(nr);
}


void PlayerBox::blinkOn()
{
  for (int i = 0; i < PokerHandSize; i++) {
    if (m_player->getFoundCard(i)) 
      hideCard(i);
  }
}


void PlayerBox::blinkOff()
{
  for (int i = 0; i < PokerHandSize; i++) {
    if (!m_cardWidgets[i]->isVisible())
      paintCard(i);
  }
}


void PlayerBox::setHeldEnabled(bool on)
{
  m_enableHeldLabels = on;
  if (!on) {
    for (int i = 0; i < PokerHandSize; i++)
      m_heldLabels[i]->hide();
  }
}


void PlayerBox::singlePlayerGame(int newCashPerRound)
{
  m_singlePlayer = true;
  m_cashPerRound = newCashPerRound;
}


void PlayerBox::hideCard(int nr)
{
  m_cardWidgets[nr]->hide();
}


bool PlayerBox::getHeld(int nr) const
{
  return m_cardWidgets[nr]->getHeld();
}


void PlayerBox::cardClicked(int no)
{
  cardClicked(m_cardWidgets[no-1]);
}


void PlayerBox::repaintCard()
{
  for (int i = 0; i < PokerHandSize; i++)
    m_cardWidgets[i]->repaintDeck();
}


#include "playerbox.moc"
