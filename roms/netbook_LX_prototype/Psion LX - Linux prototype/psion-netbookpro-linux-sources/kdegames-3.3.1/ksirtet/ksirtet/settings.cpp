#include "settings.h"
#include "settings.moc"

#include <qlayout.h>
#include <qcheckbox.h>

#include <klocale.h>


KSGameConfig::KSGameConfig()
{
    int row = _grid->numRows();
    int col = _grid->numCols();

    QCheckBox *cb = new QCheckBox(i18n("Old rotation style"), this, "kcfg_OldRotationStyle");
    _grid->addMultiCellWidget(cb, row, row, 0, col-1);
}
