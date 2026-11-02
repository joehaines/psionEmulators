#include "misc_ui.h"
#include "misc_ui.moc"

#include <qpainter.h>

#include "base/piece.h"
#include "base/board.h"
#include "base/baseprefs.h"
#include "base/kzoommainwindow.h"
#include "factory.h"

const uint GIFT_SHOWER_TIMEOUT = 800;
const uint GIFT_POOL_TIMEOUT = 2000;

const uint SHADOW_HEIGHT = 10;

const uint GI_WIDTH = 15;
const uint GI_HEIGHT = 11;
const uint ARROW_HEIGHT = 3;
const uint ARROW_WIDTH = 7;

const uint LED_WIDTH = 15;
const uint LED_HEIGHT = 15;
const uint LED_SPACING = 5;

/*****************************************************************************/
ShowNextPiece::ShowNextPiece(BaseBoard *board, QWidget *parent)
    : FixedCanvasView(parent, "show_next_piece")
{
    setCanvas(board->next());
    setFrameStyle(QFrame::Panel | QFrame::Sunken);
    KZoomMainWindow::addWidget(this);
}

/*****************************************************************************/
Shadow::Shadow(BaseBoard *board, QWidget *parent)
    : QWidget(parent, "shadow"), _xOffset(board->frameWidth()),
     _board(board), _show(false)
{
    KZoomMainWindow::addWidget(this);
    connect(board, SIGNAL(updatePieceConfigSignal()), SLOT(update()));
}

QSize Shadow::sizeHint() const
{
	return QSize(_xOffset + _board->matrix().width() * BasePrefs::blockSize(),
				 SHADOW_HEIGHT);
}

QSizePolicy Shadow::sizePolicy() const
{
	return QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void Shadow::setDisplay(bool show)
{
    _show = show;
    update();
}

void Shadow::paintEvent(QPaintEvent *)
{
	if ( !_show ) return;

	const Piece *piece = _board->currentPiece();
	uint pf = piece->min().first + _board->currentPos().first;
	uint pl = pf + piece->size().first - 1;

    QPainter p(this);
    p.setBrush(black);
    p.setPen(black);
	for (uint i=pf; i<=pl; i++) {
		QRect r(_xOffset + i * BasePrefs::blockSize() + 1 , 0,
				BasePrefs::blockSize() - 2, SHADOW_HEIGHT);
		p.drawRect(r);
	}
}


/*****************************************************************************/
class Led : public QWidget
{
 public:
	Led(const QColor &c, QWidget *parent)
		: QWidget(parent), col(c), _on(FALSE) {}

	QSizePolicy sizePolicy() const
		{ return QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed); }
	QSize sizeHint() const { return QSize(LED_WIDTH, LED_HEIGHT); }

	void on()  { if (!_on) { _on = TRUE; repaint(); } }
	void off() { if (_on)  {_on = FALSE; repaint(); } }
	void setColor(const QColor &c) { if (c!=col) { col = c; repaint(); } }

 protected:
	void paintEvent(QPaintEvent *) {
		QPainter p(this);
		p.setBrush((_on ? col.light() : col.dark()));
		p.setPen(black);
		p.drawEllipse(0, 0, width(), height());
	}

 private:
	QColor col;
	bool   _on;
};

GiftPool::GiftPool(QWidget *parent)
    : QHBox(parent, "gift_pool"), nb(0), ready(false)
{
	setSpacing(LED_SPACING);
	leds.resize(cfactory->cbi.nbGiftLeds);
	for (uint i=0; i<leds.size(); i++)
        leds.insert(i, new Led(yellow, this));
}

QSize GiftPool::sizeHint() const
{
	QSize s = (leds.size() ? leds[0]->sizeHint() : QSize());
	return QSize((s.width()+LED_SPACING)*leds.size()-LED_SPACING, s.height());
}

QSizePolicy GiftPool::sizePolicy() const
{
	return QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void GiftPool::put(uint n)
{
	if ( n==0 ) return;
	if ( nb==0 && !ready )
        QTimer::singleShot(cfactory->cbi.giftPoolTimeout,
                           this, SLOT(timeout()));
	uint e = QMIN(nb+n, leds.size());
	for (uint i=nb; i<e; i++) leds[i]->on();
	uint f = QMIN(nb+n-e, leds.size());
	for (uint i=0; i<f; i++) leds[i]->setColor(red);
	nb += n;
}

uint GiftPool::take()
{
	Q_ASSERT(ready);
	for (uint i=0; i<leds.size(); i++) {
		leds[i]->setColor(yellow);
		leds[i]->off();
	}
    uint max = cfactory->cbi.maxGiftsToSend;
	if ( nb>max ) {
		uint p = nb - max;
		nb = 0;
		put(p);
		return max;
	} else {
		ready = false;
		uint t = nb;
		nb = 0;
		return t;
	}
}

void GiftPool::reset()
{
    killTimers();
    ready = false;
    nb = 0;
    for (uint i=0; i<leds.size(); i++) {
		leds[i]->setColor(yellow);
		leds[i]->off();
	}
}

//-----------------------------------------------------------------------------
PlayerProgress::PlayerProgress(BaseBoard *board, QWidget *parent,
	              	       const char *name)
  : KGameProgress(0, board->matrix().height(), 0, KGameProgress::Vertical,
                  parent, name), _board(board)
{
  setBackgroundColor(lightGray);
  setTextEnabled(false);
  setBarColor(blue);
  KZoomMainWindow::addWidget(this);
}

QSize PlayerProgress::sizeHint() const
{
  return QSize(10, _board->matrix().height() * BasePrefs::blockSize())
    + 2 * QSize(frameWidth(), frameWidth());
}

QSizePolicy PlayerProgress::sizePolicy() const
{
  return QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}
