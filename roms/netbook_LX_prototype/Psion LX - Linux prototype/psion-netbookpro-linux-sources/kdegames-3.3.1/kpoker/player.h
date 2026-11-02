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


#ifndef PLAYER_H
#define PLAYER_H

// QT includes
#include <qstring.h>

// KDE includes
#include <krandomsequence.h>
#include <kdebug.h>

// own includes
#include "poker.h"


// ================================================================
//                           Player classes


class PokerPlayer
{
 public:
  PokerPlayer();
  ~PokerPlayer();

  /**
   * Calculates the money which the computer will bet
   *
   * use @ref changeBet() for human players
   * @param bet The minimum bet that the player has to bet. probably the same as the bet the human player has bet before
   * @param mayRaise Specifies if the player may raise
   * @return The player's bet
   **/
  int bet(int bet, bool mayRaise = true);

  /**
   * Same as @ref bet() but should bet a little bit more if the cards are good enough (they were already exchanged)
   **/
  int raise(int raise);

  /**
   * This method changes the player's bet by betChange
   * @param Specifies the change
   * @return true if successful, false if not
   **/
  bool changeBet(int betChange);

  /**
   * Only used by computer players
   *
   * Calculates which cards shall be exchanged
   * @param skip[] Will be set true if the card shall be exchanged (if the 1st card shall be exchanged so skip[0] will be true)
   **/
  void exchangeCards(bool skip[]);

  /**
   * This method will return all cards to the pile which means that all cards will be set to 0 (=deck)
   **/
  void giveCardsBack();

  /**
   * Returns a card to the pile
   * @param cardNr specifies the card which will be returned to the pile
   **/
  void giveCardBack(int cardNr);

  /**
   * Begins a new round
   **/
  void newRound();

  /**
   * Sets the player's cash to newCash
   * @param newCash The new cash
   **/
  void setCash(int newCash) 		{ m_money = newCash; }

  /**
   * This makes the player human
   **/
  void setHuman()			{ m_isHuman = true; }

  /**
   * Sets a new name
   * @param newName The new name of the player
   **/
  void setName(QString newName) 		{ m_name = newName; }

  /**
   * Informs the player that he is out (or is not out anymore)
   * @param newOut true if player is out or false if player is back to the game
   **/
  void setOut(bool newOut)		{ isOut = newOut; }

  /**
   * Takes a card
   * @param nr The number of the card (0 = first card)
   * @param value The card itself
   **/
  void takeCard(int nr, int value) 	{ 
#if 0
    const char *suitnames[] = {"C_", "D_", "S_", "H_"};
    if (value > 0)
      kdDebug() << "Got card " << suitnames[(value - 1) % 4]
		<< (value - 1) / 4 + 2 << endl;
#endif
    m_hand.setCard(nr, (CardValue) value);
  }

  /**
   * Informs the player about new rules
   * @param min The minimum possible bet
   * @param max The maximum possible bet
   **/
  void setBetDefaults(int min, int max)	{ minBet = min; maxBet = max; }

  /**
   * @param cardNr The number of the card (0 = first card)
   * @return The card
   **/
  CardValue getCard(int cardNr) const		{ return m_hand.getCard(cardNr);}

  PokerHand &getHand()                  { return m_hand; }

  /**
   * @return The money of the player
   **/
  int getCash() const			{ return m_money; }

  /**
   * @return How much the player has bet
   **/
  int getCurrentBet() const 		{ return currentBet; }

  /**
   * Returns the found card at nr
   *
   * The found cards specify the best cards the player has, e.g. if the player has one pair both cards will be found here
   * @param nr The number of the wanted foundCard
   * @return The found card number nr
   **/
  bool      getFoundCard(int nr) const   { return m_hand.getFoundCard(nr); }

  /**
   * @return If the player is human or not
   **/
  bool getHuman() const			{ return m_isHuman; }

  /**
   * @return The name of the player
   **/
  QString getName() const		{ return m_name; }

  // FIXME: Rename to hasFolded?
  /**
   * @return True if the player is out or false if not
   **/
  bool out()				{ return isOut; }


  /**
   * This test the cards of the player; searches for the result
   *
   * Used by @ref sortedResult() and in one player games
   * @return The result (10 = the best, 0 = nothing)
   **/
  PokerHandType testHand() { return m_hand.analyze(); }


 protected:

  /**
   * This sorts the result generated by @ref testHand() a little bit to be used in games with more than one player
   * @return The points of the hand (a royal flush is e.g. 0, a best card is 500 + the best card)
   **/
  int sortedResult();


 private:
  // Basic data:
  QString          m_name;	// The name of the player.
  bool             m_isHuman;	// True if the player is human.

  // The hand itself
  PokerHand       m_hand;

  // The financial situation
  int              m_money;
  int              currentBet;

  // True if we are out of the game (have folded).
  bool             isOut;

  // Properties of the game.
  // FIXME: Move this to the game itself.
  // FIXME: Add a pointer to the poker game object.
  int              minBet;
  int              maxBet;

  // Extra stuff
  KRandomSequence  random;
};

#endif
