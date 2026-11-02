/* Class Spion
 *
 * Josef Weidendorfer, 10/97
 */


#include <qgroupbox.h>
#include <qlayout.h>
#include <qlabel.h>

#include <klocale.h>
#include <kapplication.h>

#include "BoardWidget.h"
#include "Spy.h"

Spy::Spy(Board& b)
  :board(b)
{
  int i;

  top = new QVBoxLayout(this, 5);
  
  QLabel *l = new QLabel(this);
  l->setText( i18n("Actual examined position:") );
  l->setFixedHeight( l->sizeHint().height() );
  l->setAlignment( AlignVCenter | AlignLeft );
  top->addWidget( l );

  QHBoxLayout* b1 = new QHBoxLayout();
  top->addLayout( b1, 10 );
	
  for(i=0;i<BoardCount;i++) {
    QVBoxLayout *b2 = new QVBoxLayout();
    b1->addLayout( b2 );

    actBoard[i] = new BoardWidget(board,this);
    actLabel[i] = new QLabel(this);
    actLabel[i]->setText("---");
    // actLabel[i]->setFrameStyle( QFrame::Panel | QFrame::Sunken );
    actLabel[i]->setAlignment( AlignHCenter | AlignVCenter);
    actLabel[i]->setFixedHeight( actLabel[i]->sizeHint().height() );

    b2->addWidget( actBoard[i] );
    b2->addWidget( actLabel[i] );
    connect( actBoard[i], SIGNAL(mousePressed()), this, SLOT(nextStep()) );
  }

  l = new QLabel(this);
  l->setText( i18n("Best move so far:") );
  l->setFixedHeight( l->sizeHint().height() );
  l->setAlignment( AlignVCenter | AlignLeft );
  top->addWidget( l );

  b1 = new QHBoxLayout();
  top->addLayout( b1, 10 );
	
  for(i=0;i<BoardCount;i++) {
    QVBoxLayout *b2 = new QVBoxLayout();
    b1->addLayout( b2 );

    bestBoard[i] = new BoardWidget(board,this);
    bestLabel[i] = new QLabel(this);
    bestLabel[i]->setText("---");
    //    bestLabel[i]->setFrameStyle( QFrame::Panel | QFrame::Sunken );
    bestLabel[i]->setAlignment( AlignHCenter | AlignVCenter);
    bestLabel[i]->setFixedHeight( bestLabel[i]->sizeHint().height() );

    b2->addWidget( bestBoard[i] );
    b2->addWidget( bestLabel[i] );
    connect( bestBoard[i], SIGNAL(mousePressed()), this, SLOT(nextStep()) );
  }
  
  connect( &board, SIGNAL(update(int,int,Move&,bool)), 
	   this, SLOT(update(int,int,Move&,bool)) );
  connect( &board, SIGNAL(updateBest(int,int,Move&,bool)), 
	   this, SLOT(updateBest(int,int,Move&,bool)) );
  top->activate();
  setCaption(i18n("Spy"));
  // KWM::setDecoration(winId(), 2);
  resize(500,300);
}

void Spy::keyPressEvent(QKeyEvent *)
{
	nextStep();
}

void Spy::nextStep()
{
  next = true;
}

void Spy::clearActBoards()
{
  for(int i=0;i<BoardCount;i++) {
    actBoard[i]->clearPosition();
    actBoard[i]->draw();
    actLabel[i]->setText("---");
  }
}

void Spy::update(int depth, int value, Move& m, bool wait)
{
	next = false;
	
	if (depth>BoardCount-1) return;
	actBoard[depth]->showMove(m,3);
	//	actBoard[depth]->showMove(m,0,false);

	if (!wait) {
	  actLabel[depth]->setText("---");
	  return;
	}

	if (depth<BoardCount-1) {
	  board.playMove(m);
	  actBoard[depth+1]->updatePosition(true);
	  actLabel[depth+1]->setNum(value);
	  board.takeBack();

	  if (depth<BoardCount-2) {
	    actBoard[depth+2]->clearPosition();
	    actBoard[depth+2]->draw();
	  }
	}

	while(!next)
	  kapp->processEvents();
}

void Spy::updateBest(int depth, int value, Move& m, bool cutoff)
{
  if (depth>BoardCount-1) return;
  bestBoard[depth]->showMove(m,3);
  //  board.showMove(m,0);

  if (depth<BoardCount-1) {
    board.playMove(m);
    bestBoard[depth+1]->updatePosition(true);

    QString tmp;
    tmp.setNum(value);
    if (cutoff) tmp += " (CutOff)";
    bestLabel[depth+1]->setText(tmp);
    board.takeBack();
  }
}






#include "Spy.moc"
