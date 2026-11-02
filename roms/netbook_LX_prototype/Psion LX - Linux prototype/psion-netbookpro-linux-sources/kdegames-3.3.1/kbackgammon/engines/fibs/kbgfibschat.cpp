
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

  $Id: kbgfibschat.cpp,v 1.26 2004/06/26 07:05:36 binner Exp $

*/


#include "kbgfibschat.h"
#include "kbgfibschat.moc"

#include <qstring.h>

#include <qlayout.h>
#include <qlabel.h>
#include <qpopupmenu.h>
#include <qregexp.h>
#include <qfont.h>
#include <qwhatsthis.h>
#include <qdatetime.h>
#include <qclipboard.h>
#include <qsimplerichtext.h>
#include <qregion.h>
#include <qpalette.h>
#include <qpainter.h>
#include <qpoint.h>
#include <qlistbox.h>
#include <qiconset.h>
#include <qstringlist.h>
#include <qdict.h>

#include <klocale.h>
#include <kconfig.h>
#include <kapplication.h>
#include <kdebug.h>
#include <kstdaction.h>
#include <ktabctl.h>
#include <kaction.h>
#include <kiconloader.h>

#include "clip.h"
#include "version.h"


/*
 * Private utility class that might become more generally useful in
 * the future. Basically, it implements rich text QListBox items.
 */
class KLBT : public QListBoxText
{

public:

    /*
     * Constructor
     */
    KLBT(QWidget *parent, const QString &text = QString::null, const QString &player = QString::null)
        : QListBoxText(text)
    {
        w = parent;
        n = new QString(player);
        t = new QSimpleRichText(text, w->font());

        // FIXME: this is not yet perfect
        t->setWidth(w->width()-20);
    }

    /*
     * Destructor
     */
    virtual ~KLBT()
    {
        delete t;
        delete n;
    }

    /*
     * Overloaded required members returning height
     */
    virtual int height(const QListBox *) const
    {
        return (1+t->height());
    }

    /*
     * Overloaded required members returning width
     */
    virtual int width(const QListBox *) const
    {
        return t->width();
    }

    /*
     * The context menu needs the name of the player. It's easier
     * than extracting it from the text.
     */
    QString player() const
    {
        return *n;
    }

protected:

    /*
     * Required overloaded member to paint the text on the painter p.
     */
    virtual void paint(QPainter *p)
    {
        t->draw(p, 1, 1, QRegion(p->viewport()), w->colorGroup());
    }

private:

    QSimpleRichText *t;
    QWidget *w;
    QString *n;

};


class KBgChatPrivate
{
public:

    /*
     * Name of the users
     */
    QString mName[2];

    /*
     * Hold and assemble info text
     */
    QString mText;

    /*
     * Numbers of the private action list.
     */
    enum Privact {Inquire, InviteD, Invite1, Invite2, Invite3, Invite4,
                  Invite5, Invite6, Invite7, InviteR, InviteU, Silent,
                  Talk, Gag, Ungag, Cleargag, Copy, Clear, Close, MaxAction};

    /*
     * Available actions
     */
    KAction *mAct[MaxAction];

    /*
     * Context menu and invitation menu
     */
    QPopupMenu *mChat, *mInvt;

    /*
     * list of users we do not want to hear shouting
     */
    QStringList mGag;

    /*
     * Listbox needed by the setup dialog
     */
    QListBox *mLb;

    /*
     * Internal ID to name mapping
     */
    QDict<int> *mName2ID;

};


// == constructor, destructor ==================================================

/*
 * Constructor of the chat window.
 */
KBgChat::KBgChat(QWidget *parent, const char *name)
    : KChat(parent, false)
{
    d = new KBgChatPrivate();
    KActionCollection* actions = new KActionCollection(this);

    d->mName[0] = QString::null;
    d->mChat = 0;
    d->mInvt = new QPopupMenu();

    setAutoAddMessages(false); // we get an echo from FIBS
    setFromNickname(i18n("%1 user").arg(PROG_NAME));

    if (!addSendingEntry(i18n("Kibitz to watchers and players"), CLIP_YOU_KIBITZ))
        kdDebug(10500) << "adding kibitz" << endl;
    if (!addSendingEntry(i18n("Whisper to watchers only"), CLIP_YOU_WHISPER))
        kdDebug(10500) << "adding whisper" << endl;

    connect(this, SIGNAL(rightButtonClicked(QListBoxItem *, const QPoint &)),
            this, SLOT(contextMenu(QListBoxItem *, const QPoint &)));
    connect(this, SIGNAL(signalSendMessage(int, const QString &)),
            this, SLOT(handleCommand(int, const QString &)));

    d->mName2ID = new QDict<int>(17, true);
    d->mName2ID->setAutoDelete(true);

    /*
     * some eye candy :)
     */
    setIcon(kapp->miniIcon());
    setCaption(i18n("Chat Window"));

    QWhatsThis::add(this, i18n("This is the chat window.\n\n"
                               "The text in this window is colored depending on whether "
                               "it is directed at you personally, shouted to the general "
                               "FIBS population, has been said by you, or is of general "
                               "interest. If you select the name of a player, the context "
                               "contains entries specifically geared towards that player."));
    /*
     * Define set of available actions
     */
    d->mAct[KBgChatPrivate::Inquire] = new KAction(i18n("Info On"),
                                                   QIconSet(kapp->iconLoader()->loadIcon(
                                                                "help.xpm", KIcon::Small)),
                                                   0, this, SLOT(slotInquire()), actions);
    d->mAct[KBgChatPrivate::Talk]    = new KAction(i18n("Talk To"),
                                                   QIconSet(kapp->iconLoader()->loadIcon(
                                                                PROG_NAME "-chat.png", KIcon::Small)),
                                                   0, this, SLOT(slotTalk()), actions);

    d->mAct[KBgChatPrivate::InviteD]  = new KAction(i18n("Use Dialog"),    0, this,
                                                    SLOT(slotInviteD()),  actions);
    d->mAct[KBgChatPrivate::Invite1]  = new KAction(i18n("1 Point Match"), 0, this,
                                                    SLOT(slotInvite1()),  actions);
    d->mAct[KBgChatPrivate::Invite2]  = new KAction(i18n("2 Point Match"), 0, this,
                                                    SLOT(slotInvite2()),  actions);
    d->mAct[KBgChatPrivate::Invite3]  = new KAction(i18n("3 Point Match"), 0, this,
                                                    SLOT(slotInvite3()),  actions);
    d->mAct[KBgChatPrivate::Invite4]  = new KAction(i18n("4 Point Match"), 0, this,
                                                    SLOT(slotInvite4()),  actions);
    d->mAct[KBgChatPrivate::Invite5]  = new KAction(i18n("5 Point Match"), 0, this,
                                                    SLOT(slotInvite5()),  actions);
    d->mAct[KBgChatPrivate::Invite6]  = new KAction(i18n("6 Point Match"), 0, this,
                                                    SLOT(slotInvite6()),  actions);
    d->mAct[KBgChatPrivate::Invite7]  = new KAction(i18n("7 Point Match"), 0, this,
                                                    SLOT(slotInvite7()),  actions);
    d->mAct[KBgChatPrivate::InviteU]  = new KAction(i18n("Unlimited"),     0, this,
                                                    SLOT(slotInviteU()),  actions);
    d->mAct[KBgChatPrivate::InviteR]  = new KAction(i18n("Resume"),        0, this,
                                                    SLOT(slotInviteR()),  actions);

    d->mAct[KBgChatPrivate::InviteD]->plug(d->mInvt);

    d->mInvt->insertSeparator();

    d->mAct[KBgChatPrivate::Invite1]->plug(d->mInvt);
    d->mAct[KBgChatPrivate::Invite2]->plug(d->mInvt);
    d->mAct[KBgChatPrivate::Invite3]->plug(d->mInvt);
    d->mAct[KBgChatPrivate::Invite4]->plug(d->mInvt);
    d->mAct[KBgChatPrivate::Invite5]->plug(d->mInvt);
    d->mAct[KBgChatPrivate::Invite6]->plug(d->mInvt);
    d->mAct[KBgChatPrivate::Invite7]->plug(d->mInvt);

    d->mInvt->insertSeparator();

    d->mAct[KBgChatPrivate::InviteU]->plug(d->mInvt);
    d->mAct[KBgChatPrivate::InviteR]->plug(d->mInvt);

    d->mAct[KBgChatPrivate::Gag]      = new KAction(i18n("Gag"),   0, this, SLOT(slotGag()),     actions);
    d->mAct[KBgChatPrivate::Ungag]    = new KAction(i18n("Ungag"), 0, this, SLOT(slotUngag()),   actions);
    d->mAct[KBgChatPrivate::Cleargag] = new KAction(i18n("Clear Gag List"), 0, this, SLOT(slotCleargag()), actions);
    d->mAct[KBgChatPrivate::Copy]     = KStdAction::copy(this, SLOT(slotCopy()), actions);
    d->mAct[KBgChatPrivate::Clear]    = new KAction(i18n("Clear"),        0, this, SLOT(slotClear()), actions);
    d->mAct[KBgChatPrivate::Close]    = KStdAction::close(this, SLOT(hide()), actions);
    d->mAct[KBgChatPrivate::Silent]   = new KToggleAction(i18n("Silent"), 0, this, SLOT(slotSilent()), actions);
}


/*
 * Destructor
 */
KBgChat::~KBgChat()
{
    delete d->mName2ID;
    delete d->mChat; // save to delete NULL pointers
    delete d->mInvt;
    delete d;
}


// == configuration handling ===================================================

/*
 * Restore the previously stored settings
 */
void KBgChat::readConfig()
{
    KConfig* config = kapp->config();
    config->setGroup("chat window");

    QPoint pos(10, 10);

    pos = config->readPointEntry("ori", &pos);
    setGeometry(pos.x(), pos.y(), config->readNumEntry("wdt",460), config->readNumEntry("hgt",200));

    config->readBoolEntry("vis", false) ? show() : hide();

    ((KToggleAction *)d->mAct[KBgChatPrivate::Silent])->setChecked(config->readBoolEntry("sil", false));

    d->mGag = config->readListEntry("gag");
}

/*
 * Save the current settings to disk
 */
void KBgChat::saveConfig()
{
    KConfig* config = kapp->config();
    config->setGroup("chat window");

    config->writeEntry("ori", pos());
    config->writeEntry("hgt", height());
    config->writeEntry("wdt", width());
    config->writeEntry("vis", isVisible());

    config->writeEntry("sil", ((KToggleAction *)d->mAct[KBgChatPrivate::Silent])->isChecked());

    config->writeEntry("gag", d->mGag);
}


/*
 * Setup dialog page of the player list - allow the user to select the
 * columns to show
 *
 * FIXME: need to be able to set font here KChatBase::setBothFont(const QFont& font)
 */
void KBgChat::getSetupPages(KTabCtl *nb, int space)
{
    /*
     * Main Widget
     * ===========
     */
    QWidget *w = new QWidget(nb);
    QGridLayout *gl = new QGridLayout(w, 2, 1, space);

    d->mLb = new QListBox(w);
    d->mLb->setMultiSelection(true);

    d->mLb->insertStringList(d->mGag);

    QLabel *info = new QLabel(w);
    info->setText(i18n("Select users to be removed from the gag list."));

    QWhatsThis::add(w, i18n("Select all the users you want "
                            "to remove from the gag list "
                            "and then click OK. Afterwards "
                            "you will again hear what they shout."));

    gl->addWidget(d->mLb, 0, 0);
    gl->addWidget(info, 1, 0);

    /*
     * put in the page
     * ===============
     */
    gl->activate();
    w->adjustSize();
    w->setMinimumSize(w->size());
    nb->addTab(w, i18n("&Gag List"));
}

/*
 * Remove all the selected entries from the gag list
 */
void KBgChat::setupOk()
{
    for (uint i = 0; i < d->mLb->count(); ++i) {
        if (d->mLb->isSelected(i))
            d->mGag.remove(d->mLb->text(i));
    }
    d->mLb->clear();
    d->mLb->insertStringList(d->mGag);
}

/*
 * Don't do anything
 */
void KBgChat::setupCancel()
{
    // empty
}

/*
 * By default, all players stay in the gag list
 */
void KBgChat::setupDefault()
{
    d->mLb->clearSelection();
}


// == various slots and functions ==============================================

/*
 * Overloaded member to create a QListBoxItem for the chat window.
 */
QListBoxItem* KBgChat::layoutMessage(const QString& fromName, const QString& text)
{
    QListBoxText* message = new KLBT(this, text, fromName);
    return message;
}

/*
 * Catch hide events, so the engine's menu can be update.
 */
void KBgChat::showEvent(QShowEvent *e)
{
    QFrame::showEvent(e);
    emit windowVisible(true);
}

/*
 * Catch hide events, so the engine's menu can be update.
 */
void KBgChat::hideEvent(QHideEvent *e)
{
    emit windowVisible(false);
    QFrame::hideEvent(e);
}

/*
 * At the beginning of a game, add the name to the list and switch to
 * kibitz mode.
 */
void KBgChat::startGame(const QString &name)
{
    int *id = d->mName2ID->find(d->mName[1] = name);
    if (!id) {
        id = new int(nextId());
        d->mName2ID->insert(name, id);
        addSendingEntry(i18n("Talk to %1").arg(name), *id);
    }
    setSendingEntry(CLIP_YOU_KIBITZ);
}

/*
 * At the end of a game, we switch to talk mode.
 */
void KBgChat::endGame()
{
    int *id = d->mName2ID->find(d->mName[1]);
    if (id)
        setSendingEntry(*id);
    else
        setSendingEntry(SendToAll);
}

/*
 * Set the chat window ready to talk to name
 */
void KBgChat::fibsTalk(const QString &name)
{
    int *id = d->mName2ID->find(name);
    if (!id) {
        id = new int(nextId());
        d->mName2ID->insert(name, id);
        addSendingEntry(i18n("Talk to %1").arg(name), *id);
    }
    setSendingEntry(*id);
}

/*
 * Remove the player from the combo box when he/she logs out.
 */
void KBgChat::deletePlayer(const QString &name)
{
    int *id = d->mName2ID->find(name);
    if (id) {
        removeSendingEntry(*id);
        d->mName2ID->remove(name);
    }
}

/*
 * Take action when the user presses return in the line edit control.
 */
void KBgChat::handleCommand(int id, const QString& msg)
{
    int realID = sendingEntry();

    switch (realID) {
    case SendToAll:
        emit fibsCommand("shout " + msg);
        break;
    case CLIP_YOU_KIBITZ:
        emit fibsCommand("kibitz " + msg);
        break;
    case CLIP_YOU_WHISPER:
        emit fibsCommand("whisper " + msg);
        break;
    default:
        QDictIterator<int> it(*d->mName2ID);
        while (it.current()) {
            if (*it.current() == realID) {
                emit fibsCommand("tell " + it.currentKey() + " " + msg);
                return;
            }
            ++it;
        }
        kdDebug(10500) << "unrecognized ID in KBgChat::handleCommand" << endl;
    }
}


// == handle strings from the server ===========================================

/*
 * A message from the server that should be handled by us. If necessary,
 * we replace the CLIP number by a string and put the line into the window.
 *
 * This function emits the string in rich text format with the signal
 * personalMessage - again: the string contains rich text!
 */
void KBgChat::handleData(const QString &msg)
{
    QString clip = msg.left(msg.find(' ')), user, cMsg = msg;
    QDateTime date;

    bool flag = false;
    int cmd = clip.toInt(&flag);

    if (flag) {
        cMsg.replace(0, cMsg.find(' ')+1, "");

        user = cMsg.left(cMsg.find(' '));

        switch (cmd) {
        case CLIP_SAYS:
            if (!d->mGag.contains(user)) {
                cMsg = i18n("<u>%1 tells you:</u> %2").arg(user).arg(cMsg.replace(QRegExp("^" + user), ""));
                cMsg = "<font color=\"red\">" + cMsg + "</font>";
                emit personalMessage(cMsg);
            } else
                cMsg = "";
            break;

        case CLIP_SHOUTS:
            if ((!((KToggleAction *)d->mAct[KBgChatPrivate::Silent])->isChecked()) && (!d->mGag.contains(user))) {
                cMsg = i18n("<u>%1 shouts:</u> %2").arg(user).arg(cMsg.replace(QRegExp("^" + user), ""));
                cMsg = "<font color=\"black\">" + cMsg + "</font>";
            } else
                cMsg = "";
            break;

        case CLIP_WHISPERS:
            if (!d->mGag.contains(user)) {
                cMsg = i18n("<u>%1 whispers:</u> %2").arg(user).arg(cMsg.replace(QRegExp("^" + user), ""));
                cMsg = "<font color=\"red\">" + cMsg + "</font>";
                emit personalMessage(cMsg);
            } else
                cMsg = "";
            break;

        case CLIP_KIBITZES:
            if (!d->mGag.contains(user)) {
                cMsg = i18n("<u>%1 kibitzes:</u> %2").arg(user).arg(cMsg.replace(QRegExp("^" + user), ""));
                cMsg = "<font color=\"red\">" + cMsg + "</font>";
                emit personalMessage(cMsg);
            } else
                cMsg = "";
            break;

        case CLIP_YOU_SAY:
            cMsg = i18n("<u>You tell %1:</u> %2").arg(user).arg(cMsg.replace(QRegExp("^" + user), ""));
            cMsg = "<font color=\"darkgreen\">" + cMsg + "</font>";
            emit personalMessage(cMsg);
            user = QString::null;
            break;

        case CLIP_YOU_SHOUT:
            cMsg = i18n("<u>You shout:</u> %1").arg(cMsg);
            cMsg = "<font color=\"darkgreen\">" + cMsg + "</font>";
            emit personalMessage(cMsg);
            user = QString::null;
            break;

        case CLIP_YOU_WHISPER:
            cMsg = i18n("<u>You whisper:</u> %1").arg(cMsg);
            cMsg = "<font color=\"darkgreen\">" + cMsg + "</font>";
            emit personalMessage(cMsg);
            user = QString::null;
            break;

        case CLIP_YOU_KIBITZ:
            cMsg = i18n("<u>You kibitz:</u> %1").arg(cMsg);
            cMsg = "<font color=\"darkgreen\">" + cMsg + "</font>";
            emit personalMessage(cMsg);
            user = QString::null;
            break;

        case CLIP_MESSAGE:
            user = cMsg.left(cMsg.find(' ')+1);
            cMsg.remove(0, cMsg.find(' ')+1);
            date.setTime_t(cMsg.left(cMsg.find(' ')+1).toUInt());
            cMsg.remove(0, cMsg.find(' '));
            cMsg = i18n("<u>User %1 left a message at %2</u>: %3").arg(user).arg(date.toString()).arg(cMsg);
            cMsg = "<font color=\"red\">" + cMsg + "</font>";
            emit personalMessage(cMsg);
            user = QString::null;
            break;

        case CLIP_MESSAGE_DELIVERED:
            cMsg = i18n("Your message for %1 has been delivered.").arg(user);
            cMsg = QString("<font color=\"darkgreen\">") + cMsg + "</font>";
            emit personalMessage(cMsg);
            user = QString::null;
            break;

        case CLIP_MESSAGE_SAVED:
            cMsg = i18n("Your message for %1 has been saved.").arg(user);
            cMsg = QString("<font color=\"darkgreen\">") + cMsg + "</font>";
            emit personalMessage(cMsg);
            user = QString::null;
            break;

        default: // ignore the message
            return;
        }

    } else {

        /*
         * Special treatment for non-CLIP messages
         */
        if (cMsg.contains(QRegExp("^You say to yourself: "))) {
            cMsg.replace(QRegExp("^You say to yourself: "),
                         i18n("<u>You say to yourself:</u> "));
        } else {
            kdDebug(user.isNull(), 10500) << "KBgChat::handleData unhandled message: "
                                                  << cMsg.latin1() << endl;
            return;
        }
    }

    if (!cMsg.isEmpty())
        addMessage(user, cMsg);
}


// == context menu and related slots ===========================================

/*
 * RMB opens a context menu.
 */
void KBgChat::contextMenu(QListBoxItem *i, const QPoint &p)
{
    /*
     * Even if i is non-null, user might still be QString::null
     */
    d->mName[0] = (i == 0) ? QString::null : ((KLBT *)i)->player();
    d->mText = (i == 0) ? QString::null : ((KLBT *)i)->text();

    /*
     * Get a new context menu every time. Safe to delete the 0
     * pointer.
     */
    delete d->mChat; d->mChat = new QPopupMenu();

    /*
     * Fill the context menu with actions
     */
    if (!d->mName[0].isNull()) {

        d->mAct[KBgChatPrivate::Talk]->setText(i18n("Talk to %1").arg(d->mName[0]));
        d->mAct[KBgChatPrivate::Talk]->plug(d->mChat);

        d->mAct[KBgChatPrivate::Inquire]->setText(i18n("Info on %1").arg(d->mName[0]));
        d->mAct[KBgChatPrivate::Inquire]->plug(d->mChat);

        // invite menu is always the same
        d->mChat->insertItem(i18n("Invite %1").arg(d->mName[0]), d->mInvt);

        d->mChat->insertSeparator();

        if (d->mGag.contains(d->mName[0]) <= 0) {
            d->mAct[KBgChatPrivate::Gag]->setText(i18n("Gag %1").arg(d->mName[0]));
            d->mAct[KBgChatPrivate::Gag]->plug(d->mChat);
        } else {
            d->mAct[KBgChatPrivate::Ungag]->setText(i18n("Ungag %1").arg(d->mName[0]));
            d->mAct[KBgChatPrivate::Ungag]->plug(d->mChat);
        }
    }
    if (d->mGag.count() > 0)
        d->mAct[KBgChatPrivate::Cleargag]->plug(d->mChat);

    if ((d->mGag.count() > 0) || (!d->mName[0].isNull()))
        d->mChat->insertSeparator();

    d->mAct[KBgChatPrivate::Silent]->plug(d->mChat);

    d->mChat->insertSeparator();

    d->mAct[KBgChatPrivate::Copy ]->plug(d->mChat);
    d->mAct[KBgChatPrivate::Clear]->plug(d->mChat);
    d->mAct[KBgChatPrivate::Close]->plug(d->mChat);

    d->mChat->popup(p);
}

/*
 * Clear the gag list
 */
void KBgChat::slotCleargag()
{
    d->mGag.clear();

    QString msg("<font color=\"blue\">");
    msg += i18n("The gag list is now empty.");
    msg += "</font>";

    addMessage(QString::null, msg);
}

/*
 * Gag the selected user
 */
void KBgChat::slotGag()
{
    d->mGag.append(d->mName[0]);

    QString msg("<font color=\"blue\">");
    msg += i18n("You won't hear what %1 says and shouts.").arg(d->mName[0]);
    msg += "</font>";

    addMessage(QString::null, msg);
}

/*
 * Simple interface to the actual talk slot
 */
void KBgChat::slotTalk()
{
    fibsTalk(d->mName[0]);
}

/*
 * Remove selected user from gag list
 */
void KBgChat::slotUngag()
{
    d->mGag.remove(d->mName[0]);

    QString msg("<font color=\"blue\">");
    msg += i18n("You will again hear what %1 says and shouts.").arg(d->mName[0]);
    msg += "</font>";

    addMessage(QString::null, msg);
}

/*
 * Get information on selected user
 */
void KBgChat::slotInquire()
{
    kdDebug(d->mName[0].isNull(), 10500) << "KBgChat::slotInquire: user == null" << endl;
    emit fibsCommand("whois " + d->mName[0]);
}

/*
 * Block all shouts from the chat window
 */
void KBgChat::slotSilent()
{
    QString msg;
    if (((KToggleAction *)d->mAct[KBgChatPrivate::Silent])->isChecked())
        msg = "<font color=\"blue\">" + i18n("You will not hear what people shout.") + "</font>";
    else
        msg = "<font color=\"blue\">" + i18n("You will hear what people shout.") + "</font>";
    addMessage(QString::null, msg);
}

/*
 * Copy the selected line to the clipboard. Strip the additional HTML
 * from the text before copying.
 */
void KBgChat::slotCopy()
{
    d->mText.replace(QRegExp("<u>"), "");
    d->mText.replace(QRegExp("</u>"), "");
    d->mText.replace(QRegExp("</font>"), "");
    d->mText.replace(QRegExp("^.*\">"), "");

    kapp->clipboard()->setText(d->mText);
}

/*
 * Invite the selected player.
 */
void KBgChat::slotInviteD()
{
    kdDebug(d->mName[0].isNull(), 10500) << "KBgChat::slotInvite: user == null" << endl;
    emit fibsRequestInvitation(d->mName[0]);
}
void KBgChat::slotInvite1() { emit fibsCommand("invite " + d->mName[0] + " 1"); }
void KBgChat::slotInvite2() { emit fibsCommand("invite " + d->mName[0] + " 2"); }
void KBgChat::slotInvite3() { emit fibsCommand("invite " + d->mName[0] + " 3"); }
void KBgChat::slotInvite4() { emit fibsCommand("invite " + d->mName[0] + " 4"); }
void KBgChat::slotInvite5() { emit fibsCommand("invite " + d->mName[0] + " 5"); }
void KBgChat::slotInvite6() { emit fibsCommand("invite " + d->mName[0] + " 6"); }
void KBgChat::slotInvite7() { emit fibsCommand("invite " + d->mName[0] + " 7"); }

void KBgChat::slotInviteU() { emit fibsCommand("invite " + d->mName[0] + " unlimited"); }
void KBgChat::slotInviteR() { emit fibsCommand("invite " + d->mName[0]); }


// EOF
