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

#include "player.h"
#include "defines.h"


// ================================================================
//                         class Player


PokerPlayer::PokerPlayer()
  : m_hand()
{
  m_name = "Player";
  m_isHuman = false;

  m_money    = START_MONEY;
  currentBet = 0;

  isOut = false;
  // random.setSeed(0);
}


PokerPlayer::~PokerPlayer()
{
}


// ----------------------------------------------------------------


void PokerPlayer::giveCardsBack()
{
  m_hand.clear();
}


void PokerPlayer::giveCardBack(int cardNr)
{
  m_hand.clear(cardNr);
}


/* Set skip[i] to true if the corresponding card should *not* be
 * exchanged.
 */

void PokerPlayer::exchangeCards(bool skip[])
{ 
  //TODO: improve!
  // this is still a VERY simple method!

  m_hand.analyze();
  for (int i = 0; i < PokerHandSize; i++) {
    skip[i] = m_hand.getFoundCard(i);
  }
}


/* Prepare the player for a new round.
 */

void PokerPlayer::newRound()
{
  giveCardsBack();

  currentBet = 0;
  isOut      = false;
}


int PokerPlayer::bet(int origBet, bool mayRaise)
{
  // NOT useable for (status == continueBet) !!!
  // needs a rewrite for games with > 2 players
  int bet = origBet;

  // first bet minBet
  currentBet = minBet;
  m_money -= currentBet;
  bet -= currentBet;
  int newBet = bet; // bet at least the old bet

  if (bet > getCash())// we don't have the money :-(
    return 0;

  // calculate the chances and bet any value
  int chance = sortedResult();

  if (chance < 350) { // 3 of a kind or better!!
    newBet =  maxBet - (int)random.getLong(maxBet /2);//we subtract a 
    // random number to hide our cards (every player would
    // know we have good cards if we would bet maxBet) 
  }
  else if (chance < 400) { // 2 pairs
    newBet = bet + (int)random.getLong(maxBet /2 + origBet);
    if (newBet > maxBet)
      newBet = maxBet;
  }
  else if (chance < 500) { // one pair
    newBet = bet + (int)random.getLong(maxBet /4 + 1);
    if (newBet > getCash() - 2 * minBet)
      newBet = bet;
    if (bet >= getCash() /3)
      newBet = 0;
  }
  else if (chance < 506) { // best card is at least a ten
    newBet = bet;
    if (bet >= getCash() /3)
      newBet = 0;
  }
  else { // bad cards
    if (getCash() - bet >= bet) {// we would still have some money
      newBet = bet;
      if (bet >= getCash() /4)
	newBet = 0;
    }
    else
      newBet = 0;
  }

  // and now a final re-check
  if (newBet > bet) {
    if (random.getLong(20) == 0)
      newBet = bet;
    else if (random.getLong(30) <= 1)
      newBet = bet + (newBet - bet) /2;
  }

  if (newBet > getCash())
    newBet = bet; // maybe raise only a little bit but by now just do not raise

  if (!mayRaise && newBet > bet)
    newBet = bet;

  if (!changeBet(newBet))
    return 0; // BIG error

  return currentBet;
}


int PokerPlayer::raise(int origRaise)
{
  // NOT useable for (status == continueRaise) !!!
  // needs a rewrite for games with > 2 players
  int raise    = origRaise - getCurrentBet();
  int newRaise = raise;

  if (newRaise > getCash())// we don't have the money :-(
    return 0;

  // Calculate the chances and bet any value.
  int chance = sortedResult();

  if (chance < 350) { // 3 of a kind or better!!
    newRaise =  maxBet - (int)random.getLong(maxBet - maxBet /2);
    // we subtract a random number to hide our cards 
    // (every player would know we have good cards if 
    // we would bet maxBet) 
  }
  else if (chance < 400) { // 2 pairs
    newRaise = raise + (int)random.getLong(maxBet /2 + origRaise + 10);
    if (newRaise > maxBet)
      newRaise = maxBet;
  }
  else if (chance < 500) { // one pair
    newRaise = raise + (int)random.getLong(maxBet /4 + 1);
    if (newRaise > getCash() - 2 * minBet)
      newRaise = raise;
    if (raise >= getCash() /2)
      newRaise = 0;
  }
  else if (chance < 506) { // best card is at least a ten
    newRaise = raise;
    if (raise >= getCash() /2)
      newRaise = 0;
  }
  else { // bad cards
    if (getCash() - raise >= raise && raise <= minBet * 2) { // we would still have some money
      if (raise > getCash() /2)
	newRaise = 0;
      else
	newRaise = raise;
    }
    else
      newRaise = 0;
  }

  // And now a final re-check.
  if (newRaise > raise) {
    if (random.getLong(20) == 0)
      newRaise = raise;
    else if (random.getLong(30) <= 1)
      newRaise = raise + (newRaise - raise) /2;
  }

  if (newRaise > getCash())
    newRaise = raise; // maybe raise only a little bit but by now just do not raise

  if (!changeBet(newRaise))
    return 0; // BIG error

  return currentBet;
}


bool PokerPlayer::changeBet(int betChange)
{
  if (currentBet + betChange >= 0 && getCash() - betChange >= 0) {
    setCash(getCash() - betChange);
    currentBet += betChange;
    return true;
  }
  return false;
}


int PokerPlayer::sortedResult()
{
  PokerHandType result = m_hand.analyze();

  //Ok, the result produced by testHand() is a little bit... uncomfortable
  //so lets sort it for use in displayWinner_Computer()
  //additionally we extend the values e.g. by bestCard and so on

  int newResult = m_hand.getCardScore();

  // FIXME: Change this so that scores are higher for better hands.
  //        Don't forget to change m_hand.getCardScore() as well.
  switch (result) {
  case RoyalFlush:
    newResult += 0; // the royal flush is the best you can get
    break;
  case StraightFlush:
    newResult += 50; // straight flush
    break;
  case FourOfAKind:
    newResult += 100; // 4 of a kind
    break;
  case FullHouse:
    newResult += 150; // full house
    break;
  case Flush:
    newResult += 200; // flush
    break;
  case Straight:
    newResult += 250; // straight
    break;
  case ThreeOfAKind:
    newResult += 300; // 3 of a kind
    break;
  case TwoPairs:
    newResult += 350; // two pairs
    break;
  case Pair:
    newResult += 400; // one pair
    break;
  case HighCard:
    {
      CardValue  bestCard = m_hand.findNextBest(ROOF, false);
      newResult = 500 + ((int) H_ACE - (int) bestCard);
    }
    break;

  default:
    // Shouldn't get here.
    assert(0);
  }

  return newResult;
  // The lowest newResult is now the best.
}
