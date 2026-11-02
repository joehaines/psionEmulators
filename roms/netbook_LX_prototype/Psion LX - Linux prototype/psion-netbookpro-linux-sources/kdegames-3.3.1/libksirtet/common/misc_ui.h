#ifndef COMMON_MISC_UI_H
#define COMMON_MISC_UI_H

#include <qcanvas.h>
#include <qptrvector.h>
#include <qhbox.h>

#include <kgameprogress.h>

#include "base/board.h"

/*****************************************************************************/
class ShowNextPiece : public FixedCanvasView
{
 Q_OBJECT
 public:
    ShowNextPiece(BaseBoard *, QWidget *parent);
};

/*****************************************************************************/
class Shadow : public QWidget
{
 Q_OBJECT
 public:
    Shadow(BaseBoard *, QWidget *parent);

    virtual QSize sizeHint() const;
    virtual QSizePolicy sizePolicy() const;
    void setDisplay(bool show);

 private:
    int              _xOffset;
    const BaseBoard *_board;
    bool             _show;

    void paintEvent(QPaintEvent *);
};

/*****************************************************************************/
class Led;

class GiftPool : public QHBox
{
 Q_OBJECT
 public:
    GiftPool(QWidget *parent);

    virtual QSize sizeHint() const;
    virtual QSizePolicy sizePolicy() const;

    void reset();
	void put(uint);
	uint take();
	bool pending() const { return ready; }

 private slots:
    void timeout() { ready = true; }

 private:
	QPtrVector<Led> leds;
	uint         _timeout, nb;
	bool         ready;
};


/*****************************************************************************/
class PlayerProgress : public KGameProgress
{
  Q_OBJECT
public:
  PlayerProgress(BaseBoard *board, QWidget *parent = 0, const char *name = 0);
  
  virtual QSize sizeHint() const;
  virtual QSizePolicy sizePolicy() const;
  
private:
  BaseBoard *_board;
};

#endif
