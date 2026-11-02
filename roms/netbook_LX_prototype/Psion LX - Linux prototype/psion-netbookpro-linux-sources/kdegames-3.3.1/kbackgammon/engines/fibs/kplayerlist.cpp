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

  $Id: kplayerlist.cpp,v 1.19 2004/07/12 20:19:56 binner Exp $

*/

#include "kplayerlist.moc"
#include "kplayerlist.h"

#include <qlayout.h>
#include <qiconset.h>
#include <qgroupbox.h>
#include <qpopupmenu.h>
#include <qcheckbox.h>
#include <qlistview.h>
#include <qwhatsthis.h>
#include <qdatetime.h>
#include <qlabel.h>

#include <kapplication.h>
#include <kconfig.h>
#include <kaction.h>
#include <kstdaction.h>
#include <ktabctl.h>
#include <kiconloader.h>
#include <kdebug.h>

#include <string.h>
#include <stdio.h>

#include "kbgfibs.h"
#include "version.h"


/*
 * Simple container for information on columns of the list view.
 *
 * index : the internal index in the list
 * width : width of the column in pixel
 * show  : whether the column is visible
 * cb    : check box for the setup dialog
 */
class KFibsPlayerListCI {

public:

    int index, width;
    bool show;
    QCheckBox *cb;
    QString key, name;

};

/*
 * Extension of the QListViewItem class that has a custom key function
 * that can deal with the different items of the player list.
 */
class KFibsPlayerListLVI : public KListViewItem {

public:

    /*
     * Constructor
     */
    KFibsPlayerListLVI(KFibsPlayerList *parent) : KListViewItem(parent) { _plist = parent; }

    /*
     * Destructor
     */
    virtual ~KFibsPlayerListLVI() {}

    /*
     * Overloaded key function for sorting
     */
    virtual QString key(int col, bool) const
    {
        int real_col = _plist->cIndex(col);

        QString s = text(col);

        switch (real_col) {
        case KFibsPlayerList::Player:
        case KFibsPlayerList::Opponent:
        case KFibsPlayerList::Watches:
        case KFibsPlayerList::Client:
        case KFibsPlayerList::Email:
        case KFibsPlayerList::Status:
        case KFibsPlayerList::Host:
            s = s.lower();
            break;
        case KFibsPlayerList::Idle:
        case KFibsPlayerList::Experience:
            s.sprintf("%08d", s.toInt());
            break;
        case KFibsPlayerList::Rating:
            s.sprintf("%08d", (int)(1000*s.toDouble()));
            break;
        case KFibsPlayerList::Time:
            s = s.lower();
            break;
        default:
            kdDebug(10500) << "KFibsPlayerListLVI::key(): illegal column" << endl;
            break;
        }
        return s;
    }

private:

    KFibsPlayerList *_plist;

};

/*
 * Private data of the player list
 */
class KFibsPlayerListPrivate {

public:

    /*
     * Named constants for the popup menu actions
     */
    enum MenuID {Info, Talk, Mail, InviteD, Invite1, Invite2, Invite3, Invite4,
                 Invite5, Invite6, Invite7, InviteR, InviteU,
                 Look, Watch, Unwatch, BlindAct, Update, Reload, Close, ActionEnd};

    /*
     * Various actions for the context menu
     */
    KAction *mAct[ActionEnd];

    /*
     * All relevant information on the columns
     */
    KFibsPlayerListCI *mCol[KFibsPlayerList::LVEnd];

    /*
     * Context menus for player related commands
     */
    QPopupMenu *mPm[2];

    /*
     * ID of the invite menu in the context menu
     */
    int mInID;

    /*
     * Are we watching?
     */
    bool mWatch;

    /*
     * count similar clients - KFibs & kbackgammon
     */
    int mCount[2];

    /*
     * Short abbreviations for Blind, Ready, and Away.
     */
    QString mAbrv[KFibsPlayerList::MaxStatus];

    /*
     * Name of the last selected player - for internal purposes
     */
    QString mUser;

    /*
     * Our own name
     */
    QString mName;

    /*
     * Email address of the last selected player - for internal purposes
     */
    QString mMail;

};


// == constructor, destructor and setup ========================================

/*
 * Construct the playerlist and do some initial setup
 */
KFibsPlayerList::KFibsPlayerList(QWidget *parent, const char *name)
    : KListView(parent, name)
{
    d = new KFibsPlayerListPrivate();
    KActionCollection* actions = new KActionCollection(this);

    /*
     * Allocate the column information
     */
    for (int i = 0; i < LVEnd; i++)
        d->mCol[i] = new KFibsPlayerListCI();

    /*
     * Initialize variables
     */
    d->mCol[Player]->name     = i18n("Player");
    d->mCol[Opponent]->name   = i18n("Opponent");
    d->mCol[Watches]->name    = i18n("Watches");
    d->mCol[Status]->name     = i18n("Status");
    d->mCol[Rating]->name     = i18n("Rating");
    d->mCol[Experience]->name = i18n("Exp.");
    d->mCol[Idle]->name       = i18n("Idle");
    d->mCol[Time]->name       = i18n("Time");
    d->mCol[Host]->name       = i18n("Host name");
    d->mCol[Client]->name     = i18n("Client");
    d->mCol[Email]->name      = i18n("Email");

    // These strings shouldn't be translated!!
    d->mCol[Player]->key     = "player";
    d->mCol[Opponent]->key   = "opponent";
    d->mCol[Watches]->key    = "watches";
    d->mCol[Status]->key     = "status";
    d->mCol[Rating]->key     = "rating";
    d->mCol[Experience]->key = "experience";
    d->mCol[Idle]->key       = "idle";
    d->mCol[Time]->key       = "time";
    d->mCol[Host]->key       = "hostname";
    d->mCol[Client]->key     = "client";
    d->mCol[Email]->key      = "email";

    d->mCount[0] = d->mCount[1] = 0;

    d->mAbrv[Blind] = i18n("abreviate blind", "B");
    d->mAbrv[Away ] = i18n("abreviate away",  "A");
    d->mAbrv[Ready] = i18n("abreviate ready", "R");

    d->mName = QString::null;

    d->mWatch = false;

    /*
     * Get a sane caption, initialize some eye candy and read the
     * configuration - needed for the column information.
     */
    updateCaption();
    setIcon(kapp->miniIcon());
    QWhatsThis::add(this, i18n("This window contains the player list. It shows "
                               "all players that are currently logged into FIBS."
                               "Use the right mouse button to get a context "
                               "menu with helpful information and commands."));

    readColumns();

    /*
     * Put the columns into the list view
     */
    for (int i = 0; i < LVEnd; i++) {
        if (d->mCol[i]->show) {
            d->mCol[i]->index = addColumn(d->mCol[i]->name, d->mCol[i]->width);
            if (i == Experience || i == Rating || i == Time || i == Idle)
                setColumnAlignment(d->mCol[i]->index, AlignRight);
        } else {
            d->mCol[i]->index = -1;
        }
    }
    setAllColumnsShowFocus(true);

    /*
     * Create context menus
     */
    d->mPm[0] = new QPopupMenu();
    d->mPm[1] = new QPopupMenu();

    /*
     * Create the whole set of actions
     */
    d->mAct[KFibsPlayerListPrivate::Info] = new KAction(i18n("Info"),
                                                        QIconSet(kapp->iconLoader()->loadIcon
                                                                 ("help.xpm", KIcon::Small)),
                                                        0, this, SLOT(slotInfo()),   actions);
    d->mAct[KFibsPlayerListPrivate::Talk] = new KAction(i18n("Talk"),
                                                        QIconSet(kapp->iconLoader()->loadIcon
                                                                 (PROG_NAME "-chat.png", KIcon::Small)),
                                                        0, this, SLOT(slotTalk()),   actions);

    d->mAct[KFibsPlayerListPrivate::Look]     = new KAction(i18n("Look"), 0, this, SLOT(slotLook()),   actions);
    d->mAct[KFibsPlayerListPrivate::Watch]    = new KAction(i18n("Watch"), 0, this, SLOT(slotWatch()),  actions);
    d->mAct[KFibsPlayerListPrivate::Unwatch]  = new KAction(i18n("Unwatch"), 0, this, SLOT(slotUnwatch()),actions);
    d->mAct[KFibsPlayerListPrivate::BlindAct] = new KAction(i18n("Blind"), 0, this, SLOT(slotBlind()),  actions);
    d->mAct[KFibsPlayerListPrivate::Update]   = new KAction(i18n("Update"), 0, this, SLOT(slotUpdate()), actions);

    d->mAct[KFibsPlayerListPrivate::Reload] = KStdAction::redisplay(this, SLOT(slotReload()), actions);
    d->mAct[KFibsPlayerListPrivate::Mail]   = KStdAction::mail(this, SLOT(slotMail()),   actions);
    d->mAct[KFibsPlayerListPrivate::Close]  = KStdAction::close(this, SLOT(hide()), actions);

    d->mAct[KFibsPlayerListPrivate::InviteD]  = new KAction(i18n("Use Dialog"),    0, this,
                                                            SLOT(slotInviteD()), actions);
    d->mAct[KFibsPlayerListPrivate::Invite1]  = new KAction(i18n("1 Point Match"), 0, this,
                                                            SLOT(slotInvite1()), actions);
    d->mAct[KFibsPlayerListPrivate::Invite2]  = new KAction(i18n("2 Point Match"), 0, this,
                                                            SLOT(slotInvite2()), actions);
    d->mAct[KFibsPlayerListPrivate::Invite3]  = new KAction(i18n("3 Point Match"), 0, this,
                                                            SLOT(slotInvite3()), actions);
    d->mAct[KFibsPlayerListPrivate::Invite4]  = new KAction(i18n("4 Point Match"), 0, this,
                                                            SLOT(slotInvite4()), actions);
    d->mAct[KFibsPlayerListPrivate::Invite5]  = new KAction(i18n("5 Point Match"), 0, this,
                                                            SLOT(slotInvite5()), actions);
    d->mAct[KFibsPlayerListPrivate::Invite6]  = new KAction(i18n("6 Point Match"), 0, this,
                                                            SLOT(slotInvite6()), actions);
    d->mAct[KFibsPlayerListPrivate::Invite7]  = new KAction(i18n("7 Point Match"), 0, this,
                                                            SLOT(slotInvite7()), actions);
    d->mAct[KFibsPlayerListPrivate::InviteU]  = new KAction(i18n("Unlimited"),     0, this,
                                                            SLOT(slotInviteU()), actions);
    d->mAct[KFibsPlayerListPrivate::InviteR]  = new KAction(i18n("Resume"),        0, this,
                                                            SLOT(slotInviteR()), actions);

    /*
     * Fill normal context menu
     */
    d->mAct[KFibsPlayerListPrivate::Info]->plug(d->mPm[0]);
    d->mAct[KFibsPlayerListPrivate::Talk]->plug(d->mPm[0]);
    d->mAct[KFibsPlayerListPrivate::Mail]->plug(d->mPm[0]);
    d->mPm[0]->insertSeparator();
    d->mInID = d->mPm[0]->insertItem(i18n("Invite"), d->mPm[1]); // save ID for later
    d->mAct[KFibsPlayerListPrivate::Look    ]->plug(d->mPm[0]);
    d->mAct[KFibsPlayerListPrivate::Watch   ]->plug(d->mPm[0]);
    d->mAct[KFibsPlayerListPrivate::Unwatch ]->plug(d->mPm[0]);
    d->mAct[KFibsPlayerListPrivate::BlindAct]->plug(d->mPm[0]);
    d->mPm[0]->insertSeparator();
    d->mAct[KFibsPlayerListPrivate::Update]->plug(d->mPm[0]);
    d->mAct[KFibsPlayerListPrivate::Reload]->plug(d->mPm[0]);
    d->mPm[0]->insertSeparator();
    d->mAct[KFibsPlayerListPrivate::Close]->plug(d->mPm[0]);

    /*
     * Fill the invitation menu
     */
    d->mAct[KFibsPlayerListPrivate::InviteD]->plug(d->mPm[1]);
    d->mPm[1]->insertSeparator();
    d->mAct[KFibsPlayerListPrivate::Invite1]->plug(d->mPm[1]);
    d->mAct[KFibsPlayerListPrivate::Invite2]->plug(d->mPm[1]);
    d->mAct[KFibsPlayerListPrivate::Invite3]->plug(d->mPm[1]);
    d->mAct[KFibsPlayerListPrivate::Invite4]->plug(d->mPm[1]);
    d->mAct[KFibsPlayerListPrivate::Invite5]->plug(d->mPm[1]);
    d->mAct[KFibsPlayerListPrivate::Invite6]->plug(d->mPm[1]);
    d->mAct[KFibsPlayerListPrivate::Invite7]->plug(d->mPm[1]);
    d->mPm[1]->insertSeparator();
    d->mAct[KFibsPlayerListPrivate::InviteU]->plug(d->mPm[1]);
    d->mAct[KFibsPlayerListPrivate::InviteR]->plug(d->mPm[1]);

    /*
     * Right mouse button gets context menu, double click gets player info
     */
    connect(this, SIGNAL(contextMenu(KListView *, QListViewItem *, const QPoint &)),
            this, SLOT(showContextMenu(KListView *, QListViewItem *, const QPoint &)));
    connect(this, SIGNAL(doubleClicked(QListViewItem *, const QPoint &, int)),
            this, SLOT(getPlayerInfo(QListViewItem *, const QPoint &, int)));
}

/*
 * Destructor deletes members
 */
KFibsPlayerList::~KFibsPlayerList()
{
    for (int i = 0; i < LVEnd; i++)
        delete d->mCol[i];
    delete d->mPm[0];
    delete d->mPm[1];
    delete d;
}


// == settings and config ======================================================

/*
 * Called when the setup dialog is positively closed
 */
void KFibsPlayerList::setupOk()
{
    int i;
    bool change = false;

    for (i = 1; i < LVEnd; i++)
        change |= (d->mCol[i]->cb->isChecked() != d->mCol[i]->show);

    /*
     * Only juggle with the columns if something changed
     */
    if (change) {

        /*
         * It's important to remove the columns in reverse order
         */
        for (i = LVEnd-1; i > 0; i--)
            if (d->mCol[i]->show)
                removeColumn(d->mCol[i]->index);

        /*
         * Now add all columns that are selected
         */
        for (i = 1; i < LVEnd; i++) {
            if ((d->mCol[i]->show = d->mCol[i]->cb->isChecked())) {
                d->mCol[i]->index = addColumn(d->mCol[i]->name, d->mCol[i]->width);
                if (i == Experience || i == Rating || i == Time || i == Idle)
                    setColumnAlignment(d->mCol[i]->index, AlignRight);
            } else {
                d->mCol[i]->index = -1;
            }
        }

        /*
         * Reload the list
         */
        slotReload();
    }

    /*
     * store the new settings
     */
    saveConfig();

}

/*
 * Setup dialog page of the player list - allow the user to select the
 * columns to show
 */
void KFibsPlayerList::getSetupPages(KTabCtl *nb, int space)
{
    int i;

    /*
     * Main Widget
     */
    QWidget *w = new QWidget(nb);
    QGridLayout *gl = new QGridLayout(w, 2, 1, space);

    /*
     * Label
     */
    QGroupBox *gbl = new QGroupBox(w);
    gbl->setTitle(i18n("Column Selection"));

    gl->addWidget(gbl, 0, 0);

    /*
     * Note that the first column (Player == 0) is always there
     */
    QLabel *lb = new QLabel(i18n("Select all the columns that you would\n"
                                 "like to be shown in the player list."), gbl);

    for (i = 1; i < LVEnd; i++) {
        d->mCol[i]->cb = new QCheckBox(d->mCol[i]->name, gbl);
        d->mCol[i]->cb->setChecked(d->mCol[i]->show);
    }

    gl = new QGridLayout(gbl, LVEnd, 2, 20);
    gl->addWidget(lb, 0, 0);

    // two column layout....
    for (i = 1; i < LVEnd/2; i++) {
        gl->addWidget(d->mCol[2*i-1]->cb, i, 0);
        gl->addWidget(d->mCol[2*i  ]->cb, i, 1);
    }
    gl->addWidget(d->mCol[2*i-1]->cb, i, 0);
    if (2*i < LVEnd)
        gl->addWidget(d->mCol[2*i]->cb, i, 1);

    /*
     * put in the page and connect
     */
    nb->addTab(w, i18n("&Playerlist"));

    connect(nb, SIGNAL(applyButtonPressed()), this, SLOT(setupOk()));
}

/*
 * Nothing to cancel/undo
 */
void KFibsPlayerList::setupCancel()
{
    // do nothing
}

/*
 * By default all entries are checked
 */
void KFibsPlayerList::setupDefault()
{
    for (int i = 0; i < LVEnd; i++)
        d->mCol[i]->cb->setChecked(true);
}

/*
 * Restore the columns
 */
void KFibsPlayerList::readColumns()
{
    KConfig* config = kapp->config();
    config->setGroup(name());

    for (int i = 0; i < LVEnd; i++) {
        d->mCol[i]->show  = config->readBoolEntry("col-" + d->mCol[i]->key, true);
        d->mCol[i]->width = config->readNumEntry("col-w-" + d->mCol[i]->key, -1);
    }
}

/*
 * Restore the saved settings
 */
void KFibsPlayerList::readConfig()
{
    KConfig* config = kapp->config();
    config->setGroup(name());

    QPoint pos, defpos(10, 10);
    pos = config->readPointEntry("ori", &defpos);
    setGeometry(pos.x(), pos.y(), config->readNumEntry("wdt",460),
                config->readNumEntry("hgt",190));

    (config->readBoolEntry("vis", false)) ? show() : hide();

    readColumns();
}

/*
 * Save current settings
 */
void KFibsPlayerList::saveConfig()
{
    KConfig* config = kapp->config();
    config->setGroup(name());

    config->writeEntry("ori", pos());
    config->writeEntry("hgt", height());
    config->writeEntry("wdt", width());

    config->writeEntry("vis", isVisible());

    for (int i = 0; i < LVEnd; i++) {
        config->writeEntry("col-" + d->mCol[i]->key, d->mCol[i]->show);
        config->writeEntry("col-w-" + d->mCol[i]->key,
                           (d->mCol[i]->show) ? columnWidth(d->mCol[i]->index) : -1);
    }
}


// == popup menu slots and functions ===========================================

/*
 * Save selected player, update the menu entries and show the popup menu
 */
void KFibsPlayerList::showContextMenu(KListView *, QListViewItem *i, const QPoint &p)
{
    /*
     * Get the name of the selected player
     */
    d->mUser = (i ? i->text(Player) : QString::null);

    d->mAct[KFibsPlayerListPrivate::Info  ]->setText(i18n("Info on %1" ).arg(d->mUser));
    d->mAct[KFibsPlayerListPrivate::Talk  ]->setText(i18n("Talk to %1" ).arg(d->mUser));
    d->mAct[KFibsPlayerListPrivate::Mail  ]->setText(i18n("Email to %1").arg(d->mUser));
    d->mAct[KFibsPlayerListPrivate::Look  ]->setText(i18n("Look at %1" ).arg(d->mUser));
    d->mAct[KFibsPlayerListPrivate::Watch ]->setText(i18n("Watch %1"   ).arg(d->mUser));
    d->mAct[KFibsPlayerListPrivate::Update]->setText(i18n("Update %1"  ).arg(d->mUser));

    d->mAct[KFibsPlayerListPrivate::Info    ]->setEnabled(i);
    d->mAct[KFibsPlayerListPrivate::Talk    ]->setEnabled(i);
    d->mAct[KFibsPlayerListPrivate::Mail    ]->setEnabled(i);
    d->mAct[KFibsPlayerListPrivate::Look    ]->setEnabled(i);
    d->mAct[KFibsPlayerListPrivate::Watch   ]->setEnabled(i);
    d->mAct[KFibsPlayerListPrivate::Update  ]->setEnabled(i);
    d->mAct[KFibsPlayerListPrivate::BlindAct]->setEnabled(i);

    d->mAct[KFibsPlayerListPrivate::Unwatch]->setEnabled(d->mWatch);

    d->mPm[0]->setItemEnabled(d->mInID, i && d->mName != d->mUser);
    d->mPm[0]->changeItem(d->mInID, i18n("Invite %1").arg(d->mUser));

    d->mMail = (i && d->mCol[Email]->show ? i->text(d->mCol[Email]->index) : QString::null);
    d->mAct[KFibsPlayerListPrivate::Mail]->setEnabled(!d->mMail.isEmpty());

    if (i && d->mCol[Status]->show)
        d->mAct[KFibsPlayerListPrivate::BlindAct]->setText
            ((i->text(d->mCol[Status]->index).contains(d->mAbrv[Blind])) ?
             i18n("Unblind %1").arg(d->mUser) : i18n("Blind %1").arg(d->mUser));
    else
        d->mAct[KFibsPlayerListPrivate::BlindAct]->setText(i18n("Blind"));

    // show the menu
    d->mPm[0]->popup(p);
}

/*
 * Reload the entire list
 */
void KFibsPlayerList::slotReload()
{
    emit fibsCommand("rawwho");
    clear();
}

/*
 * Stop watching
 */
void KFibsPlayerList::slotUnwatch()
{
    emit fibsCommand("unwatch");
}

/*
 * Blind/Unblind user
 */
void KFibsPlayerList::slotBlind()
{
    emit fibsCommand("blind " + d->mUser);
}

/*
 * Start talking to user
 */
void KFibsPlayerList::slotTalk()
{
    emit fibsTalk(d->mUser);
}

/*
 * Request information on user
 */
void KFibsPlayerList::slotInfo()
{
    emit fibsCommand("whois " + d->mUser);
}

/*
 * Look at user
 */
void KFibsPlayerList::slotLook()
{
    emit fibsCommand("look " + d->mUser);
}

/*
 * Send an email to player user at address email
 */
void KFibsPlayerList::slotMail()
{
    kapp->invokeMailer(d->mMail, QString::null);
}

/*
 * Request a new entry for user
 */
void KFibsPlayerList::slotUpdate()
{
    emit fibsCommand("rawwho " + d->mUser);
}

/*
 * Watch user and get an updated board
 */
void KFibsPlayerList::slotWatch()
{
    emit fibsCommand("watch " + d->mUser);
    emit fibsCommand("board");
}

/*
 * Request information about the selected user
 */
void KFibsPlayerList::getPlayerInfo(QListViewItem *i, const QPoint &, int col)
{
    int num = cIndex(col);
    if (col < 0 || num < 0 || num > 2 || i->text(num).isEmpty())
        num = 0;
    emit fibsCommand("whois " + i->text(num));
}

/*
 * Invite the selected user.
 */
void KFibsPlayerList::slotInviteD()
{
    emit fibsInvite(d->mUser);
}
void KFibsPlayerList::slotInvite1() { emit fibsCommand("invite " + d->mUser + " 1"); }
void KFibsPlayerList::slotInvite2() { emit fibsCommand("invite " + d->mUser + " 2"); }
void KFibsPlayerList::slotInvite3() { emit fibsCommand("invite " + d->mUser + " 3"); }
void KFibsPlayerList::slotInvite4() { emit fibsCommand("invite " + d->mUser + " 4"); }
void KFibsPlayerList::slotInvite5() { emit fibsCommand("invite " + d->mUser + " 5"); }
void KFibsPlayerList::slotInvite6() { emit fibsCommand("invite " + d->mUser + " 6"); }
void KFibsPlayerList::slotInvite7() { emit fibsCommand("invite " + d->mUser + " 7"); }

void KFibsPlayerList::slotInviteU() { emit fibsCommand("invite " + d->mUser + " unlimited"); }
void KFibsPlayerList::slotInviteR() { emit fibsCommand("invite " + d->mUser); }


// == inserting and updating the list ==========================================

/*
 * Add or change the entry of player with the corresponding string
 * from the server - rawwho
 */
void KFibsPlayerList::changePlayer(const QString &line)
{
    char entry[LVEnd][100];
    char ready[2], away[2];
    QListViewItem *i;
    QDateTime fromEpoch;
    QString str_entry[LVEnd], tmp;

    entry[Status][0] = '\0';

    // the line comes from FIBS and is 7 bit ASCII
    sscanf(line.latin1(), "%99s %99s %99s %1s %1s %99s %99s %99s %99s %99s %99s %99s", entry[Player], entry[Opponent],
           entry[Watches], ready, away, entry[Rating], entry[Experience], entry[Idle], entry[Time],
           entry[Host], entry[Client], entry[Email]);

    // convert time
    tmp = entry[Time];
    fromEpoch.setTime_t(tmp.toUInt());
    strcpy(entry[Time], fromEpoch.toString().latin1());

    // clear empty strings and copy
    for (int j = 0; j < LVEnd; j++) {
        if ((str_entry[j] = entry[j]) == "-")
            str_entry[j] = "";
    }
    str_entry[Status].replace(Ready, 1, ready[0] == '0' ? "-" : d->mAbrv[Ready]);
    str_entry[Status].replace(Away,  1, away [0] == '0' ? "-" : d->mAbrv[Away ]);
    str_entry[Status].replace(Blind, 1, "-");

    // disable drawing until the end of update
    setUpdatesEnabled(false);

    // try to find the item in the list
    QListViewItemIterator it(this);
    for ( ; it.current(); ++it) {
        if (it.current()->text(0) == str_entry[Player]) {
            i = it.current();
            goto found;
        }
    }

    // getting here means we have to create a new entry
    i = new KFibsPlayerListLVI(this);

    // count the KFibs and KBackgammon clients
    if (str_entry[Client].contains("KFibs"))
        d->mCount[0]++;
    else if (str_entry[Client].contains(PROG_NAME))
        d->mCount[1]++;

    // new entry requires an update to the player count
    updateCaption();

    goto update;

 found:

    // getting here means the player is in the list - update private status
    str_entry[Status].replace(Blind,1,i->text(Status).contains
                              (d->mAbrv[Blind]) ? d->mAbrv[Blind] : "-");

 update:

    for (int j = 0; j < LVEnd; j++) {
        if (d->mCol[j]->show)
            i->setText(d->mCol[j]->index, str_entry[j]);
    }

    // find out if we are watching somebody
    if (d->mName == str_entry[Player])
        d->mWatch = !str_entry[Watches].isEmpty();
}

/*
 * Remove player from the list
 */
void KFibsPlayerList::deletePlayer(const QString &player)
{
    QListViewItemIterator it(this);
    for ( ; it.current(); ++it) {
        if (it.current()->text(0) == player) {
            if (it.current()->text(Client).contains(PROG_NAME))
                --d->mCount[1];
            else if (it.current()->text(Client).contains("KFibs"))
                --d->mCount[0];
            delete it.current();
            updateCaption();
            return;
        }
    }
}

/*
 * Set/Unset the status stat in the corresponding column of the list
 */
void KFibsPlayerList::changePlayerStatus(const QString &player, int stat, bool flag)
{
    QListViewItem *i = 0;

    /*
     * Find the correct line
     */
    QListViewItemIterator it(this);
    for ( ; it.current(); ++it) {
        if (it.current()->text(Player) == player) {
            i = it.current();
            break;
        }
    }
    if (!i)	return;

    /*
     * Update the status flag
     */
    i->setText(Status, i->text(Status).replace(stat, 1, (flag) ? d->mAbrv[stat] : "-"));
}


// == various slots and functions ==============================================

/*
 * Reverse column to index mapping. Return negative on error.
 */
int KFibsPlayerList::cIndex(int col)
{
    for (int i = 0; i < LVEnd; i++)
        if (d->mCol[i]->index == col)
            return i;
    return -1;
}

/*
 * Catch hide events, so the engine's menu can be update.
 */
void KFibsPlayerList::showEvent(QShowEvent *e)
{
    KListView::showEvent(e);
    emit windowVisible(true);
}

/*
 * Catch hide events, so the engine's menu can be update.
 */
void KFibsPlayerList::hideEvent(QHideEvent *e)
{
    emit windowVisible(false);
    KListView::hideEvent(e);
}

/*
 * Called at the end of updates to re-enable the UI
 */
void KFibsPlayerList::stopUpdate()
{
    setUpdatesEnabled(true);
    triggerUpdate();
}

/*
 * Knowing our own name allows us to disable certain menu entries for
 * ourselves.
 */
void KFibsPlayerList::setName(const QString &name)
{
    d->mName = name;
}

/*
 * Update the caption of the list by including the current client
 * count
 */
void KFibsPlayerList::updateCaption()
{
    setCaption(i18n("Player List - %1 - %2/%3").arg(childCount()).arg(d->mCount[0]).arg(d->mCount[1]));
}

/*
 * Clear the list and reset the client counters
 */
void KFibsPlayerList::clear()
{
    d->mCount[0] = 0;
    d->mCount[1] = 0;
    QListView::clear();
}

// EOF
