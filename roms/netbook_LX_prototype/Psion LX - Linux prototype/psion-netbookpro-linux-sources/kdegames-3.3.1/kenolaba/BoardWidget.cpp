/* Implementation of class BoardWidget
 * 
 * This class handles rendering of a Board to a KDE/QT widget,
 * shows moves (with a timer) and manages input of moves
 *
 * Josef Weidendorfer, 9/97
 */

#include <qbitmap.h>
#include <qpainter.h>
#include <qcursor.h>

#include <klocale.h>
#include <kdebug.h>

#ifdef HAVE_KIR
#include <kimgrender.h>
#endif

#include "Board.h"
#include "BoardWidget.h"

/* Cursors */
#include "bitmaps/Arrow1"
#include "bitmaps/Arrow1Mask"
#include "bitmaps/Arrow2"
#include "bitmaps/Arrow2Mask"
#include "bitmaps/Arrow3"
#include "bitmaps/Arrow3Mask"
#include "bitmaps/Arrow4"
#include "bitmaps/Arrow4Mask"
#include "bitmaps/Arrow5"
#include "bitmaps/Arrow5Mask"
#include "bitmaps/Arrow6"
#include "bitmaps/Arrow6Mask"

BoardWidget::BoardWidget(Board& b, QWidget *parent, const char *name)
  : BallWidget(10,9,parent, name), board(b)
{
  pList =0;
  gettingMove = false;
  editMode = false;
  renderMode = false;

#ifdef HAVE_KIR
  m_backRenderer = KIRManager::attach( this, "Background" );
  connect( m_backRenderer, SIGNAL(rendered()),
	   this, SLOT(drawBoard()) );
#endif

  /* setup cursors */
	
#define createCursor(bitmap,name) \
  static QBitmap bitmap(bitmap##_width, bitmap##_height,               \
                        (unsigned char *) bitmap##_bits, TRUE);        \
  static QBitmap bitmap##Mask(bitmap##Mask_width, bitmap##Mask_height, \
			(unsigned char *) bitmap##Mask_bits, TRUE);    \
  name = new QCursor(bitmap, bitmap##Mask, bitmap##_x_hot, bitmap##_y_hot);
	
  createCursor(Arrow1, arrow[1]);
  createCursor(Arrow2, arrow[2]);
  createCursor(Arrow3, arrow[3]);
  createCursor(Arrow4, arrow[4]);
  createCursor(Arrow5, arrow[5]);
  createCursor(Arrow6, arrow[6]);

  setCursor(crossCursor);

  //  boardColor   = new QColor("lightblue");
  boardColor   = new QColor(backgroundColor());
  redColor     = new QColor("red2");
  yellowColor  = new QColor("yellow2");
  redHColor    = new QColor("orange");
  yellowHColor = new QColor("green");

  initBalls();   
	
  updatePosition();
}

BoardWidget::~BoardWidget()
{
  for(int i=1; i<7; i++)
    if (arrow[i] != 0)
      delete arrow[i];

#ifdef HAVE_KIR  
  if (m_backRenderer != 0)
    delete m_backRenderer;
#endif
  delete boardColor;
  delete redColor;
  delete yellowColor;
  delete redHColor;
  delete yellowHColor;

}

void BoardWidget::configure()
{
#ifdef HAVE_KIR
  if (m_backRenderer != 0) {
    m_backRenderer->setup();
    m_backRenderer->manager()->saveModules();
  }
#endif
}
   

void BoardWidget::createPos(int pos, int i, int j, Ball* b)
{
  int x=(465*(2*(i)-(j))/9);
  int y=(500*19*(j)/100);
  createBallPosition(pos, x,y, b);
}
                                                 
void BoardWidget::initBalls()
{
  n2 = new Ball( *yellowColor );
  h2 = new Ball( *yellowHColor );
  d2 = new Ball( *yellowHColor, 3.14/2 );

  n1 = new Ball( *redColor );
  h1 = new Ball( *redHColor );
  d1 = new Ball( *redHColor, 3.14/2 );

  // e  = new Ball( white,0,0 );
  // e->setSpecials(.6,.85,.75);

  createBlending(1,10,h1,n1);
  createBlending(2,10,h1,d1);
  createBlending(3,10,h2,n2);
  createBlending(4,10,h2,d2);

  int i,j,pos;
  for(j=-4;j<5;j++)
    for(i= ((j>0)?j-4:-4) ; i< ((j<0)?5+j:5) ;i++) {
      pos=60+j*11+i;
      createPos(pos, i,j, 0);
    }

  pos = 0;
  /* the outer marks of color1 */
  for(i=0;i<3;i++) createPos(pos++, -6, i-4, 0 );
  for(i=0;i<3;i++) createPos(pos++, 2+i, i-4, 0 );

  /* the outer marks of color2 */
  for(i=0;i<3;i++) createPos(pos++, 6, 4-i, 0 );
  for(i=0;i<3;i++) createPos(pos++, -2-i, 4-i, 0 );
}  

void BoardWidget::resizeEvent(QResizeEvent *e)
{
  drawBoard();
  BallWidget::resizeEvent(e); 
}

void BoardWidget::moveEvent(QMoveEvent*)
{
  drawBoard();
}

void BoardWidget::paintEvent(QPaintEvent *)
{
  if (renderMode) {
    pm = boardPM;
    BallWidget::paint(&pm);
  }
  else
    draw();
  bitBlt(this, 0, 0, &pm);
}


void drawShadedHexagon(QPainter *p, int x, int y, int r, int lineWidth,
		       const QColorGroup& g, bool sunken)
{
  int dx=r/2, dy=(r*87)/100;
  int y1=y-dy, y2=y+dy;
  int i;

  QPen oldPen = p->pen();
    
  p->setPen(sunken ? g.midlight() : g.dark());

  for(i=0; i<lineWidth; i++) {
    p->drawLine( x-i+dx, y-dy, x+2*dx-i, y);
    p->drawLine( x+2*dx-i, y, x-i+dx, y+dy);
    p->drawLine( x-i+dx, y1+i, x+i-dx, y1+i);
  }

  p->setPen(sunken ? g.dark() : g.midlight());

  for(i=0; i<lineWidth; i++) {
    p->drawLine( x+i-dx, y-dy, x+i-2*dx, y);
    p->drawLine( x+i-2*dx, y, x+i-dx, y+dy);
    p->drawLine( x-i+dx, y2-i, x+i-dx, y2-i);
  }

  p->setPen(oldPen);
}


void drawColor(QPainter *p, int x, int y, int r, QColor* c)
{
  QColor w("white");
  QPalette pal(*c);

  QPen oldPen = p->pen();
  QBrush oldBrush = p->brush();

  p->setBrush(pal.active().dark());
  p->setPen(pal.active().dark());
  p->drawEllipse( x-r - 10,y-r +5, 2*r,2*r);

  p->setBrush(pal.active().mid());
  p->setPen(pal.active().mid());
  p->drawEllipse( x-r,y-r, 2*r,2*r);

  p->setBrush(pal.active().light());
  p->setPen(pal.active().light());
  p->drawEllipse( x-r/3, y-r/3, 4*r/3,4*r/3);

  p->setBrush(w);
  p->setPen(w);
  p->drawEllipse( x+r/3, y+r/3, r/3,r/3);

  p->setPen(oldPen);
  p->setBrush(oldBrush);
}  


void BoardWidget::drawBoard()
{
  boardPM.resize(width(), height());
  boardPM.fill(this, 0,0);

#ifndef HAVE_KIR
  QColorGroup g = QPalette( *boardColor ).active();
  QColorGroup g2 = QWidget::colorGroup();

  int boardSize = width() *10/12;
  if (boardSize > height()) boardSize = height();
  
  QPainter p;
  p.begin(&boardPM);
  p.setBrush(g2.brush(QColorGroup::Mid));
  
  QWMatrix m;
  QPoint cp = rect().center();
  m.translate(cp.x(), cp.y());
  m.scale(boardSize/1100.0, boardSize/1000.0);

  m.rotate(0);

  p.setWorldMatrix(m);

  /* draw field */

  int i,j;

  QPointArray a;
  int dx=520 /2, dy=(520 *87)/100;
  a.setPoints(6, -dx,-dy, dx,-dy, 2*dx,0, dx,dy, -dx,dy, -2*dx,0 );
  p.drawPolygon(a);

  drawShadedHexagon(&p, 0,0, 505, 1, g, false);
  drawShadedHexagon(&p, 0,0, 512, 3, g, true);
  drawShadedHexagon(&p, 0,0, 525, 5, g2, true);

#define xpos(i,j) (495*(2*(i)-(j))/9)
#define ypos(j)   (500*19*(j)/100)
	
  for(j=-4;j<5;j++)
    for(i= ((j>0)?j-4:-4) ; i< ((j<0)?5+j:5) ;i++) {
      int x=xpos(i,j);
      int y=ypos(j);
		  
      drawShadedHexagon(&p, x,y, 50, 2, g, true);
      drawShadedHexagon(&p, x,y, 30, 1, g, false);
    }
  p.end();
#endif
  draw();
}

void BoardWidget::renderBalls(bool r)
{
  renderMode=r;
  draw();
}

void BoardWidget::updateBalls()
{
  int i,j;
  
  for(j=-4;j<5;j++)
    for(i= ((j>0)?j-4:-4) ; i< ((j<0)?5+j:5) ;i++) {
      int pos = 60+j*11+i;
      int w=field[60+j*11+i];
      
      if (w==Board::color1) {
        if (positions[pos]->def != n1) {
          positions[pos]->def= n1;
          startAnimation(pos,1, ANIMATION_FORWARD);
        }
        else {
          stopAnimation(pos);
	}	
      }                         
      else if (w==Board::color1bright)
        startAnimation(pos,2,ANIMATION_LOOP);
      else if (w==Board::color2) {
        if (positions[pos]->def != n2) {
          positions[pos]->def= n2;
          startAnimation(pos,3,ANIMATION_FORWARD);
        }
        else {
          stopAnimation(pos);
	}
      }
      else if (w==Board::color2bright)
        startAnimation(pos,4,ANIMATION_LOOP);
      else if (w==Board::free) {
	positions[pos]->def= 0;
	positions[pos]->actAnimation = 0;
	//	stopAnimation(pos);
      }                         
    }
  
  for(i=0;i<6;i++)
    positions[i]->def= ((14-color1Count)>i && color1Count>0) ? n1:0;
  
  for(i=6;i<12;i++)
    positions[i]->def= ((14-color2Count)>i-6 && color2Count>0) ? n2:0;
  
}

void BoardWidget::draw()
{
  if (boardPM.isNull())
    return;

  pm = boardPM;

  if (renderMode) {
    updateBalls();
    repaint(false);
    return;
  }

  int boardSize = width() *10/12;
  if (boardSize > height()) boardSize = height();
    
  QPainter p;
  p.begin(&pm);
  p.setBrush(foregroundColor());
  
  QWMatrix m;
  QPoint cp = rect().center();
  m.translate(cp.x(), cp.y());
  m.scale(boardSize/1100.0, boardSize/1000.0);

  m.rotate(0);

  p.setWorldMatrix(m);

  /* draw fields */

  int i,j;

  for(j=-4;j<5;j++)
    for(i= ((j>0)?j-4:-4) ; i< ((j<0)?5+j:5) ;i++) {
      int x=xpos(i,j);
      int y=ypos(j);
      int w=field[60+j*11+i];
		  
      if (w==Board::color1) 
	drawColor(&p, x,y, 35, redColor );
      else if (w==Board::color1bright) 
	drawColor(&p, x,y, 35, redHColor );
      else if (w==Board::color2) 
	drawColor(&p, x,y, 35, yellowColor );
      else if (w==Board::color2bright)
	drawColor(&p, x,y, 35, yellowHColor );
    }

  if (color1Count >0) {
    /* the outer marks of color1 */
    if (color1Count <12) {
      for(i=11; i>8 && i>color1Count ;i--)
	drawColor(&p, xpos(12-i,7-i)+55, ypos(7-i), 35, redColor );
    }
    for(i=14; i>11 && i>color1Count ;i--)
      drawColor(&p, xpos(-6,10-i)+55, ypos(10-i), 35, redColor );
    
    /* the outer marks of color2 */
    if (color2Count <12) {
      for(i=11; i>8 && i>color2Count ;i--)
	drawColor(&p, xpos(i-12,i-7)-55, ypos(i-7), 35, yellowColor);
    }
    for(i=14; i>11 && i>color2Count ;i--)
      drawColor(&p, xpos(6,i-10)-55, ypos(i-10), 35, yellowColor);
  }
  p.end();
  bitBlt(this, 0, 0, &pm);
}

/** updatePosition
 *
 * Update my position with that of the <board> member.
 * If <updateGUI> is true, draw widget
 */
void BoardWidget::updatePosition(bool updateGUI)
{
  for(int i=0; i<Board::AllFields;i++)
    field[i] = board[i];
  color1Count = board.getColor1Count();
  color2Count = board.getColor2Count();
  color       = board.actColor();
  boardOK     = true;

  if (updateGUI) draw();
}


bool BoardWidget::setEditMode(bool mode)
{
  if (editMode == false && mode==true) {
    editMode = true;
  }
  else if (editMode == true && mode == false) {
    editMode = false;

    for(int i=0; i<Board::AllFields;i++)
      board.setField( i, field[i]);
    board.setColor1Count(color1Count);
    board.setColor2Count(color2Count);
    
  }
  return editMode;
}    


void BoardWidget::clearPosition()
{
  for(int i=0; i<Board::AllFields;i++)
    field[i] = 0;
  color1Count = color2Count = 0;
}

void BoardWidget::showMove(const Move& mm, int step, bool updateGUI)
{
  int f, dir, dir2;
  int opponentNew, colorNew;
  bool afterMove;
  static Move lastMove;
  Move m;
  
  if (boardOK) {
    /* board ok means: board has the normal state 
     *  (e.g. no highlighting) 
     */
    if (step == 0)
      return;      /* nothing to be done */
  }
  boardOK = (step == 0) ? true:false;
  
  if (step == 0) 
    m = lastMove;
  else {
    m = lastMove = mm;
  }
  
  if (color == Board::color1) {
    colorNew = (step<2) ? Board::color1 : 
      (step>2) ? Board::color1bright:Board::free;
    opponentNew = (step<2) ? Board::color2 : Board::color2bright;
  }
  else {
    colorNew = (step<2) ? Board::color2 :
      (step>2) ? Board::color2bright:Board::free;
    opponentNew = (step<2) ? Board::color1 : Board::color1bright;
  }
  
  afterMove = (step == 1) || (step == 4);
  
  f = m.field;
  dir = Board::fieldDiffOfDir(m.direction);
  
  /* first field */
  field[f] = afterMove ? Board::free : colorNew;
  
  switch(m.type) {
  case Move::out2:        /* (c c c o o |) */
    field[f + dir] = colorNew;
    field[f + 2*dir] = colorNew;
    field[f + 3*dir] = afterMove ? colorNew : opponentNew;
    field[f + 4*dir] = opponentNew;
    break;		
  case Move::out1with3:   /* (c c c o |)   */
    field[f + dir] = colorNew;
    field[f + 2*dir] = colorNew;
    field[f + 3*dir] = afterMove ? colorNew : opponentNew;
    break;
  case Move::move3:       /* (c c c .)     */
    field[f + dir] = colorNew;
    field[f + 2*dir] = colorNew;
    field[f + 3*dir] = afterMove ? colorNew : Board::free;
    break;
  case Move::out1with2:   /* (c c o |)     */
    field[f + dir] = colorNew;
    field[f + 2*dir] = afterMove ? colorNew : opponentNew;
    break;
  case Move::move2:       /* (c c .)       */
    field[f + dir] = colorNew;
    field[f + 2*dir] = afterMove ? colorNew : Board::free;
    break;
  case Move::push2:       /* (c c c o o .) */
    field[f + dir] = colorNew;
    field[f + 2*dir] = colorNew;
    field[f + 3*dir] = afterMove ? colorNew : opponentNew;
    field[f + 4*dir] = opponentNew;
    field[f + 5*dir] = afterMove ? opponentNew : Board::free;
    break;
  case Move::left3:		
    dir2 = Board::fieldDiffOfDir(m.direction-1);
    field[f+dir2] = afterMove ? colorNew : Board::free;
    field[f+=dir] = afterMove ? Board::free : colorNew;
    field[f+dir2] = afterMove ? colorNew : Board::free;
    field[f+=dir] = afterMove ? Board::free : colorNew;
    field[f+dir2] = afterMove ? colorNew : Board::free;
    break;
  case Move::right3:
    dir2 = Board::fieldDiffOfDir(m.direction+1);
    field[f+dir2] = afterMove ? colorNew : Board::free;
    field[f+=dir] = afterMove ? Board::free : colorNew;
    field[f+dir2] = afterMove ? colorNew : Board::free;
    field[f+=dir] = afterMove ? Board::free : colorNew;
    field[f+dir2] = afterMove ? colorNew : Board::free;
    break;
  case Move::push1with3:   /* (c c c o .) => (. c c c o) */
    field[f + dir] = colorNew;
    field[f + 2*dir] = colorNew;
    field[f + 3*dir] = afterMove ? colorNew : opponentNew;
    field[f + 4*dir] = afterMove ? opponentNew : Board::free;
    break;
  case Move::push1with2:   /* (c c o .) => (. c c o) */
    field[f + dir] = colorNew;
    field[f + 2*dir] = afterMove ? colorNew : opponentNew;
    field[f + 3*dir] = afterMove ? opponentNew : Board::free;
    break;
  case Move::left2:
    dir2 = Board::fieldDiffOfDir(m.direction-1);
    field[f+dir2] = afterMove ? colorNew : Board::free;
    field[f+=dir] = afterMove ? Board::free : colorNew;
    field[f+dir2] = afterMove ? colorNew : Board::free;
    break;
  case Move::right2:
    dir2 = Board::fieldDiffOfDir(m.direction+1);
    field[f+dir2] = afterMove ? colorNew : Board::free;
    field[f+=dir] = afterMove ? Board::free : colorNew;
    field[f+dir2] = afterMove ? colorNew : Board::free;
    break;
  case Move::move1:       /* (c .) => (. c) */
    field[f + dir] = afterMove ? colorNew : Board::free;
    break;
  default:
    break;
  }
  
  if (updateGUI)
    draw();
}

void BoardWidget::showStart(const Move& m, int step, bool updateGUI)
{
  int f, dir;
  int colorNew;
  
  if (boardOK) {
    /* board ok means: board has the normal state before move */
    if (step == 0)
      return;      /* nothing to be done */
  }
  boardOK = (step == 0) ? true:false;
  
  if (color == Board::color1)
    colorNew = (step==0) ? Board::color1 : Board::color1bright;
  else 
    colorNew = (step==0) ? Board::color2 : Board::color2bright;
  
  f = m.field;
  
  /* first field */
  field[f] = colorNew;
  
  switch(m.type) {
  case Move::left3:
  case Move::right3:
    dir = Board::fieldDiffOfDir(m.direction);
    field[f + dir] = colorNew;
    field[f + 2*dir] = colorNew;
    break;
  case Move::left2:
  case Move::right2:
    dir = Board::fieldDiffOfDir(m.direction);
    field[f + dir] = colorNew;
  default:
    break;
  }
  
  if (updateGUI)
    draw();
}


void BoardWidget::choseMove(MoveList *pl)
{ 
  if (!gettingMove && pl != 0) {
    pList = pl;
    gettingMove = true;
    mbDown = false;
    actValue = - board.calcEvaluation();
    kdDebug(12011) << "Chose Move..." << endl;
  }
}


/* returns position of point (xx,yy)
 */
int BoardWidget::positionOf(int xx, int yy)
{
  int boardSize = QMIN( width()*10/12, height() );
  int x = (1000 * (xx- (width()-boardSize)/2)) / boardSize;
  int y = (1000 * (yy- (height()-boardSize)/2)) / boardSize;
  
  /*
  kdDebug(12011) << "(" << xx << "," << yy << ") -> ("
	    << x << "," << y << ")" << endl;
  */

  y = (y-2)/47;
  if (y < 2 || y > 18) return 0;
  x= ((x+25)/25+ (y-10) )/2;
  if (y <  10 && ((x < 2) || (x > 8+y) )) return 0;
  if (y >= 10 && ((x < y-8) || (x > 18) )) return 0;

  return 22*y + x;
}


bool isBetweenFields(int pos)
{
  bool res = ( ((pos%2) == 1) || ( ((pos/22)%2) == 1) );
  //  kdDebug(12011) << "Pos " << pos << " is between fields: " << res << endl;
  return res;
}

int fieldOf(int pos)
{
  int f = 11*(pos/44) + (pos%22)/2;
  //  kdDebug(12011) << "Field1 of pos " << pos << " is " << f << endl;
  return f;
}

int field2Of(int pos)
{
  int y = pos/22, x = pos%22;
  int f2 = 0, f1 = 11*(y/2) + (x/2);

  if ( (y%2) == 0) {
    /* exact on row */
    if ( (x%2) != 0) {
      /* horizontial between fields */
      f2 = f1+1;
    }
  }
  else {
    /* vertical between fields */
    f2 = f1 + ( ((x%2)==0) ? 11:12 );
  }

  //  kdDebug(12011) << "Field2 of pos " << pos << " is " << f2 << endl;
  return f2;
}


/* get direction depending on difference of positions */
int directionOfPosDiff(int d)
{
  if (d>0) {
    return ((d<21) ? Move::Right : 
	    ((d%22) == 0) ? Move::LeftDown :
	    ((d%23) == 0) ? Move::RightDown : 0);
  }
  else if (d<0) {
    return ((d>-21) ? Move::Left : 
	    ((d%23) == 0) ? Move::LeftUp :
	    ((d%22) == 0) ? Move::RightUp : 0);
  }
  return 0;
}

int directionOfFieldDiff(int d)
{
  if (d>0) {
    return ((d<10) ? Move::Right : 
	    ((d%11) == 0) ? Move::LeftDown :
	    ((d%12) == 0) ? Move::RightDown : 0);
  }
  else if (d<0) {
    return ((d>-10) ? Move::Left : 
	    ((d%12) == 0) ? Move::LeftUp :
	    ((d%11) == 0) ? Move::RightUp : 0);
  }
  return 0;
}

/* Check if <pos> is a valid start position for a allowed move
 * If yes, set 
 *   <startField>, <actMove> and <startType>
 */
bool BoardWidget::isValidStart(int pos, bool midPressed)
{
  bool res = false;
  int f1 = fieldOf(pos);

  startField = f1;

  if (isBetweenFields(pos)) {
    int f2 = field2Of(pos);

    actMove = Move(f1, directionOfFieldDiff( f2-f1 ), Move::none);
    res = pList->isElement(actMove, MoveList::start2);
    if (!res) {
      startField = f2;
      actMove = Move(f2, directionOfFieldDiff( f1-f2 ), Move::none);
      res = pList->isElement(actMove, MoveList::start2);
    }
    startType = MoveList::start2;
    return res;
  }

  if (midPressed) {
    startType = MoveList::start3;

    /* Check all 6 directions */
    for(int dir=1;dir<7;dir++) {
      actMove = Move(f1 - Board::fieldDiffOfDir(dir), dir, Move::none );
      if (pList->isElement(actMove, startType))
	return true;
    }
    /* if we don't find a side move3 fall trough to normal moves... */
  }

  startType = MoveList::start1;
  actMove = Move(f1, 0, Move::none);
  res = pList->isElement(actMove, startType);

  return res;
}


/* Check if <pos> is a valid end position for a move
 * regarding <startPos>
 * If yes, set <actMove> 
 */
bool BoardWidget::isValidEnd(int pos)
{
  int dir = directionOfPosDiff(pos - startPos);
  Move m;

  if (dir == 0) return false;

  switch(startType) {
  case MoveList::start1:
    m = Move(startField, dir, Move::none);
    if (!pList->isElement(m, startType))
      return false;
    break;

  case MoveList::start2:
    {
      int f1 = fieldOf(startPos);
      int f2 = field2Of(startPos);
      int dir2 = directionOfFieldDiff( f2-f1 );
      int dir3 = directionOfFieldDiff( f1-f2 );

      switch((dir2-dir+6)%6) {
      case 1:
	m = Move(f1, dir2, Move::left2);
	break;
      case 2:
	m = Move(f2, dir3, Move::right2);
	break;
      case 4:
	m = Move(f2, dir3, Move::left2);
	break;
      case 5:
	m = Move(f1, dir2, Move::right2);
	break;
      default:
	return false;
      }
      if (!pList->isElement(m, startType))
	return false;

      break;
    }
  case MoveList::start3:
    {
      int rightDir = (dir%6)+1;
      m = Move( startField - Board::fieldDiffOfDir(rightDir), rightDir, Move::left3 );
      if (!pList->isElement(m, startType)) {
	int leftDir = ((dir-2)%6)+1;
	m = Move( startField - Board::fieldDiffOfDir(leftDir), leftDir, Move::right3 );
	if (!pList->isElement(m, startType))
	  return false;
      }
    }
    break;
  }

  actMove = m;
  shownDirection = dir;
  return true;
}



void BoardWidget::mousePressEvent( QMouseEvent* pEvent )
{
  int pos = positionOf( pEvent->x(), pEvent->y() );
  int f = fieldOf(pos);

  if (pEvent->button() == RightButton) {
    emit rightButtonPressed(f, pEvent->globalPos());
    return;
  }

  if (!gettingMove && !editMode) {
    return;
  }
  mbDown = true;


  if (editMode) {
    editColor = (pEvent->button() == MidButton) ? 
      Board::color2 : Board::color1;
    int newColor = (pEvent->button() == MidButton) ? 
      Board::color2bright : Board::color1bright;

    if (field[f] == Board::free) {
      field[f] = newColor;
    }
    else if (field[f] == Board::color1) {
      if (editColor == Board::color1) {
       editColor = Board::free;
       newColor = Board::color1bright;
      }
      field[f] = newColor;
    }
    else if (field[f] == Board::color2) {
      if (editColor == Board::color2) {
       editColor = Board::free;
       newColor = Board::color2bright;
      }
      field[f] = newColor;
    }
    else {
      editColor = Board::out;
    }

    oldPos = pos;
    draw();
    return;
  }

  startValid = isValidStart(pos, (pEvent->button() == MidButton));
  kdDebug(12011) << "Start pos " << pos << " is valid: " << startValid << endl;
  //  actMove.print();

  if (!startValid) return;
  startPos = oldPos = pos;

  showStart(actMove,1);
  startShown = true;

  QString tmp;
  actValue = - board.calcEvaluation();
  tmp = i18n("Board value: %1").arg(actValue);
  emit updateSpy(tmp);
}


void BoardWidget::mouseMoveEvent( QMouseEvent* pEvent )
{
  if ((!gettingMove && !editMode) || !mbDown) return;

  int pos = positionOf( pEvent->x(), pEvent->y() );
  if (pos == oldPos) return;
  oldPos = pos;

  if (editMode) {
    int f = fieldOf(pos);
    if (field[f] != Board::out && field[f] != editColor) {
      int newColor = (editColor == Board::color1) ? Board::color1bright :
       (editColor == Board::color2) ? Board::color2bright :
       (field[f] == Board::color1) ? Board::color1bright :
       (field[f] == Board::color2) ? Board::color2bright : field[f];
      field[f] = newColor;
      draw();
    }
    return;
  }

  if (!startValid) {
    /* We haven't a valid move yet. Check if we are over a valid start */

    startValid = isValidStart(pos, (pEvent->button() == MidButton));
    kdDebug(12011) << "Start pos " << pos << " is valid: " << startValid << endl;
    //    actMove.print();

    if (!startValid) return;
    startPos = oldPos = pos;

    showStart(actMove,1);
    startShown = true;

    QString tmp;
    actValue = - board.calcEvaluation();
    tmp = i18n("Board value: %1").arg(actValue);
    emit updateSpy(tmp);
    return;
  }

  /* restore board */
  updatePosition();
  startShown = false;

  if (isValidEnd(pos)) {
    //    actMove.print();

    board.playMove(actMove);
    int v = board.calcEvaluation();
    board.takeBack();

    QString tmp;
    tmp.sprintf("%+d", v-actValue);
    QString str = QString("%1 : %2").arg(actMove.name()).arg(tmp);
    emit updateSpy(str);
    
    showMove(actMove,3);
    setCursor(*arrow[shownDirection]);
  }
  else {
    QString tmp;

    setCursor(crossCursor);
    if (pos == startPos) {
	showStart(actMove,1);
	startShown = true;
	tmp = i18n("Board value: %1").arg(actValue);
    }
    else
      draw();
    emit updateSpy(tmp);
  }
}


void BoardWidget::mouseReleaseEvent( QMouseEvent* pEvent )
{
  if (!gettingMove && !editMode) return;
  mbDown = false;

  if (editMode) {
    int i;

    //    printf("Releasing...");
    for(i=0; i<Board::AllFields;i++)
      if (field[i] == Board::color1bright ||
         field[i] == Board::color2bright) {
       //printf(" Found %d\n",i);
       field[i] = editColor;
      }

    for(i=0; i<Board::AllFields;i++)
      board.setField( i, field[i]);

    int vState = board.validState(); // set color1/2Count
    color1Count = board.getColor1Count();
    color2Count = board.getColor2Count();

    draw();

    emit edited(vState);
    return;
  }

  if (!startValid) return;

  int pos = positionOf( pEvent->x(), pEvent->y() );
  if (isValidEnd(pos)) {
    //    actMove.print();
    startValid = false;
    setCursor(crossCursor);
    gettingMove = false;
    emit moveChoosen(actMove);
    return;
  }

  updatePosition(true);
  startValid = false;
  setCursor(crossCursor);

  QString tmp;
  emit updateSpy(tmp);
}

QSize  BoardWidget::sizeHint() const
{
   return QSize(400, 350);
}

#include "BoardWidget.moc"
