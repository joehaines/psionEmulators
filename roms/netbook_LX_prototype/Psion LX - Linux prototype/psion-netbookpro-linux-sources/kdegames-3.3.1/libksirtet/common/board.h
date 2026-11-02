#ifndef COMMON_BOARD_H
#define COMMON_BOARD_H

#include "base/board.h"


class GiftPool;
class AI;

class Board : public BaseBoard
{
 Q_OBJECT
 public:
    Board(bool graphic, GiftPool *, QWidget *parent);
    virtual ~Board();

    void setType(bool computer);
    virtual void start(const GTInitData &);
    void unpause();
    void stop();

    virtual uint gift() = 0;
    virtual bool needRemoving() = 0;

    GiftPool *giftPool() const { return _giftPool; }

 public slots:
    void pMoveLeft();
    void pMoveRight();
    void pDropDownStart();
    void pDropDownStop();
    void pOneLineDown();
    void pRotateLeft();
    void pRotateRight();
    void pMoveLeftTotal();
    void pMoveRightTotal();

 private slots:
    bool timeout();

 signals:
    void levelUpdated();

 protected:
    KRandomSequence randomGarbage;

    virtual bool beforeGlue(bool bump, bool first);
    virtual void gluePiece();
    virtual bool afterGlue(bool /*doAll*/, bool /*first*/) { return false; }
    virtual bool afterGift(bool /*first*/) { return false; }
    virtual bool putGift(uint) = 0;

    virtual uint normalTime() const;
    void updateLevel();

    void settingsChanged();

 private:
    uint      _dropHeight;
    GiftPool *_giftPool;
    AI       *aiEngine;

    bool newPiece();
    void pieceDropped(uint dropHeight);
    void _afterGift(bool first);
    void _beforeGlue(bool first);
    void _afterGlue(bool first);
    bool afterAfterRemove();
    bool startTimer();
    void showBoard(bool show);
    void updateLevel(uint newLevel);
};

#endif
