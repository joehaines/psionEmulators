/* Class AbTop */

#include <qpopupmenu.h>
#include <qtimer.h>
#include <qclipboard.h>

#include <kaction.h>
#include <kapplication.h>
#include <kconfig.h>
#include <kdialogbase.h>
#include <kiconloader.h>
#include <klocale.h>
#include <kmenubar.h>
#include <kstdaccel.h>
#include <kglobal.h>
#include <kstatusbar.h>
#include <kstdaction.h>
#include <kstdgameaction.h>
#include <kdebug.h>

#include "AbTop.h"
#include "Board.h"
#include "BoardWidget.h"
#include "EvalDlgImpl.h"
#include "EvalScheme.h"
#include "Network.h"
#include "Spy.h"
#include "version.h"

#include <stdio.h>
#include <stdlib.h>

// #define MYTRACE 1

const AbTop::Data AbTop::LEVEL[Nb_Levels] = {
    { "Easy",   I18N_NOOP("&Easy")   },
    { "Normal", I18N_NOOP("&Normal") },
    { "Hard",   I18N_NOOP("&Hard")   },
    { "Challange", I18N_NOOP("&Challenge") }
};

const AbTop::Data AbTop::IPLAY[Nb_IPlays] = {
    { "Red",    I18N_NOOP("&Red")    },
    { "Yellow", I18N_NOOP("&Yellow") },
    { "Both",   I18N_NOOP("&Both")   },
    { "None",   I18N_NOOP("&None")   }
};

AbTop::AbTop()
  :KMainWindow(0)
{
  timerState = noGame;

  myPort = Network::defaultPort;
  currentEvalScheme = 0;
  net = 0;

  actValue = 0;
  stop = false;
  editMode = false;
  spyLevel = 0;
  pastePossible = true;


  timer = new QTimer;
  connect( timer, SIGNAL(timeout()), this, SLOT(timerDone()) );

  board = new Board();
  setMoveNo(0);

  connect( board, SIGNAL(searchBreak()), this, SLOT(searchBreak()) );
  Q_CHECK_PTR(board);
  boardWidget = new BoardWidget(*board,this);

#ifdef SPION
  spy = new Spy(*board);
#endif

  connect( boardWidget, SIGNAL(updateSpy(QString)),
	   this, SLOT(updateSpy(QString)) );

  setCentralWidget(boardWidget);
  boardWidget->show();

  // this creates the GUI
  setupActions();
  setupStatusBar();
  setMinimumSize(200,300);

  // RMB context menu
  connect( boardWidget, SIGNAL(rightButtonPressed(int,const QPoint&)),
	   this, SLOT(rightButtonPressed(int,const QPoint&)) );

  connect( boardWidget, SIGNAL(edited(int)),
	   this, SLOT(edited(int)) );

  connect( board, SIGNAL(updateBestMove(Move&,int)),
	   this, SLOT(updateBestMove(Move&,int)) );

  connect( boardWidget, SIGNAL(moveChoosen(Move&)),
	   this, SLOT(moveChoosen(Move&)) );

  /* default */
  setLevel(Easy);
  setIPlay(Red);
  showMoveLong = true;
  showSpy = false;
  renderBalls = true;

  updateStatus();
  updateActions();
  setupGUI();
}

AbTop::~AbTop()
{
  /* Unregister from other abalone processes */
  delete net;
  delete timer;
#ifdef SPION
  delete spy;
#endif
}


/**
 * Create all the actions...
 *
 * The GUI will be built in createGUI using a XML file
 *
 */

void AbTop::setupActions()
{
  newAction = KStdGameAction::gameNew( this, SLOT(newGame()), actionCollection() );
  KStdGameAction::quit( this, SLOT(close()), actionCollection() );

  stopAction = new KAction( i18n("&Stop Search"), "stop", Key_S, this,
                            SLOT(stopSearch()), actionCollection(), "move_stop");

  backAction = new KAction( i18n("Take &Back"), "back",
                            KStdAccel::shortcut(KStdAccel::Prior), this,
                            SLOT(back()), actionCollection(), "move_back");

  forwardAction = new KAction( i18n("&Forward"), "forward",
                            KStdAccel::shortcut(KStdAccel::Next), this,
                            SLOT(forward()), actionCollection(), "move_forward");

  hintAction = KStdGameAction::hint(this, SLOT(suggestion()), actionCollection());

  KStdAction::copy( this, SLOT(copy()), actionCollection());
  pasteAction = KStdAction::paste( this, SLOT(paste()), actionCollection());

  (void) new KAction( i18n("&Restore Position"),
		      KStdAccel::shortcut(KStdAccel::Open),
		      this, SLOT(restorePosition()),
		      actionCollection(), "edit_restore" );

  (void) new KAction( i18n("&Save Position"),
		      KStdAccel::shortcut(KStdAccel::Save),
		      this, SLOT(savePosition()),
		      actionCollection(), "edit_save" );

  KToggleAction *ta;

  ta = new KToggleAction( i18n("&Network Play"), "net", Key_N,
			  actionCollection(), "game_net");
  connect(ta, SIGNAL(toggled(bool)), this, SLOT(gameNetwork(bool)));

  editAction = new KToggleAction( i18n("&Modify"), "edit",
                                  CTRL+Key_Insert, actionCollection(), "edit_modify");
  connect(editAction, SIGNAL(toggled(bool)), this, SLOT( editModify(bool)));

  showMenubar = KStdAction::showMenubar(this, SLOT(toggleMenubar()), actionCollection());
  KStdAction::saveOptions( this, SLOT(writeConfig()), actionCollection());

  KStdAction::preferences( this, SLOT(configure()), actionCollection());

  moveSlowAction =  new KToggleAction( i18n("&Move Slow"), 0,
                                       actionCollection(), "options_moveSlow");
  connect(moveSlowAction, SIGNAL(toggled(bool)), this, SLOT(optionMoveSlow(bool)));

  renderBallsAction =  new KToggleAction( i18n("&Render Balls"), 0,
                                          actionCollection(), "options_renderBalls");
  connect(renderBallsAction, SIGNAL(toggled(bool)), this, SLOT(optionRenderBalls(bool)));

  showSpyAction =  new KToggleAction( i18n("&Spy"), 0,
                                      actionCollection(), "options_showSpy");
  connect(showSpyAction, SIGNAL(toggled(bool)), this, SLOT(optionShowSpy(bool)));


  levelAction = KStdGameAction::chooseGameType(0, 0, actionCollection());
  QStringList list;
  for (uint i=0; i<Nb_Levels; i++)
      list.append( i18n(LEVEL[i].label) );
  levelAction->setItems(list);
  connect(levelAction, SIGNAL(activated(int)), SLOT(setLevel(int)));

  iplayAction = new KSelectAction(i18n("&Computer Play"), 0, actionCollection(), "options_iplay");
  list.clear();
  for (uint i=0; i<Nb_IPlays; i++)
      list.append( i18n(IPLAY[i].label) );
  iplayAction->setItems(list);
  connect(iplayAction, SIGNAL(activated(int)), SLOT(setIPlay(int)));
}

void AbTop::toggleMenubar()
{
  if (menuBar()->isVisible())
    menuBar()->hide();
  else
    menuBar()->show();
}

void AbTop::configure()
{
  KDialogBase *dlg = new KDialogBase( 0, "ConfigureEvaluation", true,
				      i18n("Configure Evaluation"),
				      KDialogBase::Ok | KDialogBase::Cancel,
				      KDialogBase::Ok, true);

  EvalDlgImpl *edlg = new EvalDlgImpl(dlg,board);
  dlg->setMainWidget(edlg);
  if (dlg->exec()) {
    *currentEvalScheme = *(edlg->evalScheme());
    board->setEvalScheme(currentEvalScheme);
  }
  delete edlg;
}

/* Right Mouse button pressed in BoardWidget area */
void AbTop::rightButtonPressed(int /* field */, const QPoint& pos)
{
  QPopupMenu* rmbMenu = static_cast<QPopupMenu*> (factory()->container("rmbPopup",this));
  if (rmbMenu)
    rmbMenu->popup( pos );
}

/* Read config options
 *
 * menu must already be created!
 */
void AbTop::readConfig()
{
  kdDebug(12011) << "Reading config..." << endl;

  KConfig* config = kapp->config();
  config->setGroup("Options");

  readOptions(config);

  applyMainWindowSettings( config, "Appearance" );

  showMenubar->setChecked( !menuBar()->isHidden() );

  currentEvalScheme = new EvalScheme("Current");
  currentEvalScheme->read(config);
  board->setEvalScheme( currentEvalScheme );
}

void AbTop::readOptions(KConfig* config)
{
  QString entry = config->readEntry("Level");
  for (uint i=0; i<Nb_Levels; i++)
      if ( entry==LEVEL[i].key ) setLevel(i);

  entry = config->readEntry("Computer");
  for (uint i=0; i<Nb_IPlays; i++)
      if ( entry==IPLAY[i].key ) setIPlay(i);

  showMoveLong = config->readBoolEntry("MoveSlow", false);
  moveSlowAction->setChecked( showMoveLong );

  renderBalls = config->readBoolEntry("RenderBalls", true);
  boardWidget->renderBalls(renderBalls);
  renderBallsAction->setChecked( renderBalls );

  showSpy = config->readBoolEntry("ShowSpy", true);
  board->updateSpy(showSpy);
  showSpyAction->setChecked( showSpy );
}

void AbTop::readProperties(KConfig *config)
{
  QString entry;

  readOptions(config);

  currentEvalScheme = new EvalScheme("Current");
  currentEvalScheme->read(config);
  board->setEvalScheme( currentEvalScheme );


  if (!(entry = config->readEntry("TimerState")).isNull())
    timerState = entry.toInt();
  if (timerState == noGame) return;

  stop = config->readBoolEntry("GameStopped", false);

  int mNo = 0;
  if (!(entry = config->readEntry("Position")).isNull()) {
    mNo = board->setState(entry);
    boardWidget->updatePosition(true);
  }
  setMoveNo(mNo, true);

  show();
  playGame();
}

void AbTop::writeConfig()
{
  kdDebug(12011) << "Writing config..." << endl;

  KConfig* config = kapp->config();
  config->setGroup("Options");

  writeOptions(config);

  saveMainWindowSettings( config, "Appearance" );

  if (currentEvalScheme)
    currentEvalScheme->save(config);
  config->sync();
}


void AbTop::writeOptions(KConfig *config)
{
  config->writeEntry("Level", LEVEL[levelAction->currentItem()].key);
  config->writeEntry("Computer", IPLAY[iplayAction->currentItem()].key);

  config->writeEntry("MoveSlow", showMoveLong);
  config->writeEntry("RenderBalls", renderBalls);
  config->writeEntry("ShowSpy", showSpy);
}

void AbTop::saveProperties(KConfig *config)
{
  writeOptions(config);
  if (currentEvalScheme)
    currentEvalScheme->save(config);

  config->writeEntry("TimerState", timerState);

  if (timerState == noGame) return;

  config->writeEntry("GameStopped", stop);
  config->writeEntry("Position", board->getState(moveNo));
  config->sync();
}

void AbTop::savePosition()
{
  KConfig* config = kapp->config();
  config->setGroup("SavedPosition");
  config->writeEntry("Position", board->getState(moveNo));
}

void AbTop::restorePosition()
{
  KConfig* config = kapp->config();
  config->setGroup("SavedPosition");
  QString  entry = config->readEntry("Position");

  timerState = notStarted;
  timer->stop();
  board->begin(Board::color1);
  stop = false;
  setMoveNo( board->setState(entry), true );

  if (net)
    net->broadcast( board->getASCIIState( moveNo ).ascii() );

  boardWidget->updatePosition(true);

  playGame();
}

void AbTop::setupStatusBar()
{
  QString tmp;

  QString t = i18n("Press %1 for a new game").arg( newAction->shortcut().toString());
  statusLabel = new QLabel( t, statusBar(), "statusLabel" );
  statusBar()->addWidget(statusLabel,1,false);

  // PERMANENT: Moving side + move No.

  // validPixmap, only visible in Modify mode: is position valid ?
  warningPix = BarIcon( "warning" );
  okPix = BarIcon( "ok" );
  validLabel = new QLabel( "", statusBar(), "validLabel" );
  validLabel->setFixedSize( 18, statusLabel->sizeHint().height() );
  validLabel->setAlignment( AlignCenter );
  validLabel->hide();
  validShown = false;

  redBall = BarIcon( "redball" );
  yellowBall = BarIcon( "yellowball" );
  noBall = BarIcon( "noball" );
  ballLabel = new QLabel( "", statusBar(), "ballLabel" );
  ballLabel->setPixmap(noBall);
  ballLabel->setFixedSize( 18, statusLabel->sizeHint().height() );
  ballLabel->setAlignment( AlignCenter );
  statusBar()->addWidget(ballLabel, 0, true);

  moveLabel = new QLabel( i18n("Move %1").arg("--"), statusBar(), "moveLabel" );
  statusBar()->addWidget(moveLabel, 0, true);

#ifdef MYTRACE
  /* Create a toolbar menu for debugging output level */
  KToolBar *tb = toolBar("mainToolBar");
  if (tb) {
    QPopupMenu* spyPopup = new QPopupMenu;
    spy0 = BarIcon( "spy0" );
    spy1 = BarIcon( "spy1" );
    spy2 = BarIcon( "spy2" );
    spy3 = BarIcon( "spy3" );
    spyPopup->insertItem(spy0, 0);
    spyPopup->insertItem(spy1, 1);
    spyPopup->insertItem(spy2, 2);
    spyPopup->insertItem(spy3, 3);
    connect( spyPopup, SIGNAL(activated(int)),
	     this, SLOT(setSpy(int)) );
    tb->insertButton(spy0, 30, spyPopup,
			    TRUE, i18n("Spy"));
  }
#endif

}



void AbTop::updateSpy(QString s)
{
  if (showSpy) {
    if (s.isEmpty()) {
      updateStatus();
      //      statusBar()->clear();
    }
    else
      statusLabel->setText(s);
  }
}

void AbTop::updateBestMove(Move& m, int value)
{
  if (showSpy) {
    boardWidget->showMove(m,3);
    boardWidget->showMove(m,0,false);

    QString tmp;
    tmp.sprintf("%s : %+d", (const char*) m.name().utf8(), value-actValue);
    updateSpy(tmp);
    kapp->processEvents();
  }
}


void AbTop::updateStatus()
{
  QString tmp;
  bool showValid = false;

  if (!editMode && timerState == noGame) {
    tmp = i18n("Move %1").arg("--");
    ballLabel->setPixmap(noBall);
  }
  else {
    tmp = i18n("Move %1").arg(moveNo/2 + 1);
    ballLabel->setPixmap( (board->actColor() == Board::color1)
			  ? redBall : yellowBall);
  }
  moveLabel->setText(tmp);

  if (editMode) {
    tmp = QString("%1: %2 %3 - %4 %5")
      .arg( i18n("Edit") )
      .arg( i18n("Red") ).arg(boardWidget->getColor1Count())
      .arg( i18n("Yellow") ).arg(boardWidget->getColor2Count());
    validLabel->setPixmap( (board->validState() == Board::invalid)
			   ? warningPix:okPix );
    showValid = true;
  }
  else if (timerState == noGame) {
    tmp = i18n("Press %1 for a new game").arg( newAction->shortcut().toString());
  }
  else {
    if (timerState == gameOver) {
      tmp = (board->actColor() == Board::color2) ?
	      i18n("Red won"):i18n("Yellow won");
      validLabel->setPixmap( warningPix );
      showValid = true;
    }
    else {
      tmp = QString("%1 - %2")
	.arg( (board->actColor() == Board::color1) ?
	      i18n("Red"):i18n("Yellow") )
	.arg( iPlayNow() ?
	      i18n("I am thinking...") : i18n("It is your turn!") );
    }
  }
  statusLabel->setText(tmp);
  if (validShown != showValid) {
    if (showValid) {
      statusBar()->addWidget(validLabel);
      validLabel->show();
    }
    else {
      statusBar()->removeWidget(validLabel);
      validLabel->hide();
    }
    validShown = showValid;
  }
  statusBar()->clear();
  statusBar()->repaint();
}

void AbTop::edited(int vState)
{
  if (vState == Board::empty)
    timerState = noGame;

  updateStatus();
}

/* only <stop search>, <hint>, <take back> have to be updated */
void AbTop::updateActions()
{
  bool iPlay = iPlayNow();

  /* New && Copy always on */

  /* Paste */
  pastePossible = !iPlay;
  pasteAction->setEnabled(!iPlay);

  /* Edit */
  editAction->setEnabled(!iPlay);

  /* Stop search */
  stopAction->setEnabled(iPlay);

  /* Back */
  bool bBack = (editMode && moveNo>0) ||
    (board->movesStored() >=1 && !iPlay);
  backAction->setEnabled(bBack);

  /* Forward */
  bool bForward = editMode && moveNo<999;
  forwardAction->setEnabled(bForward);

  /* Hint */
  bool bHint = !editMode && !iPlay && (haveHint().type != Move::none);
  hintAction->setEnabled(bHint);
}

/* let the program be responsive even in a long search... */
void AbTop::searchBreak()
{
  kapp->processEvents();
}


void AbTop::setSpy(int id )
{
  toolBar("mainToolBar")->setButtonPixmap(30, (id==0)?spy0:(id==1)?spy1:(id==2)?spy2:spy3 );
  spyLevel = id;
  board->setSpyLevel(spyLevel);
}

void AbTop::timerDone()
{
  int interval = 400;

  switch(timerState) {
  case noGame:
  case notStarted:
    return;
  case showMove:
  case showMove+2:
  case showSugg:
  case showSugg+2:
  case showSugg+4:
    boardWidget->showMove(actMove, 2);
    interval = 200;
    break;
  case showMove+1:
  case showMove+3:
  case showSugg+1:
  case showSugg+3:
    boardWidget->showMove(actMove, 3);
    break;
  case showSugg+5:
    interval = 800;
  case showMove+4:
    boardWidget->showMove(actMove, 4);
    break;
  case showMove+5:
    boardWidget->showMove(actMove, 0);
    timerState = moveShown;
    playGame();
    return;
  case showSugg+6:
    boardWidget->showMove(actMove, 0);
    timerState = notStarted;
    return;
  }
  timerState++;
  timer->start(interval,TRUE);
}

void AbTop::userMove()
{
    /* User has to move */
    static MoveList list;

    list.clear();
    board->generateMoves(list);

    if (list.getLength() == 0) {
      stop = true;
      timerState = gameOver;
      playGame();
    }
    else
      boardWidget->choseMove(&list);
}

bool AbTop::iPlayNow()
{
  if (editMode ||
      (board->validState() != Board::valid) ||
      timerState == gameOver)
    return false;

  int c = board->actColor();

  /* color1 is red */
  return ((iplay == Both) ||
	  ((c == Board::color1) && (iplay == Red) ) ||
	  ((c == Board::color2) && (iplay == Yellow) ));
}

void AbTop::playGame()
{
  if (timerState == moveShown) {
    if (actMove.type != Move::none) {
      board->playMove(actMove);
      moveNo++; // actColor in board is changed in playMove

      if (net)
	net->broadcast( board->getASCIIState( moveNo ).ascii() );
    }
    actValue = - board->calcEvaluation();
    boardWidget->updatePosition(true);
    timerState = notStarted;
  }
  if (!board->isValid()) {
    stop = true;
    timerState = gameOver;
  }

  updateStatus();
  updateActions();
  boardWidget->setCursor(crossCursor);
  if (stop) return;


  if (!iPlayNow()) {
    userMove();
    return;
  }
  boardWidget->setCursor(waitCursor);
  kapp->processEvents();

  if (moveNo <4) {
    /* Chose a random move making the position better for actual color */

    /* If comparing ratings among color1/2 on move, we have to negate one */
    int v = -board->calcEvaluation(), vv;
    do {
      actMove = board->randomMove();
      board->playMove(actMove);
      vv = board->calcEvaluation();
      board->takeBack();
    } while( (board->actColor() == Board::color1) ? (vv<v) : (vv>v) );
  }
  else {
    actMove = (board->bestMove());

    if (actMove.type == Move::none) {
      stop = true;
      timerState = gameOver;
      playGame();
      return;
    }
  }

  timerState = showMoveLong ? showMove : showMove+3;
  timerDone();
}

void AbTop::moveChoosen(Move& m)
{
  actMove = m;
  timerState = moveShown;
  playGame();
}

void AbTop::newGame()
{
  /* stop a running animation */
  timerState = notStarted;
  timer->stop();

  /* reset board */
  board->begin(Board::color1);
  boardWidget->updatePosition(true);
  setMoveNo(0, true);

  if (net)
    net->broadcast( board->getASCIIState( moveNo ).ascii() );

  /* if not in EditMode, start Game immediately */
  if (!editMode) {
    stop = false;
    playGame();
  }
}

/* Copy ASCII representation into Clipboard */
void AbTop::copy()
{
  QClipboard *cb = QApplication::clipboard();
  cb->setText( board->getASCIIState( moveNo ).ascii() );
}

void AbTop::paste()
{
  if (!pastePossible) return;

  QClipboard *cb = QApplication::clipboard();
  pastePosition( cb->text().ascii() );
  /* don't do this in pastePosition: RECURSION !! */

  if (net)
    net->broadcast( board->getASCIIState( moveNo ).ascii() );
}

void AbTop::pastePosition(const char * text)
{
  if (!pastePossible) return;
  if ( text ) {
    timerState = notStarted;
    timer->stop();
    board->begin(Board::color1);
    stop = false;

    int mNo = board->setASCIIState(text);
    if (mNo<0) mNo=0;
    setMoveNo( mNo, true);

    boardWidget->updatePosition(true);

    if ( (board->validState()==Board::invalid) && !editMode) {
      editAction->setChecked(true);
      return;
    }

    playGame();
  }
}


void AbTop::gameNetwork(bool on)
{
  if (!on) {
    if (net != 0) {
      delete net;
      net = 0;
    }
    return;
  }

  if (myPort == 0) myPort = Network::defaultPort;
  net = new Network(myPort);
  char *h, h2[100];
  int p, i;
  for(h = hosts.first(); h!=0; h=hosts.next()) {
    for(i=0;h[i]!=0 && h[i]!=':';i++);
    if (h[i]==':')
      p = atoi(h+i+1);
    else
      p = 0;

    if (p == 0) p = Network::defaultPort;
    strncpy(h2,h,i);
    h2[i]=0;
    net->addListener(h2, p);
  }
  QObject::connect(net, SIGNAL(gotPosition(const char *)),
		   this, SLOT(pastePosition(const char *)) );
}


void AbTop::editModify(bool on)
{
  int vState = board->validState();

  editMode = boardWidget->setEditMode( on );
  if (vState != Board::valid)
    timerState = noGame;

  updateActions();
  updateStatus();
  if (!editMode && vState == Board::valid) {
    actMove.type = Move::none;
    timerState = moveShown;
    playGame();
  }
}

void AbTop::stopGame()
{
  stop = true;
  board->stopSearch();
}

void AbTop::stopSearch()
{
  // When computer plays both, switch back to human for next color
  if (iplay == Both) {
      if (board->actColor() == Board::color1) setIPlay(Red);
      else setIPlay(Yellow);
  }
  board->stopSearch();
}

bool AbTop::queryClose()
{
  board->stopSearch();
  return true;
}

void AbTop::continueGame()
{
  if (timerState != noGame && timerState != gameOver) {
    stop = false;
    if (timerState == notStarted)
	    playGame();
  }
}

/**
 * Reset the Move number of the actual game to <m>
 * If <update> is true, update GUI actions and redraw statusbar
 */
void AbTop::setMoveNo(int m, bool updateGUI)
{
  moveNo = m;

  board->setActColor( ((moveNo%2)==0) ? Board::color1 : Board::color2 );

  if (updateGUI) {
    updateStatus();
    updateActions();
  }
}


/* "Back" action activated
 *
 * If in edit mode, simple go 1 back
 * If in a game, go back 2 if possible
 */
void AbTop::back()
{
  if (editMode) {
    if (moveNo > 0)
      setMoveNo(moveNo-1, true);
    return;
  }

  if (moveNo < 1) return;

  if (timerState == gameOver)
    timerState = notStarted;
  if (timerState != notStarted) return;

  /* If possible, go 2 steps back */
  if (moveNo>0 && board->takeBack()) moveNo--;
  if (moveNo>0 && board->takeBack()) moveNo--;
  setMoveNo( moveNo, true );

  boardWidget->updatePosition(true);

  userMove();
}

/* Only for edit Mode */
void AbTop::forward()
{
  if (editMode) {
    if (moveNo < 999)
      setMoveNo(moveNo+1, true);
    return;
  }
}

Move AbTop::haveHint()
{
  static Move m;
  static int oldMoveNo = 0;

  if (timerState != notStarted) {
    m.type = Move::none;
  }
  else if (moveNo != oldMoveNo) {
    MoveList list;

    oldMoveNo = moveNo;
    m = board->nextMove();
    board->generateMoves(list);
    if (!list.isElement(m,0))
      m.type = Move::none;
  }
  return m;
}


void AbTop::suggestion()
{
  if (timerState != notStarted) return;
  Move m = haveHint();
  if (m.type == Move::none) return;

  actMove = m;

  timerState = showSugg;
  timerDone();
}

void AbTop::setLevel(int l)
{
    levelAction->setCurrentItem(l);
    depth = l+2;
    board->setDepth(depth);
  //  kdDebug(12011) << "Level set to " << d << endl;
}

void AbTop::setIPlay(int i)
{
    iplayAction->setCurrentItem(i);
    iplay = (IPlay)i;
    continueGame();
}

void AbTop::optionMoveSlow(bool on)
{
  showMoveLong = on;
}

void AbTop::optionRenderBalls(bool on)
{
  renderBalls = on;
  boardWidget->renderBalls(renderBalls);
}

void AbTop::optionShowSpy(bool on)
{
  showSpy = on;
  board->updateSpy(showSpy);

#ifdef SPION
  if (showSpy)
    spy->show();
  else {
    spy->nextStep();
    spy->hide();
  }
#endif

}


#include "AbTop.moc"
