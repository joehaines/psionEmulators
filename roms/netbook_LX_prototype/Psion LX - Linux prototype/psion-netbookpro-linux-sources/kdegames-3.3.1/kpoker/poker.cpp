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


#include <stdio.h>
#include <assert.h>

#include "poker.h"


// ================================================================
//                         Card classes


CardDeck::CardDeck(KRandomSequence *random)
{
  m_random = random;

  reset();
}


CardDeck::~CardDeck()
{
}

 
// ----------------------------------------------------------------


void
CardDeck::reset()
{
  int        i;
  CardValue  card;

  for (i = 0, card = lowestCard; i < numCards; i++, card = card + 1)
    m_cards[i] = card;
  m_topCard = 0;

}


void
CardDeck::mix()
{
  CardValue  tmp;
  int        card;

  if (m_topCard == numCards)
    return;

  for (int i = m_topCard; i < numCards - 1; i++) {
    // Choose a random card from the non-mixed ones.
    card =  m_random->getLong(numCards - i) + i;

    // Exchange the random card with the current card. 
    tmp = m_cards[i];
    m_cards[i] = m_cards[card];
    m_cards[card] = tmp;
  }
}


CardValue
CardDeck::getTopCard()
{
  if (m_topCard == numCards)
    return DECK;

  return m_cards[m_topCard++];
}


// ================================================================
//                         Poker types


QString PokerHandNames[] = {
  "High Card",
  "Pair",
  "Two Pairs",
  "Three Of A Kind",
  "Straight",
  "Flush",
  "Full House",
  "Four Of A Kind",
  "Straight Flush",
  "Royal Flush"
};


PokerHand::PokerHand()
{
  clear();
}


PokerHand::~PokerHand()
{
}


// Compare two poker hands, and return true if the first one is less
// valuable than the second one.  Otherwise return false.

bool
PokerHand::operator<(PokerHand &hand2)
{
  CardValue  card1;
  CardValue  card2;
  int        i;

  // Make sure all relevant fields are initialized.
  if (m_changed)
    analyze();
  if (hand2.m_changed)
    hand2.analyze();

#if 0
  kdDebug() << "Hand 1: " << PokerHandNames[(int) m_type] 
	    << " (" << ((int) m_firstRank) + 2
	    << ", " << ((int) m_secondRank) + 2 
	    << ")" << endl;
  kdDebug() << "Hand 2: " << PokerHandNames[(int) hand2.m_type]
	    << " (" << ((int) hand2.m_firstRank) + 2
	    << ", " << ((int) hand2.m_secondRank) + 2 
	    << ")" << endl;
#endif

  // 1. If we have a better hand, then it is simple.
  if (m_type != hand2.m_type)
    return m_type < hand2.m_type;

  // 2. If the hands are equal, check the internal parts of the hand
  //    type (like the first and second pair of two pairs).
  switch (m_type) {
  case HighCard:
  case Pair:
  case ThreeOfAKind:
  case Straight:
  case FourOfAKind:
  case StraightFlush:
  case RoyalFlush:
    if (m_firstRank != hand2.m_firstRank)
      return m_firstRank < hand2.m_firstRank;
    break;

  case TwoPairs:
  case FullHouse:
    // Compare the first rank first, and then the second.
    if (m_firstRank != hand2.m_firstRank)
      return m_firstRank < hand2.m_firstRank;
    if (m_secondRank != hand2.m_secondRank)
      return m_secondRank < hand2.m_secondRank;
    break;

  case Flush:
    card1 = ROOF;
    card2 = ROOF;
    for (i = 0; i < PokerHandSize; i++) {
      card1 = findNextBest(card1, true);
      card2 = hand2.findNextBest(card2, true);

      if (card1 != card2)
	return card1 < card2;
    }
    // If we get here all the card ranks are the same in both hands.
    // This means that they have to be of different suits.
    break;

  default:
    // Shouldn't get here.
    assert(0);
  }

  // 3. Ok, the hands themselves are the same.  Now check if the cards
  //    outside the hands differ.
  card1 = ROOF;
  card2 = ROOF;
  while (card1 != DECK) {
    card1 = findNextBest(card1, false);
    card2 = hand2.findNextBest(card2, false);

    if (card1 != card2)
      return card1 < card2;
  }

  // 4. *Every* rank is the same.  Then they must differ in suit.

  card1 = rank2card(m_firstRank + 1);
  card2 = rank2card(hand2.m_firstRank + 1);

  return card1 < card2;
}


// ----------------------------------------------------------------
//                       Ordinary methods


// Clear the hand - set all entries to DECK.
void
PokerHand::clear()
{
  for (int i = 0; i < PokerHandSize; i++)
    m_cards[i] = DECK;

  m_changed = true;
}


/* Throw away card no 'cardno'.
 */

void
PokerHand::clear(int cardno)
{
  m_cards[cardno] = DECK;

  m_changed = true;
}


CardValue
PokerHand::getCard(int cardno) const
{
  return m_cards[cardno];
}


void
PokerHand::setCard(int cardno, CardValue card)
{
  m_cards[cardno] = card;

  m_changed = true;
}


bool
PokerHand::findRank(CardRank the_rank) const
{
  for (int i = 0; i < PokerHandSize; i++)
    if (rank(m_cards[i]) == the_rank)
      return true;

  return false;
}


bool
PokerHand::findSuit(CardSuit the_suit) const
{
  for (int i = 0; i < PokerHandSize; i++)
    if (suit(m_cards[i]) == the_suit)
      return true;

  return false;
}


// Return the next lower card value below the card 'roof'.  
//
// If 'getFound' is true, then search only those cards that are marked
// as found, i.e. are among those that determine the hand type.
// Otherwise search the cards that are *not* among these.
//

CardValue
PokerHand::findNextBest(CardValue roof, bool getFound) const
{
  CardValue  next;

  next = DECK;
  for (int i = 0; i < PokerHandSize; i++) {
    if (m_cards[i] > next && m_cards[i] < roof
	&& (getFound == m_foundCards[i]))
      next = m_cards[i];
  }

  return next;
}


int
PokerHand::testStraight()
{
  CardRank  lowest = ACE;

  /* Set ranks[i] to the value of each card and find the lowest value. */
  for (int i = 0; i < PokerHandSize; i++) {
    if (rank(m_cards[i]) < lowest)
      lowest = rank(m_cards[i]);
  }

  // Look for special cases ace-2-3-4-5.
  // (very ugly but fast to write):
  if ((findRank(ACE) && findRank(TWO)
       && findRank(THREE) && findRank(FOUR) 
       && findRank(FIVE))) {
    m_firstRank  = FIVE;
  }
  else {

    for (int i = 0; i < PokerHandSize; i++)
      if (!findRank(lowest + i)) 
	return 0; // did not find a straight

    m_firstRank  = lowest + 4;
  }

  // Found a straight.  Record it in foundCards[].
  for (int i = 0; i < PokerHandSize; i++)
    addFoundCard(i);

  // Check for a royal flush
  if (lowest == TEN)
    return 2;

  // An ordinary straight: return 1.
  return 1;
}


bool
PokerHand::testFlush()
{
  int        theSuit;
  CardValue  highest_card;

  highest_card = m_cards[0];
  theSuit      = suit(m_cards[0]);
  for (int i = 1; i < PokerHandSize; i++) {
    if (theSuit != suit(m_cards[i]))
      return 0;

    if (m_cards[i] > highest_card)
      highest_card = m_cards[i];
  }

  // Found a flush.  Now record the cards.
  for (int i = 0; i < PokerHandSize; i++)
    addFoundCard(i);
  m_firstRank = rank(highest_card);

  return true;
}


PokerHandType
PokerHand::analyze()
{
  if (m_changed)
    m_type = do_analyze();

  return m_type;
}


PokerHandType
PokerHand::do_analyze()
{
  CardValue  card1;
  CardValue  card2;
  int        i;
  int        j;
  
  cleanFoundCards();
  m_changed = 0;

  int isStraight = testStraight();

  // Detect special cases;
  if (isStraight) {
    if (testFlush()) {
      if (isStraight == 2)
	return RoyalFlush;
      else
	return StraightFlush;
    }
    else
      return Straight;
  }

  if (testFlush())
    return Flush;

  /* Find number of matches. */
  int matching = 0;
  for (i = 0; i < PokerHandSize; i++) {
    for (j = i + 1; j < PokerHandSize; j++) 
      if (rank(m_cards[i]) == rank(m_cards[j])) {
	matching++;
	addFoundCard(i);
	addFoundCard(j);
      }
  }

  // The algorithm above gives the following results for each case below.
  switch (matching) {
  case 0:			// High card
    card1 = findNextBest(ROOF, false);
    m_firstRank  = rank(card1);
    m_secondRank = (CardRank) -1;

    // In this case, there are no marked cards.  Since we need to mark
    // the best card, we have to search for it and then mark it.
    for (i = 0; i < PokerHandSize; i++) {
      if (m_cards[i] == card1) {
	addFoundCard(i);
	break;
      }
    }
    return HighCard;

  case 1:			// Pair
    m_firstRank  = rank(findNextBest(ROOF, true));
    m_secondRank = (CardRank) -1;
    return Pair;

  case 2:  			// Two pairs
    card1 = findNextBest(ROOF, true);

    // Must do this twice, since the first card we get is the second
    // card of the first pair.
    card2 = findNextBest(card1, true);
    card2 = findNextBest(card2, true);

    m_firstRank  = rank(card1);
    m_secondRank = rank(card2);
    return TwoPairs;

  case 3:			// 3 of a kind
    m_firstRank = rank(findNextBest(ROOF, true));
    return ThreeOfAKind;

  case 4: 			// Full house
    // This is the only tricky case since the value is determined more
    // by the 3 of a kind than by the pair.  The rank of the 3 of a
    // kind can be lower then the pair, though.

    // Get the best and third best cards into val1 and val2.
    card1 = findNextBest(ROOF, true);
    card2 = findNextBest(card1, true);
    card2 = findNextBest(card2, true);

    // Now we have one of two different cases:
    // 1. rank(card1) == rank(card2):  the 3 of a kind is biggest.
    // 2. rank(card1) > rank(card2):   the pair is biggest.
    if (rank(card1) == rank(card2)) {
      m_firstRank  = rank(card1);
      m_secondRank = rank(findNextBest(card2, true));
    }
    else {
      m_firstRank  = rank(card2);
      m_secondRank = rank(card1);
    }
    return FullHouse;

  case 6:			// 4 of a kind
    m_firstRank = rank(findNextBest(ROOF, true));
    return FourOfAKind;

  default:
    break;
  }

  // Shouldn't get here.
  assert(0);

  return (PokerHandType) -1;
}


// FIXME: When we fix the scores, in raise() and bet() to be higher
//        with better hands, then change the "rank(C_ACE) -" stuff 
//        below.
//
int
PokerHand::getCardScore() const
{
  int score = 0;

  for (int i = 0; i < PokerHandSize; i++) {
    if (m_foundCards[i])
      score += rank(C_ACE) - rank(m_cards[i]);
  }

  return score;
}


bool
PokerHand::getFoundCard(int cardNum) const
{
  return m_foundCards[cardNum];
}



// ----------------------------------------------------------------
//                 PokerHand protected methods


void
PokerHand::cleanFoundCards()
{
  for (int i = 0; i < PokerHandSize; i++)
    m_foundCards[i] = false;
}


void
PokerHand::addFoundCard(int cardNum)
{
  m_foundCards[cardNum] = true;
}
