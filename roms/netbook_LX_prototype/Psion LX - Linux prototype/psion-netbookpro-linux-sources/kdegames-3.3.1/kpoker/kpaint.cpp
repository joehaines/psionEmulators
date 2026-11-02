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
#include <qdir.h>
#include <qlayout.h>

// KDE includes
//#include <kglobal.h>
#include <kdebug.h>
#include <kcarddialog.h>

// own includes
#include "defines.h"
#include "poker.h"
#include "kpaint.h"


// ================================================================
//                          class CardImages


QPixmap  *CardImages::m_cardPixmaps;
QPixmap  *CardImages::m_deck;


CardImages::CardImages(QWidget* parent, const char* name)
  : QWidget(parent, name)
{
  m_cardPixmaps = new QPixmap[numCards];
  m_deck        = new QPixmap;

  // Hide the window.
  // FIXME: Why is this a QWidget?
  hide();
  // loadCards();
}


CardImages::~CardImages()
{
  delete[] m_cardPixmaps;
  delete   m_deck;
}


QPixmap *
CardImages::getCardImage(int card) const
{
  if (card == 0)
    return m_deck;
  else
    return &m_cardPixmaps[card-1];
}


// Load all the card images from the directory 'cardDir'.

void
CardImages::loadCards(QString cardDir)
{
  for (int i = 0; i < numCards; i++) {
    QString card = KCardDialog::getCardPath(cardDir, i + 1);

    if (card.isEmpty() || !m_cardPixmaps[i].load(card)) {
      if (!card.isEmpty())
	kdWarning() << "Could not load " << card << " trying default" << endl;
      card = KCardDialog::getCardPath(KCardDialog::getDefaultCardDir(), i+1);
      if (!m_cardPixmaps[i].load(card)) {
	kdError() << "Could not load " << card << endl;
      }
    }
  }
}


// Load the backside of the card deck from the file name in 'path'.

void
CardImages::loadDeck(QString path)
{
  if (!m_deck->load(path)) {
    kdWarning() << "Could not load deck - loading default deck" << endl;
    path = KCardDialog::getDefaultDeck();
    if (!m_deck->load(path))
      kdError() << "Could not load deck" << endl;
  }
}


// ================================================================
//                          class CardWidget


extern CardImages  *cardImages;


CardWidget::CardWidget( QWidget *parent, const char *name )
  : QPushButton( parent, name )
{
  m_held = false;

  setBackgroundMode( NoBackground ); // disables flickering
  connect(this, SIGNAL(clicked()), this, SLOT(ownClick()));

  setFixedSize(cardWidth, cardHeight);
}


void CardWidget::paintCard(int cardType)
{
  // Remap the card from the natural poker card values to the names
  // used by the card decks in KDE.
  //
  // FIXME: This is an ugly hack.  The correct way would be to add a
  //        method paintCard(CardValue card), but that will have to 
  //        wait until we break out the card stuff to its own file so 
  //        that there is something to include.
  int  card;
  int  rank;
  int  suit;

  if (cardType == 0)
    card = 0;
  else {
    suit = (cardType - 1) % 4;
    rank = (cardType - 1) / 4;

    rank = 12 - rank;	// ace-two --> two-ace
    switch (suit) {
    case 0:           break; // Clubs
    case 1: suit = 3; break; // Diamonds
    case 2: suit = 1; break; // Spades
    case 3: suit = 2; break; // Hearts
    }
    card = rank * 4 + suit + 1;
  }

  // Select the pixmap to use.
#if 0
  if (card == 0) {
    m_pm = &cardImage->m_deck;
  } else {
    m_pm = &cardImage->m_cardPixmaps[card-1];
  }
#else
  m_pm = cardImages->getCardImage(card);
#endif

  // Set the pixmap in the QPushButton that we inherit from.
  if ( m_pm->size() != QSize( 0, 0 ) ) {   // is an image loaded?
    setPixmap(*m_pm);
  }
}


void CardWidget::repaintDeck()
{ 
  setPixmap(*m_pm); 
  setFixedSize(cardImages->getWidth(), cardImages->getHeight());

  ((QWidget*) parent())->layout()->invalidate();
  ((QWidget*) parent())->setFixedSize( ((QWidget*) parent())->sizeHint());
}


/* Emit the pClicked signal.
 */

void CardWidget::ownClick()
{
  emit pClicked(this);
}


bool CardWidget::getHeld()
{
  return m_held;
}


void CardWidget::setHeld(bool newheld)
{
  m_held = newheld;
}


bool CardWidget::toggleHeld()
{
  m_held = !m_held;
  return m_held;
}


#include "kpaint.moc"
