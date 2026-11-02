#include "piece.h"

#include <math.h>

#include <qpainter.h>
#include <qbitmap.h>

#include <klocale.h>

#include "base/board.h"


const FEPieceInfo::Form FEPieceInfo::FORM = {
    {{ 0,  0}, {-1,  0}, { 0,  0}, { 0, -1}},
    {{ 0, -1}, {-1, -1}, {-1,  0}, {-1, -1}}
};

const char *FEPieceInfo::DEFAULT_COLORS[NB_NORM_BLOCK_TYPES + 1] = {
    "#64C864", "#64C8C8", "#C86464", "#C864C8", "#C8C8C8"
};

QColor FEPieceInfo::defaultColor(uint i) const
{ 
  if ( i>=nbColors() ) return QColor();
  return QColor(DEFAULT_COLORS[i]);
}

QString FEPieceInfo::colorLabel(uint i) const
{
    return (i==NB_NORM_BLOCK_TYPES ? i18n("Garbage color:")
            : i18n("Color #%1:").arg(i+1));
}

void FEPieceInfo::draw(QPixmap *pixmap, uint blockType, uint,
                       bool lighted) const
{
    QColor col = color(blockType);
    if (lighted) col = col.light();
    pixmap->fill(col);
}

void FEPieceInfo::setMask(QPixmap *pixmap, uint blockMode) const
{
    Q_ASSERT( pixmap->width()==pixmap->height() ); // drawing code assumes that
    QBitmap bitmap(pixmap->size(), true);
    QPainter p(&bitmap);
	p.setBrush(Qt::color1);
	p.setPen( QPen(Qt::NoPen) );

	// base circle
    int w = pixmap->width();
	int d = (int)((sqrt(2)-2./3)*w);
	QRect cr = QRect(0, 0, d, d);
	cr.moveCenter(QPoint(w/2, w/2));
	p.drawEllipse(cr);

	if (blockMode) {
        int a  = (int)(w/(3.*sqrt(2)));
        int ra = 2*w/3+1;
        cr = QRect(0, 0, ra, ra);

        // first drawing with color1
        if ( blockMode & BaseBoard::Up    ) p.drawRect(    0,     0, w, a);
        if ( blockMode & BaseBoard::Right ) p.drawRect(w-a+1,     0, a, w);
        if ( blockMode & BaseBoard::Down  ) p.drawRect(    0, w-a+1, w, a);
        if ( blockMode & BaseBoard::Left  ) p.drawRect(    0,     0, a, w);

        // second drawing with color0
        p.setBrush(Qt::color0);
        if ( (blockMode & BaseBoard::Up) || (blockMode & BaseBoard::Left) ) {
            cr.moveCenter(QPoint(0, 0));
            p.drawEllipse(cr);
        }
        if ( (blockMode & BaseBoard::Right) || (blockMode & BaseBoard::Up) ) {
            cr.moveCenter(QPoint(w-1, 0));
            p.drawEllipse(cr);
        }
        if ( (blockMode & BaseBoard::Down) || (blockMode & BaseBoard::Right) ){
            cr.moveCenter(QPoint(w-1, w-1));
            p.drawEllipse(cr);
        }
        if ( (blockMode & BaseBoard::Left) || (blockMode & BaseBoard::Down) ) {
            cr.moveCenter(QPoint(0, w-1));
            p.drawEllipse(cr);
        }
    }

    p.end();
    pixmap->setMask(bitmap);
}
