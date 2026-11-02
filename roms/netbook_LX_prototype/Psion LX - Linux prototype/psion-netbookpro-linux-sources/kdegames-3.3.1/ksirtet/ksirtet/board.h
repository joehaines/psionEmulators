#ifndef KS_BOARD_H
#define KS_BOARD_H

#include "common/board.h"


class KSBoard : public Board
{
 Q_OBJECT
 public:
	KSBoard(bool graphic, QWidget *parent);
	void copy(const GenericTetris &);

	void start(const GTInitData &);
    enum Constants { Width = 10, Nb_Stages = 6 };
    uint nbRemovedLines(uint i) const { return linesRemoved[i]; }

 signals:
    void removedLinesUpdated(uint i, uint nb);

 private:
    QMemArray<uint> filled;
    QMemArray<uint> linesRemoved;
	uint            addRemoved;

    struct StageData {
        uint todo, speed;
        bool hasPattern;
        uint pattern[Width];
    };
    static const StageData STAGE_DATA[Nb_Stages];

	uint gift();
	bool putGift(uint nb);
	bool _putGift(uint nb);
	bool needRemoving();
    void remove();
    bool toBeRemoved(const KGrid2D::Coord &) const;
    bool toFall(const KGrid2D::Coord &) const;
    void computeInfos();
    void setGarbageBlock(const KGrid2D::Coord &);

    const StageData &stageData() const { return STAGE_DATA[arcadeStage()]; }
    uint normalTime() const;
    uint arcadeTodo() const { return stageData().todo; }
    uint arcadeDone() const { return nbRemoved(); }
    void arcadePrepare();
};

#endif
