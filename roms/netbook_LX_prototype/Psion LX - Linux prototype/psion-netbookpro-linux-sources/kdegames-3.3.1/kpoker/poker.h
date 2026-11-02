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


#ifndef POKER_H
#define POKER_H

#include <qstring.h>
#include <krandomsequence.h>
#include <kdebug.h>


// ================================================================
//                         Card classes


// FIXME: DECK should be renamed to NO_CARD and moved to -1. (or maybe not)
// FIXME: Add Joker as well.
//

typedef enum {
  DECK=0,
  C_TWO,   D_TWO,   S_TWO,   H_TWO,
  C_THREE, D_THREE, S_THREE, H_THREE,
  C_FOUR,  D_FOUR,  S_FOUR,  H_FOUR,
  C_FIVE,  D_FIVE,  S_FIVE,  H_FIVE,
  C_SIX,   D_SIX,   S_SIX,   H_SIX,
  C_SEVEN, D_SEVEN, S_SEVEN, H_SEVEN,
  C_EIGHT, D_EIGHT, S_EIGHT, H_EIGHT,
  C_NINE,  D_NINE,  S_NINE,  H_NINE,
  C_TEN,   D_TEN,   S_TEN,   H_TEN,
  C_JACK,  D_JACK,  S_JACK,  H_JACK,
  C_QUEEN, D_QUEEN, S_QUEEN, H_QUEEN,
  C_KING,  D_KING,  S_KING,  H_KING,
  C_ACE,   D_ACE,   S_ACE,   H_ACE,
  ROOF				// to get a roof on the value.
} CardValue;

const CardValue   lowestCard  = C_TWO;
const CardValue   highestCard = H_ACE;
const int         numCards = (int) H_ACE;


typedef enum {
  Clubs    = 0,			// The least valuable suit
  Diamonds,
  Spades,
  Hearts			// The most valuable suit
} CardSuit;


typedef enum {
  TWO = 0,			// The least valuable rank
  THREE,
  FOUR,
  FIVE,
  SIX,
  SEVEN,
  EIGHT,
  NINE,
  TEN,
  JACK,
  QUEEN,
  KING,
  ACE				// The most valuable rank
} CardRank;



inline CardValue
operator+(CardValue card, int offset) 
{
  return (CardValue) ((int) card + offset);
}



// Get the suit from a card.

inline CardSuit
suit(CardValue card)
{
  // Need to subtract one since DECK == 0.
  return (CardSuit) (((int) card - 1) % 4);
}


// Get the rank from a card

inline CardRank
rank(CardValue card)
{
  // Need to subtract one since DECK == 0.
  return (CardRank) (((int) card - 1) / 4);
}


// Add an offset (can be negative) to a rank.
//
// NOTE: No overflow check is done.  This is mainly used for type
//       conversion reasons.

inline CardRank
operator+(CardRank rank1, int offset) 
{
  return (CardRank) ((int) rank1 + offset);
}


// Get the first card of a certain rank.  This is the same rank of clubs.

inline CardValue
rank2card(CardRank rank)
{
  return (CardValue) (((int) rank) * 4 + 1);
}

// ----------------------------------------------------------------
//                         Class CardDeck


class CardDeck {
 public:
  CardDeck(KRandomSequence *random);
  ~CardDeck();

  void       reset();
  void       mix();
  CardValue  getTopCard();

 private:
  CardValue        m_cards[numCards];
  int              m_topCard;

  KRandomSequence  *m_random;
};


// ----------------------------------------------------------------
//                         Poker related


typedef enum {
  HighCard,
  Pair,
  TwoPairs,
  ThreeOfAKind,
  Straight,
  Flush,
  FullHouse,
  FourOfAKind,
  StraightFlush,
  RoyalFlush
} PokerHandType;

// Name strings for all the hands
extern QString PokerHandNames[]; 


// Number of cards in the hand.
const int  PokerHandSize = 5;

class  PokerHand {
 public:
  PokerHand();
  ~PokerHand();

  // Operators
  bool  operator<(PokerHand &hand2);

  // Clear the hand or just one card - set the entries in question to DECK.
  void clear();
  void clear(int cardno);

  // Card stuff
  CardValue  getCard(int cardno) const;
  void       setCard(int cardno, CardValue card);
  bool       findRank(CardRank rank) const;
  bool       findSuit(CardSuit suit) const;
  CardValue  findNextBest(CardValue roof, bool onlyFound) const;

  // Poker stuff
  int            testStraight();
  bool           testFlush();
  PokerHandType  analyze();
  CardRank       get_firstRank()  const  { return m_firstRank; }
  CardRank       get_secondRank() const  { return m_secondRank; }

  bool           getFoundCard(int cardNum) const;
  int            getCardScore() const;


 protected:
  // Clear the foundCards array.
  void           cleanFoundCards();

  // Handle the "found" cards, i.e. those that comprise the scoring part.
  void           addFoundCard(int cardNum);

  // Analyze the poker hand.  This is the most important function of all.
  PokerHandType  do_analyze();


 private:
  // Primary data: The cards themselves.
  CardValue      m_cards[PokerHandSize];

  // ----------------------------------------------------------------
  // Secondary data: can be derived from the Primary data.
  bool           m_changed;	// true if something has changed since the
                                // secondary data was last recalculated.

  PokerHandType  m_type;	// Pair, Two Pairs, etc
  CardRank       m_firstRank;	// Rank of first component
  CardRank       m_secondRank;	// Rank of second component

  bool           m_foundCards[PokerHandSize]; // True for all scoring cards 
};


#endif
