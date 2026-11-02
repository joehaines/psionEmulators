#include "ai.h"
#include "ai.moc"

#include <klocale.h>


const AI::Data KSAI::DATA[] = {
    { "OccupiedLines", I18N_NOOP("Occupied lines:"), 0,
      false, nbOccupiedLines },
    { "Holes", I18N_NOOP("Number of holes:"), 0,
      false, nbHoles },
    { "Spaces", I18N_NOOP("Number of spaces:"), I18N_NOOP("Number of spaces under mean height"),
      false, nbSpaces },
    { "Peak2Peak", I18N_NOOP("Peak-to-peak distance:"), 0,
      false, peakToPeak },
    { "MeanHeight", I18N_NOOP("Mean height:"), 0,
      false, mean },
    { "FullLines", I18N_NOOP("Number of full lines:"), 0,
      true, nbRemoved },
    LastData
};

KSAI::KSAI()
    : AI(0, 200, DATA) // #### tune or configure times...
{}
