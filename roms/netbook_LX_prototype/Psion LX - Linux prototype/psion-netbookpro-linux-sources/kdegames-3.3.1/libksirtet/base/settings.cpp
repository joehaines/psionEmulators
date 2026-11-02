#include "settings.h"
#include "settings.moc"

#include <qlabel.h>
#include <qhbox.h>
#include <qlayout.h>
#include <qcheckbox.h>
#include <qhgroupbox.h>

#include <klocale.h>
#include <knuminput.h>
#include <kcolorbutton.h>
#include <kapplication.h>
#include <kdialogbase.h>

#include "piece.h"
#include "factory.h"


//-----------------------------------------------------------------------------
BaseAppearanceConfig::BaseAppearanceConfig()
    : QWidget(0, "appearance_config")
{
    QVBoxLayout *top = new QVBoxLayout(this);

    // upper part
    _main = new QWidget(this);
    top->addWidget(_main);
    _grid = new QGridLayout(_main, 3, 2, 0, KDialog::spacingHint());
    _grid->setColStretch(1, 1);

    QCheckBox *chb =
        new QCheckBox(i18n("Enable animations"), _main, "kcfg_AnimationsEnabled");
    _grid->addMultiCellWidget(chb, 2, 2, 0, 1);

    top->addSpacing(KDialog::spacingHint());

    // lower part
    QHGroupBox *gbox = new QHGroupBox(i18n("Background"), this);
    top->addWidget(gbox);
    QWidget *widget = new QWidget(gbox);
    QGridLayout *grid =
        new QGridLayout(widget, 2, 3, 0, KDialog::spacingHint());
    grid->setColStretch(2, 1);
    QLabel *label = new QLabel(i18n("Color:"), widget);
    grid->addWidget(label, 0, 0);
    KColorButton *cob = new KColorButton(widget, "kcfg_FadeColor");
    cob->setFixedWidth(100);
    grid->addWidget(cob, 0, 1);
    label = new QLabel(i18n("Opacity:"), widget);
    grid->addWidget(label, 1, 0);
    KDoubleNumInput *dn = new KDoubleNumInput(widget, "kcfg_FadeIntensity");
    dn->setRange(0.0, 1.0, 0.01);
    grid->addMultiCellWidget(dn, 1, 1, 1, 2);

    top->addStretch(1);
}

//-----------------------------------------------------------------------------
ColorConfig::ColorConfig()
    : QWidget(0, "color_config")
{
    const GPieceInfo &info = Piece::info();
    QVBoxLayout *top = new QVBoxLayout(this);
    uint nb = info.nbColors();
    QGridLayout *grid = new QGridLayout(top, nb+1, 3, KDialog::spacingHint());
    grid->setColStretch(2, 1);
    for (uint i=0; i<nb; i++) {
        QLabel *label = new QLabel(info.colorLabel(i), this);
        grid->addWidget(label, i, 0);
        KColorButton *cob = new KColorButton(this, colorKey(i));
        cob->setFixedWidth(100);
        grid->addWidget(cob, i, 1);
    }
    grid->setRowStretch(nb, 1);
}

QCString ColorConfig::colorKey(uint i)
{
    QCString s;
    s.setNum(i);
    return "kcfg_Color" + s;
}

