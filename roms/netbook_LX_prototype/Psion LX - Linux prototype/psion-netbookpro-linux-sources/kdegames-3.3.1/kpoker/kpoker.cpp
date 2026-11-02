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
#include <stdlib.h>

// QT includes
#include <qlabel.h>
#include <qtimer.h>
#include <qpainter.h>
#include <qtooltip.h>
#include <qlayout.h>

// KDE includes
#include <kmessagebox.h>
#include <kapplication.h>
#include <kglobal.h>
#include <kstandarddirs.h>
#include <kdebug.h>
#include <kconfig.h>
#include <klocale.h>
#include <kaudioplayer.h>
#include <kcarddialog.h>

// own includes
#include "betbox.h"
#include "kpaint.h"
#include "optionsdlg.h"
#include "newgamedlg.h"
#include "player.h"
#include "playerbox.h"
#include "version.h"
#include "kpoker.h"
#include "defines.h"


// ================================================================
//                         Class PokerGame


PokerGame::PokerGame(KRandomSequence *random)
  : m_deck(random)
{
  // We don't need to save if we just started.
  m_isDirty = false;
}


PokerGame::~PokerGame()
{
  m_activePlayers.clear();
  m_removedPlayers.clear();

  delete[] m_players;
}


void
PokerGame::dealCards(PokerPlayer *player, bool skip[])
{
  CardValue  card;

  for (int i = 0; i < PokerHandSize; i++) {
    if (skip[i])
      continue;

    card = m_deck.getTopCard();

    player->takeCard(i, card);
  }
}


void
PokerGame::newRound()
{
  m_pot = 0;

  // Collect the cards and mix them.
  m_deck.reset();
  m_deck.mix();
}


// ================================================================
//                       Global variables


CardImages *cardImages;


// ================================================================
//                           Class kpok


kpok::kpok(QWidget *parent, const char *name)
  : QWidget(parent, name),
    m_game(&m_random)
{
  QString version;

  m_random.setSeed(0);
  
  lastHandText = "";

  version = kapp->caption() + " " + KPOKER_VERSION;
  setCaption( version );

  mOptions   = 0;

  playerBox  = 0;
  adjust     = false;

  initWindow();		// FIXME: Change this name!!!
  initSound();

  if (!readEntriesAndInitPoker())
    exit(0);
}


kpok::~kpok()
{
  if (mOptions != 0) {
    if (m_game.m_activePlayers.count() + m_game.m_removedPlayers.count() > 1) {
      m_game.m_minBet = mOptions->getMinBet();
      m_game.m_maxBet = mOptions->getMaxBet();
    }
    drawDelay = mOptions->getDrawDelay();
    //	cashPerRound = mOptions->getCashPerRound(); // NOT(!) configurable
  }

  KConfig* conf = kapp->config();
  conf->setGroup("General");
  conf->writeEntry("MaxBet", m_game.m_maxBet);
  conf->writeEntry("MinBet", m_game.m_minBet);
  // conf->writeEntry("CashPerRound", cashPerRound);
  conf->writeEntry("DrawDelay", drawDelay);

  for (int i = 0; i < m_game.getNumPlayers(); i++)
    delete playerBox[i];
  delete[] playerBox;

  delete mOptions;
}


// Init the main window and load the configuration and card images.
//
// Only called by kpok::kpok() once.

void kpok::initWindow()
{
  blinking = true;

  // General font stuff.  Define myFixedFont and wonFont.
  QFont myFixedFont;
  myFixedFont.setPointSize(12);
  QFont wonFont;
  wonFont.setPointSize(14);
  wonFont.setBold(true);

  topLayout = new QVBoxLayout(this, BORDER);
  QVBoxLayout* topInputLayout = new QVBoxLayout;
  topLayout->addLayout(topInputLayout);

  QHBoxLayout* betLayout = new QHBoxLayout;
  inputLayout = new QHBoxLayout;
  inputLayout->addLayout(betLayout);
  topInputLayout->addLayout(inputLayout);

  // The draw button
  drawButton = new QPushButton(this);
  drawButton->setText(i18n("&Draw"));
  connect(drawButton, SIGNAL(clicked()), this, SLOT(drawClick()));
  inputLayout->addWidget(drawButton);
  inputLayout->addStretch(1);

  // The waving text
  QFont  waveFont;
  waveFont.setPointSize(16);
  waveFont.setBold(true);
  QFontMetrics  tmp(waveFont);

  // The widget where the winner is announced.
  mWonWidget = new QWidget(this);
  inputLayout->addWidget(mWonWidget, 2);
  mWonWidget->setMinimumHeight(50); //FIXME hardcoded value for the wave
  mWonWidget->setMinimumWidth(tmp.width(i18n("You won %1").arg(KGlobal::locale()->formatMoney(100))) + 20); // workaround for width problem in wave
  QHBoxLayout* wonLayout = new QHBoxLayout(mWonWidget);
  wonLayout->setAutoAdd(true);

  wonLabel = new QLabel(mWonWidget);
  wonLabel->setFont(wonFont);
  wonLabel->setAlignment(AlignCenter);
  wonLabel->hide();
  inputLayout->addStretch(1);

  // The pot
  potLabel = new QLabel(this);
  potLabel->setFont(myFixedFont);
  potLabel->setFrameStyle(QFrame::WinPanel | QFrame::Sunken);
  inputLayout->addWidget(potLabel, 0, AlignCenter);

  // Label widget in the lower left.
  clickToHold = new QLabel(this);
  clickToHold->hide();

  // Timers
  blinkTimer = new QTimer(this);
  connect( blinkTimer, SIGNAL(timeout()), SLOT(bTimerEvent()) );
  blinkStat=0;

  waveTimer = new QTimer(this);
  connect( waveTimer, SIGNAL(timeout()), SLOT(waveTimerEvent()) );

  drawTimer = new QTimer(this);
  connect (drawTimer, SIGNAL(timeout()), SLOT(drawCardsEvent()) );

  // and now the betUp/Down Buttons
  betBox = new BetBox(this, 0);
  betLayout->addWidget(betBox);
  connect(betBox, SIGNAL(betChanged(int)), this, SLOT(betChange(int)));
  connect(betBox, SIGNAL(betAdjusted()),   this, SLOT(adjustBet()));
  connect(betBox, SIGNAL(fold()),          this, SLOT(out()));

  // some tips 
  QToolTip::add(drawButton, i18n("Draw new cards"));
  QToolTip::add(potLabel, i18n("The current pot"));

  // Load all cards into pixmaps first -> in the constructor.
  cardImages = new CardImages(this, 0);

  // The configuration
  KConfig* conf = kapp->config();
  conf->setGroup("General");

  // Load the card deck.
  if (conf->readBoolEntry("RandomDeck", true)) {
    cardImages->loadDeck(KCardDialog::getRandomDeck());
  } else {
    cardImages->loadDeck(conf->readPathEntry("DeckPath", KCardDialog::getDefaultDeck()));
  }
  if (conf->readBoolEntry("RandomCardDir", true)) {
    cardImages->loadCards(KCardDialog::getRandomCardDir());
  } else {
    cardImages->loadCards(conf->readPathEntry("CardPath",
				  KCardDialog::getDefaultCardDir()));
  }

  blinkingBox = 0;
}


// Start a new complete game (not a round).

void kpok::newGame()
{
  NewGameDlg* newGameDlg;

  lastHandText = "";
  newGameDlg = new NewGameDlg(this);
  newGameDlg->hideReadingFromConfig();

  if (newGameDlg->exec()) {
    stopBlinking();
    stopDrawing();
    stopWave();

    int players = DEFAULT_PLAYERS;

    // Delete the old values
    for (int i = 0; i < m_game.getNumPlayers(); i++)
      delete playerBox[i];
    delete[] m_game.m_players;
    delete[] playerBox;// necessary?

    m_game.m_activePlayers.clear();
    m_game.m_removedPlayers.clear();

    players = newGameDlg->getPlayers();

    //most things will be done in initPoker
    initPoker(players);

    // Set/show the name and money of all players.
    for (int unsigned i = 0; i < m_game.m_activePlayers.count(); i++){
      m_game.m_activePlayers.at(i)->setName(newGameDlg->name(i));
      playerBox[i]->showName();
      m_game.m_activePlayers.at(i)->setCash((newGameDlg->money() >= m_game.m_minBet) ? newGameDlg->money() : START_MONEY);
      m_game.setDirty();
    }

    // Show the money for all players and the pot.
    paintCash();

    // Activate the Draw button.
    drawButton->setText(i18n("&Draw"));
    drawButton->setEnabled(true);
  }

  delete newGameDlg;
}


void kpok::newRound()
{
  bool  onePlayerGame         = false;

  m_game.newRound();

  playerBox[0]->setHeldEnabled(false);

  int players = m_game.m_activePlayers.count() + m_game.m_removedPlayers.count();
  if (players == 1)
    onePlayerGame = true;

  readOptions(); // maybe some options have changed so check em
  m_game.m_activePlayers.clear();
  m_game.m_removedPlayers.clear();

  if (m_game.m_players[0].getCash() < m_game.m_minBet)
    noMoney();
  else {
    for (int i = 0; i < players; i++) {
      if (m_game.m_players[i].getCash() >= m_game.m_minBet)
	m_game.m_activePlayers.append(&m_game.m_players[i]);
      else
	removePlayerFromRound(&m_game.m_players[i]);
    }
  }

  if (m_game.m_activePlayers.count() == 1 && !onePlayerGame)
    switchToOnePlayerRules();

  blinkingBox = 0;
  wonLabel->hide();
  stopBlinking();
  stopWave();

  for (int unsigned i = 0; i < m_game.m_activePlayers.count(); i++)
    m_game.m_activePlayers.at(i)->newRound();

  // We are beginning a new round so every card is available
  drawAllDecks();
  playerBox[0]->showHelds(false);

  // Deal first cards of the round
  bool skip[PokerHandSize];
  for (int i = 0; i < PokerHandSize; i++)
    skip[i] = false;

  for (int unsigned i = 0; i < m_game.m_activePlayers.count(); i++)
    drawCards(m_game.m_activePlayers.at(i), skip);

  if (m_game.m_activePlayers.count() > 1) {
    findHumanPlayer()->changeBet(m_game.m_minBet);
    m_game.bet(m_game.m_minBet);
    betBox->show();
  }
  else {
    m_game.m_activePlayers.at(0)->changeBet(cashPerRound);
    betBox->hide();
  }

  paintCash();

  drawTimer->start(drawDelay, TRUE);
}


void kpok::bet()
{
  // The players will bet, now.  Player 1 (the human one ;-)) has
  // already bet using the betBox ALL players (except player 1 who has
  // already had) will get a chance to raise the value if nobody
  // raises further that player will not get another chance, else he
  // will.

  bool raised = false;
  int oldMustBet = 0;
  if (m_game.getState() == StateRaise)
    oldMustBet = currentMustBet;

  if (m_game.getState() == StateBet || m_game.getState() == StateRaise) {
    // The betting has just begun.  This can be either the first or
    // the second betting phase (state == StateBet or StateRaise).
    // FIXME: These state names must be changed!

    // Find out how much the other players must bet.
    currentMustBet = findHumanPlayer()->getCurrentBet(); 

    // First bet as usual.
    for (int unsigned i = 0; i < m_game.m_activePlayers.count(); i++) {
      PokerPlayer *player    = m_game.m_activePlayers.at(i);
      int          playerBet = 0;

      if (player->getHuman()) 
	continue;

      if (m_game.getState() == StateBet) // first bet phase
	playerBet = player->bet(currentMustBet);
      else if (m_game.getState() == StateRaise) // 2nd bet phase
	playerBet = player->raise(currentMustBet);

      if (playerBet < currentMustBet) {
	removePlayerFromRound(player);
	// FIXME: This should be bet from the beginning!
	if (m_game.getState() == StateBet)
	  m_game.bet(m_game.m_minBet);
	i--;
      }
      else {
	// The player is seeing or has raised.

	if (playerBet > currentMustBet)
	  raised = true;

	currentMustBet = playerBet;
	m_game.bet(currentMustBet - oldMustBet);
      }
    }
  }
  else if (m_game.getState() == StateContinueBet
	   || m_game.getState() == StateContinueRaise) {

    // The bet has been raised.
    paintCash();
    for (unsigned int i = 0; i < m_game.m_activePlayers.count(); i++) {
      PokerPlayer *player    = m_game.m_activePlayers.at(i);

      // human player
      if (player->getCurrentBet() < currentMustBet && player->getHuman()) {
	removePlayerFromRound(player);
	i--;
      }
    }
  }

  paintCash();


  oldBet_raise = findHumanPlayer()->getCurrentBet(); // used by out() only
  if (m_game.getState() == StateBet || m_game.getState() == StateRaise) {
    if (raised) {
      if (m_game.getState() == StateBet)
	m_game.setState(StateContinueBet);
      else
	m_game.setState(StateContinueRaise);

      if (adjust)
	adjustBet();
      else
	out(); // not necessary
      betBox->setEnabled(true);
      betBox->beginRaise();
      if (adjust)
	emit statusBarMessage(i18n("Clicking on draw means you adjust your bet"));
      else
	emit statusBarMessage(i18n("Clicking on draw means you are out"));
    }
    else {
      if (m_game.getState() == StateBet)
	m_game.setState(StateExchangeCards);
      else
	m_game.setState(StateSee);
    }
  }
  else if (m_game.getState() == StateContinueBet && !raised) {
    emit clearStatusBar();
    //	weWillAdjustLabel->hide();
    betBox->stopRaise();
    m_game.setState(StateExchangeCards);
  }
  else if (m_game.getState() == StateContinueRaise && !raised) {
    emit clearStatusBar();
    //	weWillAdjustLabel->hide();
    betBox->stopRaise();
    m_game.setState(StateSee);
  }


  // Check if player 1 is out -> for players > 2
  // TODO: maybe if (!m_activePlayers.contains(humanPlayer))
  // {exchangeCards(); bet(); displayerWinner_computer();return;}


  // don't continue game if player 1 is alone
  // TODO: port to players > 2
  // this is ONLY working for players <= 2
  if (m_game.m_removedPlayers.count() >= 1 
      && m_game.m_activePlayers.count() == 1)
    displayWinner_Computer(m_game.m_activePlayers.at(0), true);
}


void kpok::out()
{
  // weWillAdjustLabel->hide();
  emit clearStatusBar();

  m_game.bet(oldBet_raise - findHumanPlayer()->getCurrentBet());
  findHumanPlayer()->changeBet(oldBet_raise 
			       - findHumanPlayer()->getCurrentBet());
  paintCash();
  drawButton->setText(i18n("&Draw"));
}


void kpok::adjustBet()
{
  emit clearStatusBar();

  betChange(currentMustBet - findHumanPlayer()->getCurrentBet());
  paintCash();
}


// Initiate for a completely new game (not a round).
//
// This is called from newGame(), readEntriesAndInitPoker(), loadGame().
//

void kpok::initPoker(int numPlayers, PokerPlayer* ownAllPlayers)
{
  m_game.m_numPlayers = numPlayers;

  // read some defaults
  kapp->config()->setGroup("General");
  m_game.m_minBet = kapp->config()->readNumEntry("MinBet", MIN_BET);
  m_game.m_maxBet = kapp->config()->readNumEntry("MaxBet", MAX_BET);

  // NOT(!) configurable
  cashPerRound = CASH_PER_ROUND;
  drawDelay = kapp->config()->readNumEntry("DrawDelay", DRAWDELAY);

  blinkingBox = 0;
  m_game.setState(StateStartRound);
  currentMustBet = m_game.m_minBet;

  // Init the players first.
  // We use a QPtrList to access the players.
  m_game.m_activePlayers.clear();
  m_game.m_removedPlayers.clear();

  // Create an array of PokerPlayers if we didn't get any through the call.
  if (ownAllPlayers == 0) {
    m_game.m_players = new PokerPlayer[numPlayers];
    for (int i = 0; i < numPlayers; i++)
      m_game.m_activePlayers.append(&m_game.m_players[i]);
    m_game.m_activePlayers.at(0)->setHuman();
    m_game.m_activePlayers.at(0)->setName(i18n("You"));
    for (int unsigned i = 1; i < m_game.m_activePlayers.count(); i++) {
      m_game.m_activePlayers.at(i)->setName(QString("Computer %1").arg(i-1));
    }
  }
  else {
    // There has someone an own version of m_players so lets use it!
    m_game.m_players = ownAllPlayers;
    for (int i = 0; i < numPlayers; i++)
      m_game.m_activePlayers.append(&m_game.m_players[i]);
  }

  // Inform players how much they may bet.
  for (int unsigned i = 0; i < m_game.m_activePlayers.count(); i++)
    m_game.m_activePlayers.at(i)->setBetDefaults(m_game.m_minBet, m_game.m_maxBet);

  // Make all labels / boxes / cardwidgets for every player
  playerBox = new PlayerBox *[m_game.m_activePlayers.count()]; // not nice but more easy to use ;-)

  for (int unsigned i = 0; i < m_game.m_activePlayers.count(); i++) {
    playerBox[i] = new PlayerBox(i == 0, this);
    playerBox[i]->setPlayer(m_game.m_activePlayers.at(i));
    if (i == 0)
      topLayout->insertWidget(0, playerBox[i]);
    else
      topLayout->addWidget(playerBox[i]);
    playerBox[i]->showName();
    playerBox[i]->hide();
    playerBox[i]->show();	// if it has been deleted and created again it
				// hasn't be shown correctly - hiding and
				// re-showing solves the problem

  }

  // Connects for player 1
  playerBox[0]->activateToggleHeld(); // make CardWidget::toggleHeld() work
  connect(playerBox[0], SIGNAL(toggleHeld()), this, SLOT(toggleHeld()));

  // hide some things
  playerBox[0]->showHelds(false);
  wonLabel->hide();
  emit showClickToHold(false);
  if (m_game.m_activePlayers.count() > 1) {
    setHand(i18n("Nobody"), false);
    betBox->show();
    betBox->setEnabled(false);
    potLabel->show();
  }
  else {
    setHand(i18n("Nothing"));
    betBox->hide();
    potLabel->hide();
    playerBox[0]->singlePlayerGame(cashPerRound);
  }

  // some final inits
  drawStat = 0;
  waveActive = 0;
  fCount = 0;

  // finally clear the pot and show the decks/cash - in one word: begin :-)
  m_game.clearPot();
  drawAllDecks();
  for (int unsigned i = 0; i < m_game.m_activePlayers.count(); i++) {
    playerBox[i]->repaintCard();
  }
  paintCash();
  playerBox[0]->setHeldEnabled(false);
}


void kpok::paintCash()
{
  for (int unsigned i = 0; i < m_game.m_activePlayers.count(); i++) {
    playerBox[i]->showCash();
  }
  potLabel->setText(i18n("Pot: %1").arg(KGlobal::locale()->formatMoney(m_game.getPot())));
}


void kpok::updateLHLabel()
{
  if (lastHandText != "")
    setHand(lastHandText);
  else if (getPlayers() > 1)
    setHand(i18n("Nobody"), false);
  else
    setHand(i18n("Nothing"));
}


void kpok::setHand(const QString& newHand, bool lastHand)
{
  emit changeLastHand(newHand, lastHand);

  lastHandText = newHand;
}


void kpok::toggleHeld()
{
  if (m_game.getState() == StateBet || m_game.getState() == StateContinueBet)
    playSound("hold.wav");
}


void kpok::drawClick()
{
  if (!drawButton->isEnabled())
    return;

  // If this is the start of a new round, then get new cards.
  if (m_game.getState() == StateStartRound) {
    drawButton->setEnabled(false);
    betBox->setEnabled(false);
    newRound();
  }
  else if (m_game.getState() == StateBet) { // bet
    emit showClickToHold(false);
    //clickToHold->hide();

    bet();
    if (m_game.getState() == StateExchangeCards) {// should be set in bet()
      drawClick();
    }
  }
  else if (m_game.getState() == StateContinueBet) { // continue bet
    bet();
    if (m_game.getState() == StateExchangeCards) {// should be set in bet()
      drawClick();
    }
  }
  else if (m_game.getState() == StateExchangeCards) { // exchange cards
    drawButton->setEnabled(false);
    playerBox[0]->setHeldEnabled(false);
    betBox->setEnabled(false);
    bool skip[PokerHandSize];
    for (int i = 0; i < PokerHandSize; i++)
      skip[i] = false;

    for (int unsigned i = 0; i < m_game.m_activePlayers.count(); i++) {
      if (!m_game.m_activePlayers.at(i)->getHuman())
	m_game.m_activePlayers.at(i)->exchangeCards(skip);
      else {
	for (int i = 0; i < PokerHandSize; i++) {
	  skip[i] = playerBox[0]->getHeld(i);
	  if (!skip[i])
	    playerBox[0]->paintDeck(i);
	}
      }
      drawCards(m_game.m_activePlayers.at(i), skip);
    }

    if (playerBox[0]->getHeld(0))
      drawTimer->start(0, TRUE);
    else
      drawTimer->start(drawDelay, TRUE);
  }
  else if (m_game.getState() == StateRaise) { // raise
    setBetButtonEnabled(false);
    bet();

    if (m_game.getState() == StateSee)//should be set in bet()->if no one has raised
      drawClick();
  }
  else if (m_game.getState() == StateContinueRaise) {
    bet();
    if (m_game.getState() == StateSee)
      drawClick();
  }
  else if (m_game.getState() == StateSee)
    winner();
}


void kpok::drawCards(PokerPlayer* p, bool skip[])
{
  m_game.dealCards(p, skip);
}


void kpok::displayWinner_Computer(PokerPlayer* winner, bool othersPassed)
{
  // Determine the box that contains the winner.
  for (int unsigned i = 0; i < m_game.m_activePlayers.count() + m_game.m_removedPlayers.count(); i++) {
    if (&m_game.m_players[i] == winner)
      blinkingBox = i;
  }

  // Give the pot to the winner.
  winner->setCash(winner->getCash() + m_game.getPot());
  m_game.setDirty();

  // Generate a string with winner info and show it.
  QString label;
  if (winner->getHuman())
    label = i18n("You won %1").arg(KGlobal::locale()->formatMoney(m_game.getPot()));
  else
    label = i18n("%1 won %2").arg(winner->getName()).arg(KGlobal::locale()->formatMoney(m_game.getPot()));
  wonLabel->setText(label);

  // Start the waving motion of the text.
  QFont waveFont;
  waveFont.setBold(true);
  waveFont.setPointSize(16);
  QFontMetrics tmp(waveFont);
  mWonWidget->setMinimumWidth(tmp.width(label) + 20);

  // Play a suitable sound.
  if (winner->getHuman()) {
    playSound("win.wav");
    wonLabel->hide();
    startWave();
  }
  else {
    playSound("lose.wav");
    wonLabel->show();
  }

  m_game.clearPot();
  m_game.setState(StateStartRound);
  drawButton->setEnabled(true);
  setHand(winner->getName(), false);
  paintCash();

  // Only start blinking if player 1 is still in.
  if (m_game.m_activePlayers.contains(&m_game.m_players[0]) && !othersPassed)
    startBlinking();

  drawButton->setText(i18n("&Draw"));
}


void kpok::showComputerCards()
{
  // Don't show cards of 'out' players.
  for (int unsigned i = 0; i < m_game.m_activePlayers.count(); i++) {
    if (!m_game.m_activePlayers.at(i)->getHuman()){
      playerBox[i]->paintCard(drawStat); //TODO change: 

      if (i == 1) //TODO : CHANGE!
	playSound("cardflip.wav");//perhaps in playerbox or even in  cardwidget
    }
  }

  if (drawStat == 4) {   // just did last card
    drawButton->setEnabled(true);
    drawStat = 0;
  } else { // only inc drawStat if not done with displaying
    drawStat++;
    showComputerCards();
  }
}


void kpok::setBetButtonEnabled(bool enabled)
{
  betBox->setEnabled(enabled);
}


void kpok::drawCardsEvent()
{
  if (!playerBox[0]->getHeld(drawStat)) {
    playerBox[0]->paintCard(drawStat);

    playSound("cardflip.wav");//maybe in playerbox or even in cardwidget
  }

  if (drawStat == 4) {   // just did last card
    drawButton->setEnabled(true);
    betBox->setEnabled(true);
    drawStat = 0;

    if (m_game.getState() == StateExchangeCards) {
      if (m_game.m_activePlayers.count() == 1)
	result();
      else {
	// Now give players the chance to raise.
	drawButton->setText(i18n("&See!"));
	m_game.setState(StateRaise);
      }
    } else if (m_game.getState() == StateStartRound) {
      playerBox[0]->setHeldEnabled(true);

      emit showClickToHold(true);
      //clickToHold->show();
      //TODO:
      m_game.setState(StateBet);
      //drawButton->setText(i18n("&Draw new cards"));
    }

  } else { // only inc drawStat if not done with displaying
    drawStat++;
    // look at next card and if it is held instantly call drawCardEvent again
    if (playerBox[0]->getHeld(drawStat))
      drawTimer->start(0,TRUE);
    else
      drawTimer->start(drawDelay,TRUE);
  }
}


// Called to display the result in a single player game.

void kpok::result()
{
  int testResult = m_game.m_activePlayers.at(0)->testHand();
  switch (testResult) {
  case Pair:
    if (m_game.m_activePlayers.at(0)->getHand().get_firstRank() < JACK) {
      displayWin(i18n("Nothing"), 0); 
      break;
    }

    displayWin(i18n("One Pair"), 5);  
    break;

  case TwoPairs:
    displayWin(i18n("Two Pairs"), 10); 
    break;

  case ThreeOfAKind:
    displayWin(i18n("3 of a Kind"), 15); 
    break;

  case Straight:
    displayWin(i18n("Straight"), 20); 
    break;

  case Flush:
    displayWin(i18n("Flush"), 25); 
    break;

  case FullHouse:
    displayWin(i18n("Full House"), 40); 
    break;

  case FourOfAKind:
    displayWin(i18n("4 of a Kind"), 125); 
    break;

  case StraightFlush:
    displayWin(i18n("Straight Flush"), 250); 
    break;

  case RoyalFlush:
    displayWin(i18n("Royal Flush"), 2000); 
    break;

  default:
    displayWin(i18n("Nothing"), 0);
    break;
  }

  startBlinking();
  m_game.setState(StateStartRound);

  if (m_game.m_activePlayers.at(0)->getCash() < cashPerRound)
    noMoney();
}


// Display winner and give money and so on.

void kpok::winner()
{
  PokerPlayer  *winner;

  showComputerCards();

  winner = m_game.m_activePlayers.at(0);
  for (unsigned int i = 1; i < m_game.m_activePlayers.count(); i++) {
    if (winner->getHand() < m_game.m_activePlayers.at(i)->getHand()) {
      //kdDebug() << "Hand 2 is better." << endl;
      winner = m_game.m_activePlayers.at(i);
    }
    else {
      //kdDebug() << "Hand 1 is better." << endl;
    }
  }
 
  displayWinner_Computer(winner, false);
}


void kpok::noMoney()
{
  KMessageBox::sorry(0, i18n("You Lost"), i18n("Oops, you went bankrupt.\n"
					       "Starting a new game.\n"));
  newGame();
}


void kpok::startBlinking()
{
  blinkTimer->start(650);
}


void kpok::stopBlinking()
{
  blinkTimer->stop();
  blinkStat   = 1;
  blinkingBox = 0;
}


void kpok::startWave()
{
  waveTimer->start(40);
  waveActive = true;
}


void kpok::stopWave()
{
  waveTimer->stop();
  fCount = -1; /* clear image */
  repaint ( FALSE );
  waveActive = false;
}


void kpok::stopDrawing()
{
  drawTimer->stop();
}


void kpok::waveTimerEvent()
{
  fCount = (fCount + 1) & 15;
  repaint( FALSE );
}


void kpok::bTimerEvent()
{
  if (blinking) {
    if (blinkStat != 0) {
      playerBox[blinkingBox]->blinkOn();
      blinkStat=0;
    } else {
      playerBox[blinkingBox]->blinkOff();
      blinkStat=1;
    }
  }
}


void kpok::displayWin(const QString& hand, int cashWon)
{
  QString buf;

  setHand(hand);
  m_game.m_activePlayers.at(0)->setCash(m_game.m_activePlayers.at(0)->getCash() + cashWon);
  m_game.setDirty();
  paintCash();

  if (cashWon) {
    playSound("win.wav");
    buf = i18n("You won %1!").arg(KGlobal::locale()->formatMoney(cashWon));
  } else {
    playSound("lose.wav");
    buf = i18n("Game Over"); // locale
  }
  wonLabel->setText(buf);

  if (!cashWon)
    wonLabel->show();
  else {
    wonLabel->hide();
    startWave();
  }

  //drawButton->setText(i18n("&Deal new round"));
}


void kpok::paintEvent( QPaintEvent *)
{
  /* NOTE: This was shamelessy stolen from the "hello world" example
   * coming with Qt Thanks to the Qt-Guys for doing such a cool
   * example 8-)
   */

  if (!waveActive) {
    return;
  }

  QString  txt = wonLabel->text();
  wonLabel->hide();

  static int sin_tbl[16] = {
    0, 38, 71, 92, 100, 92, 71, 38,  0, -38, -71, -92, -100, -92, -71, -38};

  if ( txt.isEmpty() ) {
    return;
  }

  QFont wonFont;
  wonFont.setPointSize(18);
  wonFont.setBold(true);

  QFontMetrics fm = QFontMetrics(wonFont);

  int w = fm.width(txt) + 20;
  int h = fm.height() * 2;
  while (w > mWonWidget->width() && wonFont.pointSize() > 6) {// > 6 for emergency abort...
    wonFont.setPointSize(wonFont.pointSize() - 1);
    fm = QFontMetrics(wonFont);
    w = fm.width(txt) + 20;
    h = fm.height() * 2;
  }

  int pmx = mWonWidget->width() / 2 - w / 2;
  int pmy = 0;
  //   int pmy = (playerBox[0]->x() + playerBox[0]->height() + 10) - h / 4;

  QPixmap pm( w, h );
  pm.fill( mWonWidget, pmx, pmy );

  if (fCount == -1) { /* clear area */
    bitBlt( mWonWidget, pmx, pmy, &pm );
    return;
  }

  QPainter p;
  int x = 10;
  int y = h/2 + fm.descent();
  unsigned int i = 0;
  p.begin( &pm );
  p.setFont( wonFont );
  p.setPen( QColor(0,0,0) );

  while ( i < txt.length() ) {
    int i16 = (fCount+i) & 15;

    p.drawText( x, y-sin_tbl[i16]*h/800, QString(txt[i]), 1 );
    x += fm.width( txt[i] );
    i++;
  }
  p.end();

  // 4: Copy the pixmap to the Hello widget
  bitBlt( mWonWidget, pmx, pmy, &pm );
}


void kpok::drawAllDecks()
{
  for (int unsigned i = 0; i < m_game.m_activePlayers.count(); i++) {
    for (int i2 = 0; i2 < PokerHandSize; i2++) {
      m_game.m_activePlayers.at(i)->takeCard(i2, 0);
      playerBox[i]->paintCard(i2);
    }
  }
}


void kpok::removePlayerFromRound(PokerPlayer* removePlayer)
{
  removePlayer->setOut(true);

  m_game.m_removedPlayers.append(removePlayer);
  if (m_game.m_activePlayers.contains(removePlayer))
    m_game.m_activePlayers.remove(removePlayer);

  for (int unsigned i = 0; i < m_game.m_activePlayers.count() + m_game.m_removedPlayers.count(); i++)
    playerBox[i]->showCash();
}


void kpok::switchToOnePlayerRules()
{
  KMessageBox::information(0, i18n("You are the only player with money!\n"
				   "Switching to one player rules..."), 
			   i18n("You Won"));

  // Hide all computer players.
  for (int unsigned i = 1; i < m_game.m_activePlayers.count() + m_game.m_removedPlayers.count(); i++) {
    playerBox[i]->hide();
  }

  m_game.m_removedPlayers.clear();
  betBox->hide();
  potLabel->hide();
  playerBox[0]->singlePlayerGame(cashPerRound);
  setHand(i18n("Nothing"));
}


PokerPlayer* kpok::findHumanPlayer()
{
  for (int unsigned i = 0; i < m_game.m_activePlayers.count(); i++) {
    if (m_game.m_activePlayers.at(i)->getHuman())
      return m_game.m_activePlayers.at(i);
  }

  return m_game.m_activePlayers.at(0);//error
}


bool kpok::readEntriesAndInitPoker()
{
  NewGameDlg* newGameDlg = NULL;
  KConfig* conf = kapp->config();
  int players = DEFAULT_PLAYERS;
  PokerPlayer* newPlayers = 0;

  conf->setGroup("NewGameDlg");
  bool showNewGameDlg = conf->readBoolEntry("showNewGameDlgOnStartup", false);
  bool aborted = false;
  bool oldGame = false;

  if (showNewGameDlg) {
    newGameDlg = new NewGameDlg(this);
    if (!newGameDlg->exec())
      return false; // exit game
  }

  if (!aborted && (!showNewGameDlg || showNewGameDlg &&
		   newGameDlg->readFromConfigFile())) {
    //try starting an old game
    oldGame = loadGame();
    if (oldGame)
      return true;
  }

  if (!aborted && showNewGameDlg && !oldGame) {
    // Don't use config file - just take the values from the dialog.
    // (This is also config - the dialog defaults are from config.)
    players = newGameDlg->getPlayers();
    if (players <= 0)
      aborted = true;
    else {
      newPlayers = new PokerPlayer[players];
      newPlayers[0].setHuman();
    }

    for (int i = 0; i < players; i++) {
      newPlayers[i].setName(newGameDlg->name(i));
      newPlayers[i].setCash(newGameDlg->money());
      m_game.setDirty();
    }
  }

  if (aborted || !showNewGameDlg && !oldGame) // user didn't say what he/she wants -> use standards
    initPoker(players);
  else // whatever user said: we have already a 'newPlayers'
    initPoker(players, newPlayers);

  if (newGameDlg != 0) {
    delete newGameDlg;
  }

  return true;
}


void kpok::betChange(int betChange)
{
  PokerPlayer* p = findHumanPlayer();

  switch(m_game.getState()){
  case StateBet:
    if (p->getCurrentBet() + betChange >= m_game.m_minBet &&
	p->getCurrentBet() + betChange <= m_game.m_maxBet) {
      // Bet at least m_minBet but not more than m_maxBet.
      if (p->changeBet(betChange))
	m_game.bet(betChange);
    }
    break;
  case StateRaise:
  case StateContinueRaise:
  case StateContinueBet:
    if (p->getCurrentBet() + betChange >= currentMustBet
	&& p->getCurrentBet() + betChange <= currentMustBet + m_game.m_maxBet) {
      if (p->changeBet(betChange))
	m_game.bet(betChange);
    }
    break;
  default:
    break;
  }

  paintCash();
}


void kpok::saveGame()
{
  kapp->config()->setGroup("Save");
  saveGame(kapp->config());
}


void kpok::saveGame(KConfig* conf)
{
  // kdWarning() << "save game" << endl;
  int players = m_game.m_numPlayers;

  conf->writeEntry("players", players);
  conf->writeEntry("lastHandText", lastHandText);

  for (int i = 0; i < players; i++) {
    conf->writeEntry(QString("Name_%1").arg(i),  m_game.m_players[i].getName());
    conf->writeEntry(QString("Human_%1").arg(i), m_game.m_players[i].getHuman());
    conf->writeEntry(QString("Cash_%1").arg(i),  m_game.m_players[i].getCash());
  }
  
  m_game.clearDirty();
}


void kpok::toggleSound()    { setSound(!sound); }
void kpok::toggleBlinking() { setBlinking(!blinking); }
void kpok::toggleAdjust()   { setAdjust(!adjust); }


void kpok::slotPreferences()
{
  if ( mOptions==0 )
    mOptions = new OptionsDlg(this, 0, m_game.m_activePlayers.count() + m_game.m_removedPlayers.count());
  if (m_game.m_activePlayers.count() + m_game.m_removedPlayers.count() > 1)
    mOptions->init(drawDelay, m_game.m_maxBet, m_game.m_minBet);
  else
    mOptions->init(drawDelay, cashPerRound);
  if (!mOptions->exec()) {
    delete mOptions;
    mOptions = 0;
  }
}


void kpok::setAdjust(bool ad)
{
  adjust = ad;

  //update guessed money statusbar if we currently are in need of adjust
  if (m_game.getState() == StateContinueBet 
      || m_game.getState() == StateContinueRaise) {
    if (adjust)
      adjustBet();
    else
      out();
  }
}


bool kpok::initSound()
{
  sound = true;
  return true;
}


void kpok::playSound(const QString &soundname)
{
  if (sound)
    KAudioPlayer::play(locate("data", QString("kpoker/sounds/")+soundname));
}


void kpok::setSound(bool s)
{
  sound = s;
}


void kpok::readOptions()
{
  if (mOptions != 0) {
    if (m_game.m_activePlayers.count() + m_game.m_removedPlayers.count() > 1) {
      m_game.m_minBet = mOptions->getMinBet();
      m_game.m_maxBet = mOptions->getMaxBet();
      for (int unsigned i = 0; i < m_game.m_activePlayers.count(); i++)
	m_game.m_activePlayers.at(i)->setBetDefaults(m_game.m_minBet, m_game.m_maxBet); // inform players how much they may bet
    }
    drawDelay = mOptions->getDrawDelay();
    //	kdDebug() << cashPerRound << endl;
    //	cashPerRound = mOptions->getCashPerRound(); // NOT(!) configurable
    delete mOptions;
    mOptions = 0;
  }
}


bool kpok::loadGame()
{
  kapp->config()->setGroup("Save");
  return loadGame(kapp->config());
}


bool kpok::loadGame(KConfig* conf)
{
  int players = DEFAULT_PLAYERS;
  PokerPlayer* newPlayers = 0;

  bool oldGame = false;

  // conf->setGroup("Save");
  players = conf->readNumEntry("players", -1);

  if (players > 0) {
    newPlayers = new PokerPlayer[players];

    for (int i = 0; i < players; i++) {
      QString buf = conf->readEntry(QString("Name_%1").arg(i),
				    "Player");
      newPlayers[i].setName(buf);
      bool human = conf->readBoolEntry(QString("Human_%1").arg(i),
				       false);
      if (human)
	newPlayers[i].setHuman(); // i == 0
      int cash = conf->readNumEntry(QString("Cash_%1").arg(i),
				    START_MONEY);
      newPlayers[i].setCash(cash);
      m_game.setDirty();
    }
    oldGame = true;
    initPoker(players, newPlayers);
    // after initPoker because initPoker does a default initialization of lastHandText
    conf->setGroup("Save");
    lastHandText = conf->readEntry("lastHandText", "");
    return true;
  }
  else
    players = DEFAULT_PLAYERS;
  return false;
}


// These slots are called from keyboard shortcuts ('1'..'5')

void kpok::exchangeCard1()  { playerBox[0]->cardClicked(1); }
void kpok::exchangeCard2()  { playerBox[0]->cardClicked(2); }
void kpok::exchangeCard3()  { playerBox[0]->cardClicked(3); }
void kpok::exchangeCard4()  { playerBox[0]->cardClicked(4); }
void kpok::exchangeCard5()  { playerBox[0]->cardClicked(5); }


// This slot is called when the user wants to change some aspect of
// the card deck (front or back).
//
// FIXME: Maybe the slot should be moved to top.cpp instead and simply
//        call a setDeck() or setBack() method in kpok.

void kpok::slotCardDeck()
{
  kapp->config()->setGroup("General");
  QString deckPath = kapp->config()->readPathEntry("DeckPath", 0);
  QString cardPath = kapp->config()->readPathEntry("CardPath", 0);
  bool randomDeck, randomCardDir;

  // Show the "Select Card Deck" dialog and load the images for the
  // selected deck, if any.
  if (KCardDialog::getCardDeck(deckPath, cardPath, this, KCardDialog::Both,
			       &randomDeck, &randomCardDir) 
      == QDialog::Accepted) {

    // Load backside and front images.
    if (playerBox && blinking && (blinkStat == 0))
      bTimerEvent();

    cardImages->loadDeck(deckPath);
    cardImages->loadCards(cardPath);

    for (int unsigned i = 0; i < m_game.m_activePlayers.count(); i++) {
      playerBox[i]->repaintCard();
    }

    // Save selected stuff in the configuration.
    kapp->config()->writePathEntry("DeckPath", deckPath);
    kapp->config()->writeEntry("RandomDeck", randomDeck);
    kapp->config()->writePathEntry("CardPath", cardPath);
    kapp->config()->writeEntry("RandomCardDir", randomCardDir);
  }
}


#include "kpoker.moc"
