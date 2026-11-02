/****************************************************************************
**
** Definition of GenericTetris, a generic class for implementing Tetris.
**
** Author   : Eirik Eng
** Created  : 940126
** Modified by Nicolas Hadacek
**
** Copyright (C) 1994 by Eirik Eng. All rights reserved.
**
**---------------------------------------------------------------------------
**
** The GenericTetris class is an abstract class that can be used to implement
** the well known game of Tetris. It is totally independent of any hardware
** platform or user interface mechanism. It has no notion of time, so you
** have to supply the timer ticks (or heartbeat) of the game. Apart from
** that this is a complete Tetris implementation.
**
** In the following it is assumed that the reader is familiar with the game
** of Tetris.
**
** The class operates on a grid of squares (referred to as the "board" below),
** where each of the different types of pieces in Tetris covers 4 squares. The
** width and height of the board can be specified in the constructor (default
** is 10x22). The coordinate (0,0) is at the TOP LEFT corner. The class
** assumes that it has total control over the board and uses this to optimize
** drawing of the board. If you need to update parts of the board
** (e.g. if you are using a window system), use the updateBoard() function.
**
** An implementation of the game must subclass from TetrisBoard and must
** implement these abstract functions:
**
**     virtual void drawSquare(int x,int y,int value)
**
**        This function is called when a square needs to be drawn
**        on the Tetris board. A value of 0 means that the square
**        should be erased. Values of 1 to 7 indicate the different
**        types of pieces. (Thus a minimal implementation can
**        draw 0 in one way and 1-7 in a second way). The x and y
**        values are coordinates on the Tetris board (see above).
**
**    virtual void gameOver()
**
**        This function is called when it is impossible to put a new
**        piece at the top of the board.
**
** To get a working minimal implementation of Tetris the following functions
** must be called from the user interface:
**
**     void startGame()
**
**         Clears the board and starts a new game.
**
**    void moveLeft()
**    void moveRight()
**    void rotateLeft()
**    void rotateRight()
**
**        The standard Tetris controls for moving and rotating the
**        falling pieces.
**
**    void dropDown();
**
**        Another Tetris control, drops the falling piece and calls the
**        virtual function pieceDropped(), whose default implementation is
**        to create a new block which appears at the top of the board.
**
**    void oneLineDown();
**
**        This is where you supply the timer ticks, or heartbeat, of the
**        game. This function moves the current falling piece one line down
**        on the board, or, if that cannot be done, calls the virtual
**        function pieceDropped() (see dropDown() above). The time between
**        each call to this function directly affects the difficulty of the
**        game. If you want to pause the game, simply block calls to the
**        user control functions above and stop calling this function (you
**        might want to call hideBoard() also).
**
** And that's it! There are several other public functions you can call
** and virtual functions you can overload to modify or extend the game.
**
** Do whatever you want with this code (i.e. the files gtetris.h,
** gtetris.cpp, tpiece.h and tpiece.cpp). It is basically a weekend hack
** and it would bring joy to my heart if anyone in any way would find
** it useful.
**
** Nostalgia, comments and/or praise can be sent to: Eirik.Eng@troll.no
**
****************************************************************************/

#ifndef GTETRIS_H
#define GTETRIS_H

#include <qpoint.h>

#include <krandomsequence.h>
#include <kgrid2d.h>


class Piece;
class BlockInfo;
class Block;

struct GTInitData {
    int  seed;
    uint initLevel;
};

class GenericTetris
{
 public:
    GenericTetris(uint width, uint height, bool withPieces, bool graphic);
    virtual ~GenericTetris();
    virtual void copy(const GenericTetris &);

    void setBlockInfo(BlockInfo *main, BlockInfo *next);
    virtual void start(const GTInitData &);

    uint moveLeft(int steps = 1)  { return moveTo(KGrid2D::Coord(-steps, 0)); }
    uint moveRight(int steps = 1) { return moveTo(KGrid2D::Coord(steps, 0)); }
    bool rotateLeft()             { return rotate(true); }
    bool rotateRight()            { return rotate(false); }
    virtual void oneLineDown();
    virtual void dropDown();

    KRandomSequence &randomSequence() { return _random; }
    uint score() const          { return _score; }
    uint level() const          { return _level; }
    uint nbClearLines() const   { return _nbClearLines; }
    uint nbRemoved() const      { return _nbRemoved; }
    bool graphic() const        { return _graphic; }
    uint firstClearLine() const { return _matrix.height() - _nbClearLines; }
    const KGrid2D::Coord &currentPos() const { return _currentPos; }
    const Piece *nextPiece() const    { return _nextPiece; }
    const Piece *currentPiece() const { return _currentPiece; }
    const KGrid2D::Square<Block *> &matrix() const { return _matrix; }

 protected:
    Piece           *_nextPiece, *_currentPiece;
 
    virtual void pieceDropped(uint /*dropHeight*/) {}
    virtual bool newPiece(); // return false if cannot place new piece
    virtual void gluePiece();
    virtual void computeInfos();

    virtual void updateRemoved(uint newNbRemoved) { _nbRemoved = newNbRemoved;}
    virtual void updateScore(uint newScore)       { _score = newScore; }
    virtual void updateLevel(uint newLevel)       { _level = newLevel; }

    void setBlock(const KGrid2D::Coord &, Block *);
    virtual void removeBlock(const KGrid2D::Coord &);
    void moveBlock(const KGrid2D::Coord &src, const KGrid2D::Coord &dest);

    virtual void updateNextPiece()   {}
    virtual void updatePieceConfig() {}
    void bumpCurrentPiece(int dec);
    void partialMoveBlock(const KGrid2D::Coord &, const QPoint &dec);

 private:
    QPoint toPoint(const KGrid2D::Coord &) const;
    uint moveTo(const KGrid2D::Coord &dec);
    bool rotate(bool left);
    void clear();
    bool canPosition(const KGrid2D::Coord &newPos, const Piece *newPiece) const;

    GenericTetris(const GenericTetris &); // disabled

    uint             _score, _level, _nbRemoved;
    uint             _nbClearLines, _initLevel;
    KGrid2D::Coord    _currentPos;
    BlockInfo       *_main;
    bool             _graphic;
    KGrid2D::Square<Block *> _matrix;
    KRandomSequence  _random;
};

#endif
