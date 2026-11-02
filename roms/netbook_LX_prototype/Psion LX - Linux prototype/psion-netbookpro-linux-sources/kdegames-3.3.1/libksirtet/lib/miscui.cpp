#include "miscui.h"
#include "miscui.moc"

#include <qlayout.h>

#include <klocale.h>


//-----------------------------------------------------------------------------
MeetingCheckBox::MeetingCheckBox(Type type, bool owner, bool server,
                                 QWidget *parent)
    : QWidget(parent, "meeting_check_box")
{
    QVBoxLayout *vbox = new QVBoxLayout(this);

    _ready = new QCheckBox(i18n("Ready"), this);
    vbox->addWidget(_ready);
    _ready->setEnabled(owner);
    connect(_ready, SIGNAL(clicked()), SLOT(changedSlot()));

    _excluded = new QCheckBox(i18n("Excluded"), this);
    vbox->addWidget(_excluded);
    _excluded->setEnabled(server);
    connect(_excluded, SIGNAL(clicked()), SLOT(changedSlot()));

    setType(type);
}

void MeetingCheckBox::setType(Type type)
{
    _ready->setChecked( type==Ready );
    _excluded->setChecked( type==Excluded );
}

MeetingCheckBox::Type MeetingCheckBox::type() const
{
    if ( _excluded->isChecked() ) return Excluded;
    if ( _ready->isChecked() ) return Ready;
    return NotReady;
}

void MeetingCheckBox::changedSlot()
{
    emit changed(type());
}

//-----------------------------------------------------------------------------
PlayerComboBox::PlayerComboBox(Type type, bool canBeEmpty, bool acceptAI,
                               QWidget *parent)
    : QComboBox(parent, "player_combo_box")
{
    insertItem(i18n("Human"));
    if (acceptAI) insertItem(i18n("AI"));
    if (canBeEmpty) insertItem(i18n("None"));
    setCurrentItem(type);

    connect(this, SIGNAL(activated(int)), SIGNAL(changed(int)));
}
