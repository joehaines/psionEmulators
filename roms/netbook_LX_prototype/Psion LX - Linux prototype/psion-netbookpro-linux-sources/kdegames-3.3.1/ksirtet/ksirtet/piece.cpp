#include "piece.h"

#include <qpainter.h>
#include <klocale.h>


const KSPieceInfo::Form KSPieceInfo::FORMS[NB_FORMS] = {
    { {{-1, -1,  0,  0}, {-1,  0,  0,  1}, {-1, -1,  0,  0}, {-1,  0,  0,  1}},
      {{ 1,  0,  0, -1}, {-1, -1,  0,  0}, { 1,  0,  0, -1}, {-1, -1,  0,  0}},
      {{ 0,  0, -1, -1}, { 1,  0,  0, -1}, { 0,  0,  1,  1}, {-1,  0,  0,  1}},
      {{-1,  0,  0,  1}, { 0,  0, -1, -1}, { 1,  0,  0, -1}, { 0,  0,  1,  1}},
      2 }, // broken line Z
    { {{-1, -1,  0,  0}, {-1,  0,  0,  1}, {-1, -1,  0,  0}, {-1,  0,  0,  1}},
      {{-1,  0,  0,  1}, { 0,  0, -1, -1}, {-1,  0,  0,  1}, { 0,  0, -1, -1}},
      {{ 0,  0,  1,  1}, { 1,  0,  0, -1}, { 0,  0, -1, -1}, {-1,  0,  0,  1}},
      {{-1,  0,  0,  1}, { 0,  0,  1,  1}, { 1,  0,  0, -1}, { 0,  0, -1, -1}},
      2 }, // broken line S
	{ {{ 0,  0,  0,  0}, {-1,  0,  1,  2}, { 0,  0,  0,  0}, {-1,  0,  1,  2}},
      {{-1,  0,  1,  2}, { 0,  0,  0,  0}, {-1,  0,  1,  2}, { 0,  0,  0,  0}},
      {{ 0,  0,  0,  0}, { 1,  0, -1, -2}, { 0,  0,  0,  0}, {-1,  0,  1,  2}},
	  {{-1,  0,  1,  2}, { 0,  0,  0,  0}, { 1,  0, -1, -2}, { 0,  0,  0,  0}},
      2 }, // line
	{ {{ 0,  0,  1,  0}, {-1,  1,  0,  0}, { 0,  0, -1,  0}, { 1, -1,  0,  0}},
      {{ 1, -1,  0,  0}, { 0,  0,  1,  0}, {-1,  1,  0,  0}, { 0,  0, -1,  0}},
      {{-1,  0,  1,  0}, { 0,  0,  0,  1}, { 1,  0, -1,  0}, { 0,  0,  0, -1}},
	  {{ 0,  0,  0, -1}, {-1,  0,  1,  0}, { 0,  0,  0,  1}, { 1,  0, -1,  0}},
      4 }, // T
	{ {{-1,  0, -1,  0}, {-1,  0, -1,  0}, {-1,  0, -1,  0}, {-1,  0, -1,  0}},
	  {{ 0,  0, -1, -1}, { 0,  0, -1, -1}, { 0,  0, -1, -1}, { 0,  0, -1, -1}},
      {{-1,  0, -1,  0}, { 0,  0, -1, -1}, { 1,  0,  1,  0}, { 0,  0,  1,  1}},
	  {{ 0,  0,  1,  1}, {-1,  0, -1,  0}, { 0,  0, -1, -1}, { 1,  0,  1,  0}},
      1 }, // square
	{ {{ 1,  1,  1,  0}, {-1,  0,  1,  1}, {-1, -1, -1,  0}, { 1,  0, -1, -1}},
	  {{ 1,  0, -1, -1}, { 1,  1,  1,  0}, {-1,  0,  1,  1}, {-1, -1, -1,  0}},
      {{-1,  0,  0,  0}, { 1,  1,  0, -1}, { 1,  0,  0,  0}, {-1, -1,  0,  1}},
      {{-1, -1,  0,  1}, {-1,  0,  0,  0}, { 1,  1,  0, -1}, { 1,  0,  0,  0}},
       4 }, // backward L
    { {{-1, -1, -1,  0}, {-1,  0,  1,  1}, { 1,  1,  1,  0}, { 1,  0, -1, -1}},
	  {{ 1,  0, -1, -1}, {-1, -1, -1,  0}, {-1,  0,  1,  1}, { 1,  1,  1,  0}},
      {{ 1,  0,  0,  0}, { 1,  1,  0, -1}, {-1,  0,  0,  0}, {-1, -1,  0,  1}},
	  {{-1, -1,  0,  1}, { 1,  0,  0,  0}, { 1,  1,  0, -1}, {-1,  0,  0,  0}},
       4 }  // L
};

const char *KSPieceInfo::COLOR_LABELS[NB_FORMS+1] = {
    I18N_NOOP("Z piece color:"), I18N_NOOP("S piece color:"),
    I18N_NOOP("I piece color:"), I18N_NOOP("T piece color:"),
    I18N_NOOP("Square color:"), I18N_NOOP("Mirrored L piece color:"),
    I18N_NOOP("L piece color:"), I18N_NOOP("Garbage block color:")
};

const char *KSPieceInfo::DEFAULT_COLORS[NB_FORMS+1] = {
    "#C86464", "#64C864", "#6464C8", "#C8C864", "#C864C8","#64C8C8", "#DAAA00",
    "#C8C8C8"
};

QColor KSPieceInfo::defaultColor(uint i) const
{
  if ( i>=nbColors() ) return QColor();
  return QColor(DEFAULT_COLORS[i]);
}

void KSPieceInfo::draw(QPixmap *pixmap, uint blockType, uint,
                       bool lighted) const
{
    QColor col = color(blockType);
	if (lighted) col = col.light();
	pixmap->fill(col);

	QPainter p(pixmap);
    QRect r = pixmap->rect();

	p.setPen( col.light() );
	p.moveTo(r.bottomLeft());
	p.lineTo(r.topLeft());
	p.lineTo(r.topRight());

	p.setPen( col.dark() );
	p.moveTo(r.topRight() + QPoint(0,1));
	p.lineTo(r.bottomRight());
	p.lineTo(r.bottomLeft() + QPoint(1,0));
}
