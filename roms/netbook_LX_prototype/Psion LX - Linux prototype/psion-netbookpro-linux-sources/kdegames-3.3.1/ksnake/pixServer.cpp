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

#include "pixServer.h"

#include <qimage.h>
#include <qpainter.h>
#include <qbitmap.h>

#include <kstandarddirs.h>
#include <klocale.h>
#include <kdebug.h>

#include "board.h"
#include "settings.h"

PixServer::PixServer( Board *b)
{
    board = b;
    initPixmaps();
    initBrickPixmap();
    initbackPixmaps();
    initRoomPixmap();
}

void PixServer::erase(int pos)
{
    if (!board->isEmpty(pos))
	return;

    QRect rect = board->rect(pos);
    bitBlt( &cachePix, rect.x(), rect.y(), &backPix,
	    rect.x(), rect.y(), rect.width(), rect.height());
}

void PixServer::restore(int pos)
{
    QRect rect = board->rect(pos);
    bitBlt( &cachePix, rect.x(), rect.y(), &roomPix,
			rect.x(), rect.y(), rect.width(), rect.height());
}

void PixServer::draw(int pos, PixMap pix, int i)
{
    QPixmap p;
    p.resize(BRICKSIZE, BRICKSIZE);

    QRect rect = board->rect(pos);

    if (! plainColor)
	bitBlt( &p, 0, 0, &backPix,
		rect.x(), rect.y(), rect.width(), rect.height());
    else
	p.fill(backgroundColor);

    switch (pix) {
    case SamyPix:        bitBlt(&p ,0,0, &samyPix[i]);
	break;
    case CompuSnakePix:  bitBlt(&p ,0,0, &compuSnakePix[i]);
	break;
    case ApplePix:       bitBlt(&p ,0,0, &applePix[i]);
	break;
    case BallPix:        bitBlt(&p ,0,0, &ballPix[i]);
	break;
    default:
	break;
    }

    bitBlt(&cachePix, rect.x(), rect.y(), &p);
}

void PixServer::initPixmaps()
{

    QPixmap pm = QPixmap(locate("appdata", "pics/snake1.png"));
    QImage qi = pm.convertToImage();
    qi=qi.smoothScale(BRICKSIZE*18,BRICKSIZE);
    pm.convertFromImage(qi,QPixmap::AvoidDither);
    for (int x = 0 ; x < 18; x++){
		compuSnakePix[x].resize(BRICKSIZE, BRICKSIZE);
		bitBlt(&compuSnakePix[x] ,0,0, &pm,x*BRICKSIZE, 0, BRICKSIZE, BRICKSIZE, Qt::CopyROP, TRUE);
		compuSnakePix[x].setMask(compuSnakePix[x].createHeuristicMask());
    }

    pm = QPixmap(locate("appdata", "pics/snake2.png"));
    qi = pm.convertToImage();
    qi=qi.smoothScale(BRICKSIZE*18,BRICKSIZE);
    pm.convertFromImage(qi,QPixmap::AvoidDither);
    for (int x = 0 ; x < 18; x++){
		samyPix[x].resize(BRICKSIZE, BRICKSIZE);
		bitBlt(&samyPix[x] ,0,0, &pm,x*BRICKSIZE, 0, BRICKSIZE, BRICKSIZE, Qt::CopyROP, TRUE);
		samyPix[x].setMask(samyPix[x].createHeuristicMask());
    }

    pm = QPixmap(locate("appdata", "pics/ball.png"));
    qi = pm.convertToImage();
    qi=qi.smoothScale(BRICKSIZE*4,BRICKSIZE);
    pm.convertFromImage(qi,QPixmap::AvoidDither);
    for (int x = 0 ; x < 4; x++){
	ballPix[x].resize(BRICKSIZE, BRICKSIZE);
	bitBlt(&ballPix[x] ,0,0, &pm,x*BRICKSIZE, 0, BRICKSIZE, BRICKSIZE, Qt::CopyROP, TRUE);
	ballPix[x].setMask(ballPix[x].createHeuristicMask());
    }

    pm = QPixmap(locate("appdata", "pics/apples.png"));
    qi = pm.convertToImage();
    qi=qi.smoothScale(BRICKSIZE*2,BRICKSIZE);
    pm.convertFromImage(qi,QPixmap::AvoidDither);
    for (int x = 0 ; x < 2; x++){
	applePix[x].resize(BRICKSIZE, BRICKSIZE);
	bitBlt(&applePix[x] ,0,0, &pm,x*BRICKSIZE, 0, BRICKSIZE, BRICKSIZE, Qt::CopyROP, TRUE);
	applePix[x].setMask(applePix[x].createHeuristicMask());
    }
}

void PixServer::initbackPixmaps()
{
	QString path;
	plainColor = FALSE;

	if(Settings::bgcolor_enabled()){
	    backgroundColor = Settings::bgcolor();
	    plainColor = TRUE;
	} else if(Settings::bgimage_enabled()) {
		// A bit of a hack.
		QStringList backgroundPixmaps =
		KGlobal::dirs()->findAllResources("appdata", "backgrounds/*.png");
	path = backgroundPixmaps[(Settings::bgimage())];
	}

    QPixmap PIXMAP;
    int pw, ph;

    backPix.resize(MAPWIDTH, MAPHEIGHT);

    if (! plainColor) {

	PIXMAP = QPixmap(path);

	if (!PIXMAP.isNull()) {
	    pw = PIXMAP.width();
	    ph = PIXMAP.height();

	    for (int x = 0; x <= MAPWIDTH; x+=pw)     //Tile BG Pixmap onto backPix
		for (int y = 0; y <= MAPHEIGHT; y+=ph)
		    bitBlt(&backPix, x, y, &PIXMAP);
	}
	else  {
	    kdDebug() << "error loading background image :" << path << endl;
	    backgroundColor = (QColor("black"));
	    plainColor = TRUE;
	}
    }
    if ( plainColor)
	backPix.fill(backgroundColor);
}

void PixServer::initBrickPixmap()
{
    QPixmap pm = QPixmap(locate("appdata", "pics/brick.png"));
    if (pm.isNull()) {
	kdFatal() << i18n("error loading %1, aborting\n").arg("brick.png");
    }
    int pw = pm.width();
    int ph = pm.height();

    offPix.resize(MAPWIDTH, MAPHEIGHT);
    for (int x = 0; x <= MAPWIDTH; x+=pw)
	for (int y = 0; y <= MAPHEIGHT; y+=ph)
	    bitBlt(&offPix, x, y, &pm);
}

void PixServer::initRoomPixmap()
{
    QPainter paint;

    roomPix.resize(MAPWIDTH, MAPHEIGHT);
    bitBlt(&roomPix,0,0, &backPix);
    paint.begin(&roomPix);

    for (unsigned int x = 0; x < board->size(); x++) {
	if (board->isBrick(x))
	    drawBrick(&paint, x);
    }
    paint.end();
    
    cachePix.resize(MAPWIDTH, MAPHEIGHT);
    bitBlt(&cachePix,0,0, &roomPix);
}

void PixServer::drawBrick(QPainter *p ,int i)
{
    //Note, ROOMPIC IS OUR 'TARGET'
    QColor light(180,180,180);
    QColor dark(100,100,100);

    int topSq   = board->getNext(N, i);  //find 'address' of neighbouring squares
    int botSq   = board->getNext(S, i);
    int rightSq = board->getNext(E ,i);
    int leftSq  = board->getNext(W, i);

    QRect rect = board->rect(i); //get our square's rect

    int x = rect.x();
    int y = rect.y();      //Get x,y location of square???

    int width, height;

    int highlight = 2;    //Highlighting Distance (pixels)?

    width = height = rect.width();

    p->fillRect(x, y, width, height, light); //By default, fill square w/ Light? no.  use dark!!!!

    bitBlt(&roomPix, x, y, &offPix, x, y, width, height ); //Copy the brick pattern onto the brick

    if (!board->isBrick(rightSq)) p->fillRect(x + width - highlight, y, highlight, height, dark);  //highlight if its an end-brick.
    if (!board->isBrick(leftSq)) p->fillRect(x, y, highlight, height, light);
    if (!board->isBrick(botSq)) p->fillRect(x, y + height - highlight, width, highlight, dark);
    if (!board->isBrick(topSq)) p->fillRect(x, y, width, highlight, light);

}

