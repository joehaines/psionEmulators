#ifndef FE_BOARD_H
#define FE_BOARD_H

#include "common/board.h"


class FEBoard : public Board
{
 Q_OBJECT
 public:
    FEBoard(bool graphic, QWidget *parent);
	void copy(const GenericTetris &);

	void start(const GTInitData &);

    uint nbPuyos() const { return _nbPuyos; }
    uint chained() const { return _chained; }
    uint nbChainedPuyos(uint i) const { return _chainedPuyos[i]; }

 signals:
    void removedUpdated(uint chain, uint removed);

 private:
	// standard methods
	bool afterGlue(bool doAll, bool first);
	AfterRemoveResult afterRemove(bool doAll, bool first);
	bool afterGift(bool first) { return !doFall(false, first, true); };
    void removeBlock(const KGrid2D::Coord &);
    bool toBeRemoved(const KGrid2D::Coord &) const;
    bool toFall(const KGrid2D::Coord &) const;
	void remove();
	bool needRemoving();
    void computeInfos();

	// Multiplayers methods
	uint gift();
	bool putGift(uint);

	KGrid2D::Square<int> _field;
	QMemArray<uint>     _groups, _chainedPuyos;
	uint                _nbPuyos, _chained, _giftRest;
};

#endif
