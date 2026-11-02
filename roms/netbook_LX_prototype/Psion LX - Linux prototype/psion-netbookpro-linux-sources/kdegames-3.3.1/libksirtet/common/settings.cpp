#include "settings.h"
#include "settings.moc"

#include <qlabel.h>
#include <qlayout.h>
#include <qcheckbox.h>
#include <qwhatsthis.h>

#include <klocale.h>
#include <knuminput.h>
#include <kdialog.h>
#include <kglobal.h>


//-----------------------------------------------------------------------------
AppearanceConfig::AppearanceConfig()
{
    int row = _grid->numRows();
    int col = _grid->numCols();

    QCheckBox *cb = new QCheckBox(i18n("Show piece's shadow"), _main, "kcfg_ShowPieceShadow");
    _grid->addMultiCellWidget(cb, row, row, 0, col-1);

    cb = new QCheckBox(i18n("Show next piece"), _main, "kcfg_ShowNextPiece");
    _grid->addMultiCellWidget(cb, row+1, row+1, 0, col-1);

    cb = new QCheckBox(i18n("Show detailed \"removed lines\" field"), _main, "kcfg_ShowDetailedRemoved");
    _grid->addMultiCellWidget(cb, row+2, row+2, 0, col-1);
}

//-----------------------------------------------------------------------------
GameConfig::GameConfig()
    : QWidget(0, "game config")
{
    QVBoxLayout *top = new QVBoxLayout(this, KDialog::marginHint(), KDialog::spacingHint());

    _grid = new QGridLayout(top, 3, 2);
    _grid->setColStretch(1, 1);

    QLabel *label = new QLabel(i18n("Initial level:"), this);
    _grid->addWidget(label, 0, 0);
    KIntNumInput *in = new KIntNumInput(this, "kcfg_InitialGameLevel");
    in->setRange(1, 20, 1, true);
    _grid->addWidget(in, 0, 1);

    _grid->addRowSpacing(1, KDialog::spacingHint());

    QCheckBox *cb = new QCheckBox(i18n("Direct drop down"), this, "kcfg_DirectDropDownEnabled");
    QWhatsThis::add(cb, i18n("Drop down is not stopped when drop down key is released."));
    _grid->addMultiCellWidget(cb, 2, 2, 0, 1);

    top->addStretch(1);
}

