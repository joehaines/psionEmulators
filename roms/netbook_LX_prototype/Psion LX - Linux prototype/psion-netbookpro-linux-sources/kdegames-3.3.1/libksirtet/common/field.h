#ifndef COMMON_FIELD_H
#define COMMON_FIELD_H

#include "lib/mp_simple_board.h"
#include "base/field.h"
#include "types.h"


class ShowNextPiece;
class GiftShower;
class Shadow;
class KProgress;
class KGameProgress;
class KGameLCDClock;

class Field : public MPSimpleBoard, public BaseField
{
 Q_OBJECT
 public:
    Field(QWidget *parent);

 public slots:
    void moveLeft();
    void moveRight();
    void dropDownStart();
    void dropDownStop();
    void oneLineDown();
    void rotateLeft();
    void rotateRight();
    void moveLeftTotal();
    void moveRightTotal();

    virtual void settingsChanged();

 protected slots:
    void scoreUpdatedSlot() { scoreUpdated(); }
    virtual void levelUpdated();
    virtual void removedUpdated();

 private:
    KGameLCDClock *showTime;
    ShowNextPiece *showNext;
    KProgress     *toLevel;
    QLabel        *labShowNext, *levelLabel;
    KGameProgress *prevHeight, *nextHeight;
    Shadow        *shadow;
    KCanvasRootPixmap *_snRootPixmap;
    ClientPlayData _cpd;

    void _init(bool AI, bool multiplayer, bool server, bool first,
               const QString &name);
    void showOpponents(bool show);
    void initVariableGUI();

    void _initFlag(QDataStream &);
    void _playFlag(QDataStream &);
    void _pauseFlag(bool pause);
    void _stopFlag(bool gameover);
    void _dataOut(QDataStream &);
    void _gameOverDataOut(QDataStream &);
    void _initDataOut(QDataStream &) {}

    KExtHighscore::Score currentScore() const;
};

#endif
