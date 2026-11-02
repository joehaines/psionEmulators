#ifndef BASE_BOARD_H
#define BASE_BOARD_H

#include <qtimer.h>
#include <qcanvas.h>

#include "gtetris.h"


class SequenceArray;
class BlockInfo;

//-----------------------------------------------------------------------------
class FixedCanvasView : public QCanvasView
{
  Q_OBJECT
public:
  FixedCanvasView(QWidget *parent = 0, const char *name = 0);
  
  virtual QSize sizeHint() const;
  
public slots:
  virtual void adjustSize();
};

//-----------------------------------------------------------------------------
class BaseBoard : public FixedCanvasView, public GenericTetris
{
 Q_OBJECT
 public:
    enum Direction { Left = 1, Right = 2, Up = 4, Down = 8, Nb_Direction = 4 };
 private:
    struct DirectionData {
        KGrid2D::SquareBase::Neighbour neighbour;
        Direction direction;
    };
    static const DirectionData DIRECTION_DATA[Nb_Direction];

 public:
    BaseBoard(bool graphic, QWidget *parent);
    virtual ~BaseBoard();
    void copy(const GenericTetris &);

    void init(bool arcade);
    virtual void start(const GTInitData &);
    virtual void pause();
    virtual void unpause();
    virtual void stop();
    bool isGameOver() const { return state==GameOver; }
    bool isPaused() const { return state==Paused; }

    bool isArcade() const { return _arcade; }
    uint arcadeStage() const { return _arcadeStage; }
    bool arcadeStageDone() const { return _arcadeStageDone; }
    virtual uint arcadeTodo() const { return 0; }
    virtual uint arcadeDone() const { return 0; }
    
    virtual void settingsChanged();
    BlockInfo *next() const { return _next; }

    int firstColumnBlock(uint column) const;
    
 public slots:
    virtual void adjustSize();

 protected slots:
    virtual bool timeout(); // return true if treated

 signals:
    void updatePieceConfigSignal();
    void removedUpdated();
    void scoreUpdated();
    void gameOverSignal();

 protected:
    virtual bool beforeRemove(bool first);
    void _beforeRemove(bool first);
    enum AfterRemoveResult { Done, NeedAfterRemove, NeedRemoving };
    virtual AfterRemoveResult afterRemove(bool doAll, bool first);
    void _afterAfterRemove();
    virtual bool afterAfterRemove() = 0;
    virtual bool startTimer(); // return true if treated
    virtual bool toBeRemoved(const KGrid2D::Coord &) const = 0;
    virtual void remove();
    virtual bool toFall(const KGrid2D::Coord &) const = 0;//height>0 when called
    virtual bool doFall(bool doAll, bool first, bool lineByLine);
    virtual void gameOver();
    virtual void arcadePrepare() {}

    uint drawCode(const KGrid2D::Coord &) const;
    void computeNeighbours();
    void partialBlockFall(const KGrid2D::Coord &src, const KGrid2D::Coord &dest);

    // return the sizes of the groups (>=minSize)
    QMemArray<uint> findGroups(KGrid2D::Square<int> &field, uint minSize,
							   bool exitAtFirstFound = false) const;
    // find group size and put -1 in the corresponding blocks (these blocks
    // should be 0 at start)
    uint findGroup(KGrid2D::Square<int> &field, const KGrid2D::Coord &) const;
    // set the size of the group in the blocks (these blocks should be -1
    // at start ie you should have called findGroup() before)
    void setGroup(KGrid2D::Square<int> &field, const KGrid2D::Coord &c,
                  uint nb) const;

    void updateRemoved(uint newRemoved);
    void updateScore(uint newScore);

    virtual void showBoard(bool show);
    void showCanvas(QCanvas *c, bool show);

    enum BoardState { GameOver, Normal, Paused,
                      DropDown, BeforeGlue, AfterGlue, BeforeRemove,
                      AfterRemove, AfterGift };
    BoardState     state, _oldState;
    QTimer         timer;
    SequenceArray *sequences;
    BlockInfo     *main, *_next;
    uint           loop;

 private:
    bool _arcade, _arcadeStageDone;
    uint _arcadeStage;

    void _afterRemove(bool first);
    void updatePieceConfig() { emit updatePieceConfigSignal(); }

    void _findGroup(KGrid2D::Square<int> &field, const KGrid2D::Coord &c,
                    uint &nb, bool set) const;
    void blockInGroup(KGrid2D::Square<int> &field, const KGrid2D::Coord &c,
                      uint value, uint &nb, bool ser) const;
};

#endif
