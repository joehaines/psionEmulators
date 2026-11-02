/**
 * This file is part of the ksnake package
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "rattler.h"

#include <qtimer.h>
#include <qlabel.h>
#include <kapplication.h>
#include <kstdgameaction.h>
#include <kdebug.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <qbitarray.h>
#include <kstandarddirs.h>

#include "level.h"
#include "basket.h"
#include "settings.h"

QBitArray gameState(5);
QLabel *label = 0;
int speed[4] = { 130, 95, 55, 40 };

Rattler::Rattler( QWidget *parent, const char *name )
	: QWidget( parent, name )
{
	setFocusPolicy(QWidget::StrongFocus);

	numBalls = Settings::balls();
	ballsAI = Settings::ballsAI();
	numSnakes = Settings::computerSnakes();
	snakesAI = Settings::snakesAI();
	skill = Settings::skill();
	room = Settings::startingRoom();

	board = new Board(35*35);
	level = new Level(board);
	pix = new PixServer(board);
	basket = new Basket(board, pix);
	samy = new SamySnake(board, pix);

	computerSnakes = new QPtrList<CompuSnake>;
	computerSnakes->setAutoDelete( TRUE );

	balls = new QPtrList<Ball>;
	balls->setAutoDelete( TRUE );

	connect( samy, SIGNAL(closeGate(int)), this, SLOT(closeGate(int)));
	connect( samy, SIGNAL(score(bool, int)), this, SLOT(scoring(bool,int)));
	connect( samy, SIGNAL(goingOut()), this, SLOT(speedUp()));
	connect( basket, SIGNAL(openGate()), this, SLOT(openGate()));

	gameState.fill(FALSE);
	gameState.setBit(Demo);

	timerCount = 0;
	QTimer::singleShot( 2000, this, SLOT(demo()) ); // Why wait?

	backgroundPixmaps =
		KGlobal::dirs()->findAllResources("appdata", "backgrounds/*.png");
}

Rattler::~Rattler()
{
	delete level;
	delete balls;
	delete computerSnakes;
	delete basket;
	delete samy;
	delete pix;
	delete board;
}

/**
 * One of the settings changed, load our settings
 */ 
void Rattler::loadSettings(){
	setBalls(Settings::balls());
	setBallsAI(Settings::ballsAI());
	setCompuSnakes(Settings::computerSnakes());
	setSnakesAI(Settings::snakesAI());
	setSkill(Settings::skill());
	setRoom(Settings::startingRoom());
	reloadRoomPixmap();
}

void Rattler::paintEvent( QPaintEvent *e)
{
	QRect rect = e->rect();

	if (rect.isEmpty())
	  return;
	QPixmap levelPix = pix->levelPix();

	basket->repaint(true);
	
	bitBlt(this, rect.x(), rect.y(),
			  &levelPix, rect.x(), rect.y(), rect.width(), rect.height());
}

void Rattler::timerEvent( QTimerEvent * )
{
	timerCount++;

	if ( !leaving )		// advance progressBar unless Samy
		emit advance();	// is going out

	for (CompuSnake *c = computerSnakes->first(); c != 0; c = computerSnakes->next()){
		if(c) {
				c->nextMove();
				c->repaint(false);
				}
	}

	for (Ball *b = balls->first(); b != 0; b = balls->next()){
		if (b) {
			b->nextMove();
			b->repaint();
		}
	}


	samyState state = ok;

	if(!gameState.testBit(Demo))
	{
		state = samy->nextMove(direction);
		samy->repaint( false );
	}

	basket->repaint( false);

	if (state == ko)
		newTry();
	else if (state == out)
		levelUp();

	bitBlt(this, 0, 0, &pix->levelPix(), 0, 0, rect().width(), rect().height());
}

void Rattler::keyPressEvent( QKeyEvent *k )
{
	if (gameState.testBit(Paused))
		return;
 	
	KKey key(k);
	if(actionCollection->action("Pl1Up")->shortcut().contains(key))
		direction = N;
	else if(actionCollection->action("Pl1Down")->shortcut().contains(key))
		direction = S;
	else if(actionCollection->action("Pl1Right")->shortcut().contains(key))
		direction = E;
	else if(actionCollection->action("Pl1Left")->shortcut().contains(key))
		direction = W;
	else if ((k->key() == Key_Return) ||
           (k->key() == Key_Enter) || (k->key() == Key_Space)) {
		if (!gameState.testBit(Demo)) {
		   k->ignore();
		   return;
		}
		restart();
	}
	else {
		k->ignore();
		return;
	}
	k->accept();
}

void Rattler::mousePressEvent( QMouseEvent *e )
{
	if (gameState.testBit(Paused))
		return;

	uint button = e->button();
	if (button == Qt::RightButton)
	{
		switch (direction)
	{
	case N: direction=E;
		break;
	case E: direction=S;
		break;
	case S: direction=W;
		break;
	case W: direction=N;
		break;
	}
	e->accept();
	}
	else if (button == Qt::LeftButton)
	{
		switch (direction)
	{
	case N: direction=W;
		break;
	case E: direction=N;
		break;
	case S: direction=E;
		break;
	case W: direction=S;
		break;
	}
	e->accept();
	}
	else
		e->ignore();
}

void Rattler::closeGate(int i)
{
	board->set(i, brick);
	pix->restore(i);
}

void Rattler::openGate()
{
	board->set(NORTH_GATE, empty);
	pix->erase(NORTH_GATE);
}

void Rattler::scoring(bool win, int i)
{
	Fruits fruit  = basket->eaten(i);

	if (gameState.testBit(Demo))
		win = TRUE;

	int p = 0;

	switch (fruit) {

	case Red:
	if (win) {
		if (!timerHasRunOut)
		p = 1 + skill*2;
		else p = 1;
	}
	else p = -2;
	break;

	case Golden:
	if (win) {
		if (!timerHasRunOut)
		p = 2 + (skill*2) + (numSnakes*2) + (numBalls+2);
		else p = 2;
	}
	else p = -5;
	break;

	default:
	break;

	}
	score(p);
}

void Rattler::score(int p)
{
	points += p;
	points = (points < 0 ? 0 : points);
	emit setPoints(points);

	while (points > check*50) {
		check++;
		if (trys < 7 && !gameState.testBit(Demo))
		emit setTrys(++trys);
	}
}

void Rattler::killedComputerSnake()
{
	if (!gameState.testBit(Demo))
		score(20);
}

void Rattler::pause()
{
	if (gameState.testBit(Init))
		return;

	if (gameState.testBit(Playing)) {

		gameState.toggleBit(Playing);
		gameState.setBit(Paused);
		stop();

		KAction* tempPauseAction = KStdGameAction::pause();

		label = new QLabel(this);
		label->setFont( QFont( "Times", 14, QFont::Bold ) );
		label->setText(i18n("Game Paused\n Press %1 to resume\n")
							.arg(tempPauseAction->shortcutText()));
		label->setAlignment( AlignCenter );
		label->setFrameStyle( QFrame::Panel | QFrame::Raised );
		label->setGeometry(182, 206, 198, 80);
		label->show();
	
		delete tempPauseAction;
		//emit togglePaused();
	}
	else if (gameState.testBit(Paused)) {
		gameState.toggleBit(Paused);
		gameState.setBit(Playing);
		start();
		cleanLabel();
		//emit togglePaused();
	}
}

void Rattler::cleanLabel()
{
	if (label) {
		delete label;
		label = 0;
	}
}

void Rattler::restartDemo()
{
	if (!gameState.testBit(Demo))
		return;

	int r = 50000+ (kapp->random() % 30000);
	QTimer::singleShot( r, this, SLOT(restartDemo()) );

	stop();
	level->create(Intro);
	pix->initRoomPixmap();
	init(FALSE);
	repaint();
	start();
}

void Rattler::demo()
{
	static  bool first_time = TRUE;

	if(gameState.testBit(Init) || gameState.testBit(Playing))
		return;

	stop();

	QTimer::singleShot( 60000, this, SLOT(restartDemo()) );
	gameState.fill(FALSE);
	gameState.setBit(Init);
	gameState.setBit(Demo);
	resetFlags();

	if(!first_time) {
	level->create(Intro);
	pix->initRoomPixmap();
	}
	repaint(rect(), FALSE);
	init(FALSE);
	run();
	first_time = FALSE;
}

void Rattler::restart()
{
	if (gameState.testBit(Init))
		return;
	stop();

	if (gameState.testBit(Paused) || gameState.testBit(Playing)) {

	switch( KMessageBox::questionYesNo( this,
						i18n("A game is already started.\n"
						 "Start a new one?\n"),  i18n("Snake Race"))) {
	case KMessageBox::No:
		if (!gameState.testBit(Paused))
			start();
			return;
		break;
	case KMessageBox::Yes:
		;
	}
	}

	gameState.fill(FALSE);
	gameState.setBit(Init);
	gameState.setBit(Playing);

	resetFlags();

	level->setLevel(room);
	level->create(Banner);
	pix->initRoomPixmap();

	cleanLabel();

	repaint();
	QTimer::singleShot( 2000, this, SLOT(showRoom()) );
}

void Rattler::newTry()
{
	stop();

	if(trys==0) {
		gameState.fill(FALSE);
		gameState.setBit(Over);
		level->create(GameOver);
		pix->initRoomPixmap();
		repaint();
		QTimer::singleShot( 5000, this, SLOT(demo()) );
		emit setScore(points);
		return;
	}
	--trys;
	gameState.fill(FALSE);
	gameState.setBit(Init);
	gameState.setBit(Playing);

	level->create(Room);
	pix->initRoomPixmap();
	init(TRUE);
	repaint();
	QTimer::singleShot( 1000, this, SLOT(run()) );
}

void Rattler::levelUp()
{
	stop();

	gameState.fill(FALSE);
	gameState.setBit(Init);
	gameState.setBit(Playing);

	score (2*(level->getLevel())+(2*numSnakes)+(2*numBalls)+(2*skill));

	level->nextLevel();
	level->create(Banner);
	pix->initRoomPixmap();
	repaint();

	QTimer::singleShot( 2000, this, SLOT(showRoom()) );
}

/* this slot is called by the progressBar  when value() == 0
or by a compuSnake wich manages to exit */
void Rattler::restartTimer()
{
	timerHasRunOut = TRUE;
	timerCount = 0;
	emit rewind();

	if ( board->isEmpty(NORTH_GATE) )
	  closeGate(NORTH_GATE);
	basket->newApples();
}

void Rattler::speedUp()
{
	leaving = TRUE;
	stop();
	start( 30 );
}

void Rattler::resetFlags()
{
	trys = 2;
	check  = 1;
	points = 0;
}

void Rattler::showRoom()
{
	level->create(Room);
	pix->initRoomPixmap();
	init(TRUE);
	repaint();
	QTimer::singleShot( 1000, this, SLOT(run()) );
}

void Rattler::init(bool play)
{
	leaving = FALSE;
	timerHasRunOut = FALSE;
	timerCount = 0;
	emit rewind();

	emit setTrys(trys);
	emit setPoints(points);

	basket->clear();
	basket->newApples();
	restartBalls(play);
	restartComputerSnakes(play);
	if(play)
		samy->init();
}

void Rattler::run()
{
	direction = N;
	gameState.toggleBit(Init);
	start();
}

void Rattler::start()
{
	gameTimer = startTimer( speed [skill] );
}

void Rattler::start(int t)
{
	gameTimer = startTimer(t);
}

void Rattler::stop()
{
	killTimers();
}

void Rattler::restartComputerSnakes(bool play)
{
	if( !computerSnakes->isEmpty())
		computerSnakes->clear();

	int i  = (play == FALSE && numSnakes == 0 ? 1 : numSnakes);

	for (int x = 0; x < i; x++) {
		CompuSnake *as;
		switch(snakesAI) {
			default: // random.
				as = new CompuSnake(board, pix);
				break;
			case 1: // eater.
				as = new EaterCompuSnake(board, pix);
				break;
			case 2: // killer.
				as = new KillerCompuSnake(board, pix);
				break;
		}
		connect( as, SIGNAL(closeGate(int)), this, SLOT(closeGate(int)));
		connect( as, SIGNAL(restartTimer()), this, SLOT(restartTimer()));
		connect( as, SIGNAL(score(bool, int)), this, SLOT(scoring(bool,int)));
		connect( as, SIGNAL(killed()), this, SLOT(killedComputerSnake()));
		computerSnakes->append(as);
	}
}

void Rattler::restartBalls(bool play)
{
	if( !balls->isEmpty())
		balls->clear();

	int i = (play == FALSE && numBalls == 0 ? 1 : numBalls);

	for (int x = 0; x < i; x++) {
		Ball *b;
		switch(ballsAI) {
			default: // default.
				b = new Ball(board, pix);
				break;
			case 1: // dumb.
				b = new DumbKillerBall(board, pix);
				break;
			case 2: // smart.
				b = new KillerBall(board, pix);
				break;
		}
		balls->append(b);
	}
}

void Rattler::setBalls(int numBalls)
{
	int count = balls->count();

	if (!gameState.testBit(Playing) ||
      !gameState.testBit(Demo) ||
       numBalls == count)
		return;

	while ( numBalls > count) {
		Ball *b = new Ball(board, pix);
		balls->append(b);
		numBalls--;
	}
	while (numBalls < count) {
		Ball *b = balls->getLast();
		b->zero();
		balls->removeLast();
		numBalls++;
	}
}

void Rattler::setBallsAI(int i)
{
	ballsAI = i;
}

void  Rattler::resizeEvent( QResizeEvent * )
{
	pix->initPixmaps();
	pix->initBrickPixmap();
	pix->initbackPixmaps();
	pix->initRoomPixmap();
}

void Rattler::setCompuSnakes(int i)
{
	CompuSnake *cs;
	numSnakes = i;
	int count = computerSnakes->count();

	if (gameState.testBit(Playing) || gameState.testBit(Demo)) {
	if ( i > count) {
		while ( i > count) {
				CompuSnake *as;
				switch(snakesAI) {
					default: // random.
						as = new CompuSnake(board, pix);
						break;
					case 1: // eater.
						as = new EaterCompuSnake(board, pix);
						break;
					case 2: // killer.
						as = new KillerCompuSnake(board, pix);
						break;
				}
		connect( as, SIGNAL(closeGate(int)), this, SLOT(closeGate(int)));
		connect( as, SIGNAL(restartTimer()), this, SLOT(restartTimer()));
		connect( as, SIGNAL(score(bool, int)), this, SLOT(scoring(bool,int)));
		connect( as, SIGNAL(killed()), this, SLOT(killedComputerSnake()));
		computerSnakes->append(as);
		i--;
		}
	}
	else if (i < count) {
		while (i < count) {
		cs = computerSnakes->getLast();
		cs->zero();
		computerSnakes->removeLast();
		i++;
		}
	}
	}
}

void Rattler::setSnakesAI(int i)
{
	snakesAI = i;
}

void Rattler::setSkill(int i)
{
	skill = i;
	if (gameState.testBit(Playing) || gameState.testBit(Demo)) {
		stop();
		start();
	}
}

void Rattler::setRoom(int i)
{
	room = i;
}

void Rattler::reloadRoomPixmap()
{
	pix->initbackPixmaps();
	pix->initRoomPixmap();
	demo();
}

#include "rattler.moc"

