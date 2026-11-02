#include "ai.h"
#include "ai.moc"

#include <klocale.h>

#include "board.h"


const AI::Data FEAI::DATA[] = {
    { "OccupiedLines", I18N_NOOP("Occupied lines:"), 0,
      false, nbOccupiedLines },
    { "Spaces", I18N_NOOP("Number of spaces:"), I18N_NOOP("Number of spaces under mean height"),
      false, nbSpaces },
    { "Peak2Peak", I18N_NOOP("Peak-to-peak distance:"), 0,
      false, peakToPeak },
    { "MeanHeight", I18N_NOOP("Mean height:"), 0,
      false, mean },
    { "RemovedEggs", I18N_NOOP("Number of removed eggs:"), 0,
      false, nbRemoved },
    { "Puyos", I18N_NOOP("Number of puyos:"), 0,
      true, nbPuyos },
    { "ChainedPuyos", I18N_NOOP("Number of chained puyos:"), 0,
      true, nbChained },
    LastData
};

FEAI::FEAI()
: AI(0, 200, DATA)
{}

double FEAI::nbPuyos(const Board &main, const Board &current)
{
    return static_cast<const FEBoard &>(current).nbPuyos()
        - static_cast<const FEBoard &>(main).nbPuyos();
}

double FEAI::nbChained(const Board &, const Board &current)
{
    return static_cast<const FEBoard &>(current).chained();
}
