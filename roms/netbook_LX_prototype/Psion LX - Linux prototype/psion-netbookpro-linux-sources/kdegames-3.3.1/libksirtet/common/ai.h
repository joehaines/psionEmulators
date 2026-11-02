#ifndef COMMON_AI_H
#define COMMON_AI_H

#include <qtimer.h>
#include <qvaluevector.h>

#include <kdialogbase.h>
#include <knuminput.h>
#include <krandomsequence.h>

class Board;
class Piece;


//-----------------------------------------------------------------------------
class AIPiece
{
 public:
    AIPiece();
    ~AIPiece();

    void init(const Piece *p, Board *b);
    bool place();
    bool increment();

    int dec() const  { return curDec; }
    uint rot() const { return curRot; }

 private:
    uint         nbPos, nbRot, curPos, curRot;
    int          curDec;
    const Piece *_piece;
    Piece       *_current;
    Board       *_board;

    void reset();
};

//-----------------------------------------------------------------------------
class AI : public QObject
{
 Q_OBJECT
 public:
    struct Data {
        const char *name, *label, *whatsthis;
        bool triggered;
        double (*function)(const Board &, const Board &);
    };
    static const Data LastData;

    AI(uint thinkTime, uint orderTime, const Data *DATA);
    virtual ~AI();

    void launch(Board *main);
    void stop();
    void start();

    class Element {
    public:
        const Data *data;
        double coefficient;
        int trigger;
    };
    const QValueVector<Element> &elements() const { return _elements; }

    void settingsChanged();

 private slots:
    void timeout();

 protected:
    virtual void initThink();

    static double nbOccupiedLines(const Board &, const Board &);
    static double nbHoles(const Board &, const Board &);
    static double nbSpaces(const Board &, const Board &);
    static double peakToPeak(const Board &, const Board &);
    static double mean(const Board &, const Board &);
    static double nbRemoved(const Board &, const Board &);

 private:
    bool think();
    void startTimer();
    bool emitOrder();
    double points() const;
    void resizePieces(uint size);

    QTimer                 timer;
    enum ThinkState { Thinking, GivingOrders };
    ThinkState             state;
    uint                   thinkTime, orderTime;
    bool                   stopped;
    QMemArray<AIPiece *>   pieces;
    QValueVector<Element>  _elements;
    Board                 *main, *board;
    KRandomSequence        random;

    bool   hasBestPoints;
    double bestPoints;
    int    bestDec;
    uint   bestRot;
};

//-----------------------------------------------------------------------------
class AIConfig : public QWidget
{
 Q_OBJECT
 public:
    AIConfig(const QValueVector<AI::Element> &elements);

    static double coefficient(const AI::Data &data);
    static int trigger(const AI::Data &data);

 private:
    static QCString triggerKey(const char *name);
    static QCString coefficientKey(const char *name);

    static const uint minThinkingDepth, maxThinkingDepth;
};

#endif
