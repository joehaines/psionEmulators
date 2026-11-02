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


#ifndef __CardWidget__
#define __CardWidget__


#include <assert.h>

#include <qpushbutton.h>
#include <qpixmap.h>


class QLabel;


/**
  * This class loads all pictures, first. So they don't have to be loaded after the start again
  * @short A help class for loading the pictures
 **/

class CardImages : public QWidget
{
  Q_OBJECT

  public:
    CardImages( QWidget *parent = 0, const char *name = 0 ); 
    ~CardImages();

    // FIXME: Use CardValue instead of int when the cards are in their
    //        own file.
    QPixmap  *getCardImage(int card) const;
    QPixmap  *getDeck() const    { return m_deck; }

    int       getWidth() const   { return m_cardPixmaps[0].width();  }
    int       getHeight() const  { return m_cardPixmaps[0].height(); }
	
    void      loadDeck(QString path);
    void      loadCards(QString cardDir);

  private:
    static QPixmap  *m_cardPixmaps;
    static QPixmap  *m_deck;
};


/**
  * This class extends the QPushButton by some methods / variables to provide a card with held labels and so on
  *
  * @short The cards
 **/
class CardWidget : public QPushButton
{
  Q_OBJECT
	    
  public:
    CardWidget( QWidget *parent=0, const char *name=0 );
	
    /**
     * Paints the deck if cardType = 0 or the card specified in cardType
     * @param cardType the card to be painted. 0 is the deck
     **/
    void        paintCard(int cardType);
	
    /**
     * @return The held status of this card
     **/
    bool        getHeld();
	
    /**
     * Sets the new held
     * @param newHeld specifies the new held
     **/
    void        setHeld(bool newheld);
	
    /**
     * Toggle the boolean member m_held.
     **/
    bool        toggleHeld(); /* returns the new value of held*/


    void repaintDeck();
	
  signals:	
    /**
     * This signal is emitted by @ref ownClick()
     * @param CardWidget is a this pointer
     **/
    void pClicked(CardWidget *);
	
  protected slots:
    /**
     * Emits the signal @ref pClicked() when the player clicks on the card
     **/
    void ownClick();
	

  private:
    QPixmap  *m_pm;                     // the loaded pixmap
    bool      m_held;

  public:
    QLabel   *heldLabel;
};


#endif
