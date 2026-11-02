/* Yo Emacs, this -*- C++ -*-

  Copyright (C) 1999-2001 Jens Hoefkens
  jens@hoefkens.com

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

  $Id: kbginvite.cpp,v 1.4 2004/04/14 16:27:24 binner Exp $

*/

#include "kbginvite.h"
#include "kbginvite.moc"

#include <qlabel.h>
#include <qlayout.h>
#include <qframe.h>
#include <qspinbox.h>
#include <qstring.h>

#include <klocale.h>
#include <klineedit.h>
#include <kpushbutton.h>
#include <kstdguiitem.h>

class KBgInvitePrivate {

public:

    KLineEdit    *mLe;
    QSpinBox     *mSb;
    QPushButton  *mInvite, *mResume, *mUnlimited, *mCancel, *mClose;

};

/*
 * Constructor is quite simple - most positioning is left to
 * the toolkit.
 */
KBgInvite::KBgInvite(const char *name)
    : KDialog(0, name, false)
{
    setCaption(i18n("Invite Players"));

    d = new KBgInvitePrivate();

    QLabel *info = new QLabel(this);

    d->mLe = new KLineEdit(this, "invitation dialog");
    d->mSb = new QSpinBox(1, 999, 1, this, "spin box");

    d->mInvite    = new QPushButton(i18n("&Invite"),    this);
    d->mResume    = new QPushButton(i18n("&Resume"),    this);
    d->mUnlimited = new QPushButton(i18n("&Unlimited"), this);

    d->mClose     = new KPushButton(KStdGuiItem::close(),     this);
    d->mCancel    = new KPushButton(KStdGuiItem::clear(),     this);

    info->setText(i18n("Type the name of the player you want to invite in the first entry\n"
                       "field and select the desired match length in the spin box."));

    QFrame *hLine = new QFrame(this);
    hLine->setFrameStyle(QFrame::Sunken|QFrame::HLine);

    /*
     * Set up layouts
     */
    QBoxLayout *vbox   = new QVBoxLayout(this);

    QBoxLayout *hbox_1 = new QHBoxLayout(vbox);
    QBoxLayout *hbox_2 = new QHBoxLayout(vbox);
    QBoxLayout *hbox_3 = new QHBoxLayout(vbox);
    QBoxLayout *hbox_4 = new QHBoxLayout(vbox);
    QBoxLayout *hbox_5 = new QHBoxLayout(vbox);

    hbox_1->addWidget(info);

    hbox_2->addWidget(d->mLe);
    hbox_2->addWidget(d->mSb);

    hbox_3->addWidget(hLine);

    hbox_4->addWidget(d->mInvite);
    hbox_4->addWidget(d->mResume);
    hbox_4->addWidget(d->mUnlimited);

    hbox_5->addWidget(d->mClose);
    hbox_5->addWidget(d->mCancel);

    /*
     * Adjust widget sizes and resize the dialog
     */
    KDialog::resizeLayout(this, marginHint(), spacingHint());
    setMinimumSize(childrenRect().size());
    vbox->activate();
    resize(minimumSize());

    /*
     * Set focus and default buttons
     */
    d->mInvite->setDefault(true);
    d->mInvite->setAutoDefault(true);
    d->mLe->setFocus();

    /*
     * Connect the buttons
     */
    connect(d->mUnlimited, SIGNAL(clicked()), SLOT(unlimitedClicked()));
    connect(d->mResume,    SIGNAL(clicked()), SLOT(resumeClicked()));
    connect(d->mInvite,    SIGNAL(clicked()), SLOT(inviteClicked()));
    connect(d->mClose,     SIGNAL(clicked()), SLOT(hide()));
    connect(d->mCancel,    SIGNAL(clicked()), SLOT(cancelClicked()));
}

/*
 * Destructor
 */
KBgInvite::~KBgInvite()
{
    delete d;
}

/*
 * After hiding, we tell our creator that we are ready to die.
 */
void KBgInvite::hide()
{
    emit dialogDone();
}

/*
 * Set player name
 */
void KBgInvite::setPlayer(const QString &player)
{
    d->mLe->setText(player);
}

/*
 * Invitation with number
 */
void KBgInvite::inviteClicked()
{
    QString tmp;
    emit inviteCommand(QString("invite ") + d->mLe->text() + " " + tmp.setNum(d->mSb->value()));
}

/*
 * Invitation for unlimited match
 */
void KBgInvite::unlimitedClicked()
{
    emit inviteCommand(QString("invite ") + d->mLe->text() + " unlimited");
}

/*
 * Resume a game
 */
void KBgInvite::resumeClicked()
{
    emit inviteCommand(QString("invite ") + d->mLe->text());
}

/*
 * Slot for Cancel. clear everything to default.
 */
void KBgInvite::cancelClicked()
{
    d->mSb->setValue(1);
    d->mLe->clear();
}

// EOF
