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


#ifndef PLAYERBOX_H
#define PLAYERBOX_H


#include <qgroupbox.h>


class QLabel;


class PokerPlayer;
class CardWidget;


class PlayerBox : public QGroupBox
{
  Q_OBJECT

  public:
    PlayerBox(bool playerOne, QWidget* parent = 0, const char* name = 0);
    ~PlayerBox();

    void cardClicked(int no);
	
    /**
      * Show the name of the player who owns this box. Use m_player to
      * get the name.
     **/
    void showName();
	
    /**
      * Paints the cash
     **/
    void showCash(); // and some more

    /**
      * Sets the player which is used e.g. by @ref showCash()
      * @param p The guy who owns this box
     **/
    void setPlayer(PokerPlayer* p)	{ m_player = p; }
	
    /**
      * Hides the card nr (0 = first card)
      *
      * Used to let the cards blink.
      * @param nr The number of the card which will be hidden
     **/
    void hideCard(int nr);

    /**
      * @param nr The number of the card (where 0 = first card)
      * @return If the card nr shall be held
     **/
    bool getHeld(int nr) const;
	
    /**
      * shows all held labels or hides them all
      * @param on Shows all all labels if true and hides them if false
     **/
    void showHelds(bool on);
	
    /**
      * Enables the held labels if on is true or disables them
      * (e.g. after exchange phase) if false
      * @param e Enables held labels if true, disables if false
     **/
    void setHeldEnabled(bool on);
	
    // FIXME: Combine these two into paintCard(int nr, bool showFront);
    /**
      * Paints the card nr
      * @param nr The number of the card (where 0 = first card)
     **/
    void paintCard(int nr);
	
    /**
     * Paints the deck
     * @param nr The number of the card (where 0 = first card)
    **/
    void paintDeck(int nr);
	
    /**
      * Starts a one player game
      * @param newCashPerRound The cash that the player has to pay every round
     **/
    void singlePlayerGame(int newCashPerRound);
	
    /**
      * Activates the held labels for this player (human player)
     **/
    void activateToggleHeld();

    /**
      * Begins blinking of the winning cards
     **/
    void blinkOn();	
	
    /**
      * Stops blinking of the winning cards
     **/
    void blinkOff();
	
    void repaintCard();


  protected:
    virtual void resizeEvent( QResizeEvent* e );


  protected slots:
    void cardClicked(CardWidget* );


  signals:
    void toggleHeld();

  private:
    // The player that acts on the hand.
    PokerPlayer  *m_player;		// The player object. 

    // Properties of the game
    bool     m_singlePlayer;	// True if this is the only player in the game.
    int      m_cashPerRound;	// one player game only  FIXME: Rename into "ante"?

    // The card widgets and "held" widgets
    CardWidget  **m_cardWidgets;
    QLabel      **m_heldLabels;
    bool          m_enableHeldLabels; // True if held labels are enabled.

    // The labels at the right hand side of the box.
    QLabel  *m_cashLabel;
    QLabel  *m_betLabel;


};
#endif

