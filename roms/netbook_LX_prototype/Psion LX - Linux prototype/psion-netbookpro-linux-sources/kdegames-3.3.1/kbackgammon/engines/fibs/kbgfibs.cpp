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

  $Id: kbgfibs.cpp,v 1.23 2004/05/15 14:05:37 binner Exp $

*/

/*

  TODO: popup dialog for accept/reject and join ??
        clear the chat history?
	game over, clear the caption?
	need show saved
	need buddy list
	need wait for player,...

*/

#include "kbgfibs.h"
#include "kbgfibs.moc"

#include <kapplication.h>
#include <kconfig.h>
#include <qtimer.h>
#include <qcheckbox.h>
#include <qlayout.h>
#include <qstring.h>
#include <qsocket.h>
#include <qpopupmenu.h>
#include <qgroupbox.h>
#include <qbuttongroup.h>
#include <qradiobutton.h>
#include <kmainwindow.h>
#include <klineeditdlg.h>
#include <kmessagebox.h>
#include <qdatetime.h>
#include <qwhatsthis.h>
#include <kaudioplayer.h>
#include <kstandarddirs.h>
#include <qvbox.h>
#include <kiconloader.h>
#include <ktabctl.h>
#include <kpassdlg.h>
#include <qcstring.h>
#include <knotifyclient.h>
#include <kaction.h>


#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <iostream>
#include "kbgboard.h"
#include "kbgstatus.h"

#include "clip.h"
#include "version.h"


void KBgEngineFIBS::start()
{
	// FIXME: open the child windows here and not in the constructor
}

// == configuration handling ===================================================

/*
 * Restore settings and ask children to do the same
 */
void KBgEngineFIBS::readConfig()
{
	KConfig *config = kapp->config();
	config->setGroup("fibs engine");

	// history variables
	lastAway = config->readEntry("away_hist", "");

	// various options
	showMsg  = config->readBoolEntry("pers_msg", false);
	whoisInvite = config->readBoolEntry("whois_invite", false);

	// connection information
	infoFIBS[FIBSHost] = config->readEntry("server", "fibs.com");
	infoFIBS[FIBSPort] = config->readEntry("port", "4321");
	infoFIBS[FIBSUser] = config->readEntry("user", "");
	infoFIBS[FIBSPswd] = config->readEntry("password", "");

	// automatic messages
	useAutoMsg[MsgBeg] = config->readBoolEntry("auto-beg", false);
	useAutoMsg[MsgLos] = config->readBoolEntry("auto-los", false);
	useAutoMsg[MsgWin] = config->readBoolEntry("auto-win", false);

	autoMsg[MsgBeg] = config->readEntry("msg-beg", "");
	autoMsg[MsgLos] = config->readEntry("msg-los", "");
	autoMsg[MsgWin] = config->readEntry("msg-win", "");

	// ask the children to read their config options
	playerlist->readConfig();
	chatWindow->readConfig();
}

/*
 * Save the engine specific settings and tell all clients
 */
void KBgEngineFIBS::saveConfig()
{
	KConfig *config = kapp->config();
	config->setGroup("fibs engine");

	// history variables
	config->writeEntry("away_hist", lastAway);

	// various options
	config->writeEntry("pers_msg", showMsg);
	config->writeEntry("whois_invite", whoisInvite);

	// connection information
	config->writeEntry("server",   infoFIBS[FIBSHost]);
	config->writeEntry("port",     infoFIBS[FIBSPort]);
	config->writeEntry("user",     infoFIBS[FIBSUser]);
	config->writeEntry("password", infoFIBS[FIBSPswd]);

	// automatic messages
	config->writeEntry("auto-beg", useAutoMsg[MsgBeg]);
	config->writeEntry("auto-los", useAutoMsg[MsgLos]);
	config->writeEntry("auto-win", useAutoMsg[MsgWin]);

	config->writeEntry("msg-beg", autoMsg[MsgBeg]);
	config->writeEntry("msg-los", autoMsg[MsgLos]);
	config->writeEntry("msg-win", autoMsg[MsgWin]);

	// ask the children to read their config options
	playerlist->saveConfig();
	chatWindow->saveConfig();
}

void KBgEngineFIBS::setupDefault()
{

	cbp->setChecked(false);
	cbi->setChecked(false);

	lec[FIBSHost]->setText("fibs.com");
	lec[FIBSPort]->setText("4321");

	lec[FIBSUser]->clear();
	lec[FIBSPswd]->clear();


	chatWindow->setupDefault();
	playerlist->setupDefault();
}

void KBgEngineFIBS::setupCancel()
{
	chatWindow->setupCancel();
	playerlist->setupCancel();
}

/*
 * Called when the setup dialog is positively closed
 */
void KBgEngineFIBS::setupOk()
{
	// various options
	showMsg = cbp->isChecked();
	whoisInvite = cbi->isChecked();

	// connection information
	for (int i = 0; i < NumFIBS; i++)
		infoFIBS[i] = lec[i]->text();

	// automatic messages
	for (int i = 0; i < NumMsg; i++) {
		useAutoMsg[i] = cbm[i]->isChecked();
		autoMsg[i] = lem[i]->text();
	}

	chatWindow->setupOk();
	playerlist->setupOk();

	// save settings
	saveConfig();
}

/*
 * Puts the FIBS specific setup into the dialog nb
 */
void KBgEngineFIBS::getSetupPages(KDialogBase *nb)
{
	/*
	 * Main Widget
	 */
	QVBox *vbp = nb->addVBoxPage(i18n("FIBS Engine"), i18n("Here you can configure the FIBS backgammon engine"),
				     kapp->iconLoader()->loadIcon(PROG_NAME "_engine", KIcon::Desktop));

	/*
	 * Get a multi page work space
	 */
	KTabCtl *tc = new KTabCtl(vbp, "fibs tabs");

	/*
	 * FIBS, local options
	 */
	QWidget *w = new QWidget(tc);
	QGridLayout *gl = new QGridLayout(w, 3, 1, nb->spacingHint());

	/*
	 * Group boxes
	 */
	QGroupBox *gbo = new QGroupBox(i18n("Options"), w);
	QGroupBox *gbm = new QGroupBox(i18n("Automatic Messages"), w);

	gl->addWidget(gbo, 0, 0);
	gl->addWidget(gbm, 1, 0);

	/*
	 * Options
	 */
	cbp = new QCheckBox(i18n("Show copy of personal messages in main window"), gbo);
	cbi = new QCheckBox(i18n("Automatically request player info on invitation"), gbo);

	QWhatsThis::add(cbp, i18n("Usually, all messages sent directly to you by other players "
				  "are displayed only in the chat window. Check this box if you "
				  "would like to get a copy of these messages in the main window."));
	QWhatsThis::add(cbi, i18n("Check this box if you would like to receive information on "
				  "players that invite you to games."));

	cbp->setChecked(showMsg);
	cbi->setChecked(whoisInvite);

	gl = new QGridLayout(gbo, 2, 1, 20);
	gl->addWidget(cbp, 0, 0);
	gl->addWidget(cbi, 1, 0);

	/*
	 * Automatic messages
	 */
	gl = new QGridLayout(gbm, NumMsg, 2, 20);

	cbm[MsgBeg] = new QCheckBox(i18n("Start match:"), gbm);
	cbm[MsgWin] = new QCheckBox(i18n("Win match:"), gbm);
	cbm[MsgLos] = new QCheckBox(i18n("Lose match:"), gbm);

	QWhatsThis::add(cbm[MsgBeg], i18n("If you want to send a standard greeting to your "
					  "opponent whenever you start a new match, check "
					  "this box and write the message into the entry "
					  "field."));
	QWhatsThis::add(cbm[MsgWin], i18n("If you want to send a standard message to your "
					  "opponent whenever you won a match, check this box "
					  "and write the message into the entry field."));
	QWhatsThis::add(cbm[MsgLos], i18n("If you want to send a standard message to your "
					  "opponent whenever you lost a match, check this box "
					  "and write the message into the entry field."));

	for (int i = 0; i < NumMsg; i++) {
		lem[i] = new QLineEdit(autoMsg[i], gbm);
		gl->addWidget(cbm[i], i, 0);
		gl->addWidget(lem[i], i, 1);
		connect(cbm[i], SIGNAL(toggled(bool)), lem[i], SLOT(setEnabled(bool)));
		cbm[i]->setChecked(useAutoMsg[i]);
		lem[i]->setEnabled(useAutoMsg[i]);
		QWhatsThis::add(lem[i], QWhatsThis::textFor(cbm[i]));
	}

	/*
	 * Put the page into the notebook
	 */
	gl->activate();
	tc->addTab(w, i18n("&Local"));


	/*
	 * FIBS, connection setup
	 */
	w = new QWidget(tc);
	gl = new QGridLayout(w, 3, 1, nb->spacingHint());

	QGroupBox *gbc = new QGroupBox(i18n("Server"), w);
	QGroupBox *gbk = new QGroupBox(i18n("Other"), w);

	gl->addWidget(gbc, 0, 0);
	gl->addWidget(gbk, 1, 0);

	/*
	 * Server box
	 */
	gl = new QGridLayout(gbc, 4, 2, 20);

	QLabel *lbc[NumFIBS];

	lbc[FIBSHost] = new QLabel(i18n("Server name:"), gbc);
	lbc[FIBSPort] = new QLabel(i18n("Server port:"), gbc);
	lbc[FIBSUser] = new QLabel(i18n("User name:"), gbc);
	lbc[FIBSPswd] = new QLabel(i18n("Password:"), gbc);

	for (int i = 0; i < NumFIBS; i++) {
		lec[i] = new QLineEdit(infoFIBS[i], gbc);
		gl->addWidget(lbc[i], i, 0);
		gl->addWidget(lec[i], i, 1);
	}
	lec[FIBSPswd]->setEchoMode(QLineEdit::Password);

	QWhatsThis::add(lec[FIBSHost], i18n("Enter here the host name of FIBS. With almost "
					    "absolute certainty this should be \"fibs.com\". "
					    "If you leave this blank, you will be asked again "
					    "at connection time."));
	QWhatsThis::add(lec[FIBSPort], i18n("Enter here the port number of FIBS. With almost "
					    "absolute certainty this should be \"4321\". "
					    "If you leave this blank, you will be asked again "
					    "at connection time."));
	QWhatsThis::add(lec[FIBSUser], i18n("Enter your login on FIBS here. If you do not have a "
					    "login yet, you should first create an account using "
					    "the corresponding menu entry. If you leave this blank, "
					    "you will be asked again at connection time."));
	QWhatsThis::add(lec[FIBSPswd], i18n("Enter your password on FIBS here. If you do not have a "
					    "login yet, you should first create an account using "
					    "the corresponding menu entry. If you leave this blank, "
					    "you will be asked again at connection time. The password "
					    "will not be visible."));

	/*
	 * Connection keepalive
	 */
	cbk = new QCheckBox(i18n("Keep connections alive"), gbk);

	QWhatsThis::add(cbk, i18n("Usually, FIBS drops the connection after one hour of inactivity. When "
				  "you check this box, %1 will try to keep the connection alive, even "
				  "if you are not actually playing or chatting. Use this with caution "
				  "if you don't have a flat rate Internet access!").arg(PROG_NAME));

	cbk->setChecked(keepalive);

	gl = new QGridLayout(gbk, 1, 1, nb->spacingHint());
	gl->addWidget(cbk, 0, 0);

	/*
	 * Done with the page, put it in
	 */
	gl->activate();
	tc->addTab(w, i18n("&Connection"));

	/*
	 * Ask children for settings
	 */
	chatWindow->getSetupPages(tc, nb->spacingHint());
	playerlist->getSetupPages(tc, nb->spacingHint());

	/*
	 * TODO: future extensions
	 */
	w = new QWidget(tc);
	tc->addTab(w, i18n("&Buddy List"));
}


// == functions related to the invitation menu =================================

/*
 * Remove a player from the invitation list in the join menu
 */
void KBgEngineFIBS::cancelJoin(const QString &info)
{
	QRegExp patt = QRegExp("^" + info + " ");

	for (int i = 0; i <= numJoin; i++) {
		if (actJoin[i]->text().contains(patt)) {
			// move all entries starting at i+1 up by one...
			for (int j = i; j < numJoin; j++)
				actJoin[j]->setText(actJoin[j+1]->text());
			actJoin[numJoin--]->unplug(joinMenu);
			break;
		}
	}
}

/*
 * Parse the information in info for the purposes of the invitation
 * submenu
 */
void KBgEngineFIBS::changeJoin(const QString &info)
{
	char name_p[100], name_o[100];
	float rate;
	int expi;

	/*
	 * Extract the name of the player, her opponent, rating and experience.
	 * It is okay to use latin1(), since the string is coming from FIBS.
	 */
	sscanf(info.latin1(), "%99s %99s %*s %*s %*s %f %i %*s %*s %*s %*s %*s",
	       name_p, name_o, &rate, &expi);

	QString name = name_p;
	QString oppo = name_o;

	QString rate_s; rate_s.setNum(rate);
	QString expi_s; expi_s.setNum(expi);

	QRegExp patt = QRegExp("^" + name + " ");

	/*
	 * We have essentially two lists of names to check against: the ones
	 * that have invited us and are not yet in the menu and the ones that
	 * are already in the menu.
	 */

	if (numJoin > -1 && oppo != "-")
		cancelJoin(name);

	for (QStringList::Iterator it = invitations.begin(); it != invitations.end(); ++it) {

		if ((*it).contains(patt)) {

			QString text, menu;

			if ((*it).contains(QRegExp(" r$"))) {
				menu = i18n("R means resume", "%1 (R)").arg(name);
				text = i18n("%1 (experience %2, rating %3) wants to resume a saved match with you. "
					    "If you want to play, use the corresponding menu entry to join (or type "
					    "'join %4').").arg(name).arg(expi_s).arg(rate_s).arg(name);
				KNotifyClient::event("invitation", i18n("%1 wants to resume a saved match with you").
						     arg(name));
			} else if ((*it).contains(QRegExp(" u$"))) {
				menu = i18n("U means unlimited", "%1 (U)").arg(name);
				text = i18n("%1 (experience %2, rating %3) wants to play an unlimited match with you. "
					    "If you want to play, use the corresponding menu entry to join (or type "
					    "'join %4').").arg(name).arg(expi_s).arg(rate_s).arg(name);
				KNotifyClient::event("invitation", i18n("%1 has invited you to an unlimited match").
						     arg(name));
			} else {
				QString len = (*it).right((*it).length() - name.length() - 1);
				menu = i18n("If the format of the (U) and (R) strings is changed, it should also be changed here",
					    "%1 (%2)").arg(name).arg(len);
				text = i18n("%1 (experience %2, rating %3) wants to play a %4 point match with you. "
					    "If you want to play, use the corresponding menu entry to join (or type "
					    "'join %5').").arg(name).arg(expi_s).arg(rate_s).arg(len).arg(name);
				KNotifyClient::event("invitation", i18n("%1 has invited you for a %2 point match").
						     arg(name).arg(len));
			}
			emit serverString("rawwho " + name); // this avoids a race
			if (whoisInvite) {
				emit serverString("whois " + name);
				emit infoText("<font color=\"red\">" + text + "</font>");
			} else
				emit infoText("<font color=\"red\">" + text + "</font><br>");

			for (int i = 0; i <=numJoin; i++)
				actJoin[i]->unplug(joinMenu);

			if (++numJoin > 7) numJoin = 7;

			for (int i = numJoin; i > 0; i--)
				actJoin[i]->setText(actJoin[i-1]->text());

			actJoin[0]->setText(menu);

			for (int i = 0; i <= numJoin; i++)
				actJoin[i]->plug(joinMenu);

			invitations.remove(it);
			break;
		}
	}

	/*
	 * If there are entries in the menu, enable it
	 */
	menu->setItemEnabled(joinMenuID, numJoin > -1);
}


// == various slots and functions ==============================================

/*
 * Keep the connection alive.
 */
void KBgEngineFIBS::keepAlive()
{
	emit serverString("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
}

/*
 * Several bookkeeping operations that have to be done at the
 * end of every game. Some of these may or may not be necessary
 * at a particular time, but they don't hurt either.
 */
void KBgEngineFIBS::endGame()
{
	playing = false;

	emit serverString("rawwho " + infoFIBS[FIBSUser]);

	actConti->setEnabled(false);
	actLeave->setEnabled(false);

	actAccept->setEnabled(false);
	actReject->setEnabled(false);

	emit allowCommand(Load, false);
	emit allowCommand(Undo, false);
	emit allowCommand(Done, false);
	emit allowCommand(Cube, false);
	emit allowCommand(Roll, false);
}

/*
 * Toggle visibility of the player list
 */
void KBgEngineFIBS::showList()
{
	playerlist->isVisible() ? playerlist->hide() : playerlist->show();
}

/*
 * Toggle visibility of the chat window
 */
void KBgEngineFIBS::showChat()
{
	chatWindow->isVisible() ? chatWindow->hide() : chatWindow->show();
}

/*
 * Process the last move coming from the board
 */
void KBgEngineFIBS::handleMove(QString *s)
{
	lastMove = *s;
	QString t = lastMove.left(1);
	int moves = t.toInt();

	emit allowCommand(Done, moves == toMove);
	emit allowCommand(Undo, moves > 0);

	/*
	 * Allow undo and possibly start the commit timer
	 */
	redoPossible &= ((moves < toMove) && (undoCounter > 0));
	emit allowCommand(Redo, redoPossible);
	if (moves == toMove && cl >= 0) {
		emit allowMoving(false);
		ct->start(cl, true);
	}
}

/*
 * Done with the move
 */
void KBgEngineFIBS::done()
{
	// prevent the timer from expiring again
	ct->stop();

	// no more moves
	emit allowMoving(false);

	// no more commands until it's our turn
	emit allowCommand(Load, false);
	emit allowCommand(Undo, false);
	emit allowCommand(Done, false);
	emit allowCommand(Cube, false);
	emit allowCommand(Roll, false);

	// Transform the string to FIBS cormat
	lastMove.replace(0, 2, "move ");
	lastMove.replace(pat[PlsChar], "-");

	// sent it to the server
	emit serverString(lastMove);
}

/*
 * Undo the last move
 */
void KBgEngineFIBS::undo()
{
	ct->stop();

	redoPossible = true;
	++undoCounter;

	emit allowMoving(true);
	emit allowCommand(Done, false);
	emit allowCommand(Redo, true);
	emit undoMove();
}

/*
 * Redo the last undone move
 */
void KBgEngineFIBS::redo()
{
	--undoCounter;
	emit redoMove();
}

/*
 * Double the cube - coming from the board
 */
void KBgEngineFIBS::doubleCube(const int w)
{
	if (playing && w == US) cube();
}

/*
 * Roll the dice - coming from the board
 */
void KBgEngineFIBS::rollDice(const int w)
{
	if (playing && w == US) roll();
}

/*
 * This engine passes all commands unmodified to the server
 */
void KBgEngineFIBS::handleCommand(QString const &cmd)
{
	emit serverString(cmd);
}

/*
 * If we have a connection, we don't quit right away
 */
bool KBgEngineFIBS::queryClose()
{
	if (connection->state() == QSocket::Idle)
		return true;

	switch (KMessageBox::warningYesNoCancel((QWidget *)parent(),i18n("Still connected. Log out first?"))) {
	case KMessageBox::Yes :
		disconnectFIBS();
		return true;
	case KMessageBox::No :
		return true;
	default: // cancel
		return false;
	}
}

/*
 * If we have a connection, we don't quit right away
 */
bool KBgEngineFIBS::queryExit()
{
	if( kapp->sessionSaving())
	    return true;
	if (connection->state() != QSocket::Idle)
		disconnectFIBS();
	return true;
}

/*
 * This displays a copy of personal messages in the main window.
 * Normally, these only get displayed in the chat window.
 */
void KBgEngineFIBS::personalMessage(const QString &msg)
{
	if (showMsg)
		emit infoText(msg);
}


// == slots and functions for FIBS commands ====================================

/*
 * Accept the offer
 */
void KBgEngineFIBS::accept()
{
	actAccept->setEnabled(false);
	actReject->setEnabled(false);

	emit serverString("accept");
}

/*
 * Reject the offer
 */
void KBgEngineFIBS::reject()
{
	actAccept->setEnabled(false);
	actReject->setEnabled(false);

	emit serverString("reject");
}

/*
 * Continue a multi game match
 */
void KBgEngineFIBS::match_conti()
{
	actConti->setEnabled(false);
	actLeave->setEnabled(false);

	emit serverString("join");
}

/*
 * Leave a multi game match
 */
void KBgEngineFIBS::match_leave()
{
	actConti->setEnabled(false);
	actLeave->setEnabled(false);

	emit serverString("leave");
}

/*
 * Go away from the server for a little while. Offer the last know away
 * message as a default to the user.
 */
void KBgEngineFIBS::away()
{
	bool ret;
	QString msg = KLineEditDlg::getText(i18n("Please type the message that should be displayed to other\n"
						 "users while you are away."),
					    lastAway, &ret, (QWidget *)parent());
	if (ret) {
		lastAway = msg;
		emit serverString("away " + msg);
		actAway->setEnabled(false);
	}
}

/*
 * Toggle being ready for games
 */
void KBgEngineFIBS::toggle_ready()
{
	emit serverString("toggle ready");
}

/*
 * Toggle the use of greedy bearoffs
 */
void KBgEngineFIBS::toggle_greedy()
{
	emit serverString("toggle greedy");
}

/*
 * Toggle whether we will be asked to double/roll or not
 */
void KBgEngineFIBS::toggle_double()
{
	emit serverString("toggle double");
}

/*
 * Toggle whether we want to see details on rating computations
 */
void KBgEngineFIBS::toggle_ratings()
{
	emit serverString("toggle ratings");
}

/*
 * Come back after being away.
 */
void KBgEngineFIBS::back()
{
	emit serverString("back");
}

/*
 * Double the cube
 */
void KBgEngineFIBS::cube()
{
	emit serverString("double");
}

/*
 * Roll the dice
 */
void KBgEngineFIBS::roll()
{
	emit serverString("roll");
}

/*
 * Reload the board
 */
void KBgEngineFIBS::load()
{
	emit serverString("board");
}

/*
 * Handle the menu short cuts for joining. This is not as pretty as it
 * could or should be, but it works and is easy to understand.
 */
void KBgEngineFIBS::join(const QString &msg)
{
	emit serverString("join " + msg.left(msg.find('(')));
}
void KBgEngineFIBS::join_0() { join(actJoin[0]->text()); }
void KBgEngineFIBS::join_1() { join(actJoin[1]->text()); }
void KBgEngineFIBS::join_2() { join(actJoin[2]->text()); }
void KBgEngineFIBS::join_3() { join(actJoin[3]->text()); }
void KBgEngineFIBS::join_4() { join(actJoin[4]->text()); }
void KBgEngineFIBS::join_5() { join(actJoin[5]->text()); }
void KBgEngineFIBS::join_6() { join(actJoin[6]->text()); }
void KBgEngineFIBS::join_7() { join(actJoin[7]->text()); }


// == invitation handling ======================================================

/*
 * Show the invitation dialog and set the name to player
 */
void KBgEngineFIBS::inviteDialog()
{
	fibsRequestInvitation("");
}

/*
 * Show the invitation dialog and set the name to player
 */
void KBgEngineFIBS::fibsRequestInvitation(const QString &player)
{
	if (!invitationDlg) {
		QString p = player;
		invitationDlg = new KBgInvite("invite");
		connect(invitationDlg, SIGNAL(inviteCommand(const QString &)), this, SLOT(handleCommand(const QString &)));
		connect(invitationDlg, SIGNAL(dialogDone()), this, SLOT(invitationDone()));
	}
	invitationDlg->setPlayer(player);
	invitationDlg->show();
}

/*
 * Finish off the invitation dialog
 */
void KBgEngineFIBS::invitationDone()
{
	delete invitationDlg;
	invitationDlg = 0;
}


// == connection handling ======================================================

/*
 * Establish a connection to the server and log in if the parameter login
 * is true.
 */
void KBgEngineFIBS::connectFIBS()
{
	/*
	 * Make sure the connection parameter are properly set.
	 */
	if (!queryConnection(false))
		return;

	conAction->setEnabled(false);
	newAction->setEnabled(false);
	disAction->setEnabled(false);

	/*
	 * Connect
	 */
	emit infoText(i18n("Looking up %1").arg(infoFIBS[FIBSHost]));
	connection->connectToHost(infoFIBS[FIBSHost], infoFIBS[FIBSPort].toUShort());

	return;
}

/*
 * Hostname has been resolved.
 */
void KBgEngineFIBS::hostFound()
{
	emit infoText(i18n("Connecting to %1").arg(infoFIBS[FIBSHost]));
}

/*
 * An error has occurred. Reset and inform the user.
 */
void KBgEngineFIBS::connError(int f)
{
	switch (f) {
	case QSocket::ErrConnectionRefused:
		emit infoText(i18n("Error, connection has been refused"));
		break;
	case QSocket::ErrHostNotFound:
		emit infoText(i18n("Error, nonexistent host or name server down."));
		break;
	case QSocket::ErrSocketRead:
		emit infoText(i18n("Error, reading data from socket"));
		break;
	}
	connectionClosed();
	return;
}

void KBgEngineFIBS::readData()
{
	QString line;
	while(connection->canReadLine()) {
		line = connection->readLine();
		if (line.length() > 2) {
			line.truncate(line.length()-2);
			handleServerData(line);
		}
	}
}

/*
 * Transmit the string s to the server
 */
void KBgEngineFIBS::sendData(const QString &s)
{
	connection->writeBlock((s+"\r\n").latin1(),2+s.length());
}

/*
 * Connection has been established. Log in and update the menus & actions.
 */
void KBgEngineFIBS::connected()
{
	conAction->setEnabled(false);
	newAction->setEnabled(false);
	disAction->setEnabled(true);

	menu->setItemEnabled( cmdMenuID, true);
	menu->setItemEnabled(respMenuID, true);
	menu->setItemEnabled(optsMenuID, true);

	/*
	 * Initialize the rx state machine
	 */
	rxStatus = RxConnect;
	rxCollect = "";

	/*
	 * Depending on whether somebody else wants to handle the login or not
	 */
	if (login) {

		/*
		 * Make sure the player list is empty when the whole list comes
		 * right after login
		 */
		playerlist->clear();

		/*
		 * Login, using the autologin feature of FIBS, before we even receive anything.
		 */
		QString entry;
		entry.setNum(CLIP_VERSION);
		emit serverString(QString("login ") + PROG_NAME + "-" + PROG_VERSION + " " + entry + " "
				  + infoFIBS[FIBSUser] + " " + infoFIBS[FIBSPswd]);

	} else {

		emit serverString("guest");
		login = true;

	}

	/*
	 * Some visual feedback and done
	 */
	emit infoText(i18n("Connected") + "<br>");
}

/*
 * Create a new account on FIBS. Obviously, this will also create
 * a connection. The actual login is handled in the message parsing
 * state machine.
 */
void KBgEngineFIBS::newAccount()
{
	if (!queryConnection(true))
		return;

	rxStatus = RxNewLogin;
	rxCollect = "";
	login = false;
	connectFIBS();
}

/*
 * Send a disconnection request to the server. The server will disconnect
 * and we will receive a connectionClosed() signal.
 */
void KBgEngineFIBS::disconnectFIBS()
{
	// send two lines in case we are stuck in the login phase
	emit serverString("quit");
	emit serverString("quit");
}

/*
 * Connection to the server is closed for some (unknown) reason. Delete
 * the connection object and get the actions into a proper state.
 */
void KBgEngineFIBS::connectionClosed()
{
	/*
	 * Read remaining input
	 */
	readData();

	/*
	 * Flush whatever is left in the rxBuffer and send a note
	 */
	emit infoText(rxCollect + "<br><hr>");
	emit infoText(i18n("Disconnected.") + "<br>");

	conAction->setEnabled(true);
	newAction->setEnabled(true);
	disAction->setEnabled(false);

	menu->setItemEnabled(joinMenuID, false);
	menu->setItemEnabled( cmdMenuID, false);
	menu->setItemEnabled(respMenuID, false);
	menu->setItemEnabled(optsMenuID, false);
}

/*
 * To establish a connection, we need to query the server name, the port
 * number, the login and the password.
 */
bool KBgEngineFIBS::queryConnection(const bool newlogin)
{
	QString text, msg;
	bool first, ret = true;

	/*
	 * query the connection parameter
	 */
	if (newlogin || infoFIBS[FIBSHost].isEmpty()) {

		msg = KLineEditDlg::getText(i18n("Enter the name of the server you want to connect to.\n"
						 "This should almost always be \"fibs.com\"."),
					    infoFIBS[FIBSHost], &ret, (QWidget *)parent());

		if (ret)
			infoFIBS[FIBSHost] = msg;
		else
			return false;

	}
	if (newlogin || infoFIBS[FIBSPort].isEmpty()) {

		msg = KLineEditDlg::getText(i18n("Enter the port number on the server. "
						 "It should almost always be \"4321\"."),
					    infoFIBS[FIBSPort], &ret, (QWidget *)parent());

		if (ret)
			infoFIBS[FIBSPort] = msg;
		else
			return false;
	}
	if (newlogin || infoFIBS[FIBSUser].isEmpty()) {

		if (newlogin)

			text = i18n("Enter the login you would like to use on the server %1. The login may not\n"
				    "contain spaces or colons. If the login you choose is not available, you'll later be\n"
				    "given the opportunity to pick another one.\n\n").arg(infoFIBS[FIBSHost]);

		else

			text = i18n("Enter your login on the server %1. If you don't have a login, you\n"
				    "should create one using the corresponding menu option.\n\n").arg(infoFIBS[FIBSHost]);


		first = true;
		do {
			msg = (KLineEditDlg::getText(text, infoFIBS[FIBSUser], &ret,
						     (QWidget *)parent())).stripWhiteSpace();
			if (first) {
				text += i18n("The login may not contain spaces or colons!");
				first = false;
			}

		} while (ret && (msg.isEmpty() || msg.contains(' ') || msg.contains(':')));

		if (ret)
			infoFIBS[FIBSUser] = msg;
		else
			return false;
	}
	if (newlogin || infoFIBS[FIBSPswd].isEmpty()) {

		if (newlogin)

			text = i18n("Enter the password you would like to use with the login %1\n"
				    "on the server %2. It may not contain colons.\n\n").
				arg(infoFIBS[FIBSUser]).arg(infoFIBS[FIBSHost]);

		else

			text = i18n("Enter the password for the login %1 on the server %2.\n\n").
				arg(infoFIBS[FIBSUser]).arg(infoFIBS[FIBSHost]);

		first = true;
		do {
			QCString password;
			if (newlogin)
				ret = (KPasswordDialog::getNewPassword(password, text) == KPasswordDialog::Accepted);
			else
				ret = (KPasswordDialog::getPassword(password, text) == KPasswordDialog::Accepted);

			password.stripWhiteSpace();
			msg = password;

			if (first) {
				text += i18n("The password may not contain colons or spaces!");
				first = false;
			}

		} while (ret && (msg.isEmpty() || msg.contains(' ') || msg.contains(':')));

		if (ret)
			infoFIBS[FIBSPswd] = msg;
		else
			return false;
	}

	/*
	 * Made it here, all parameters acquired
	 */
	return true;
}


// == message parsing ==========================================================

/*
 * Pattern setup - rather long and boring
 */
void KBgEngineFIBS::initPattern()
{
	QString pattern;

	/*
	 * Initialize the search pattern array
	 */
	pat[Welcome] = QRegExp(pattern.sprintf("^%d ", CLIP_WELCOME));
	pat[OwnInfo] = QRegExp(pattern.sprintf("^%d ", CLIP_OWN_INFO));
	pat[WhoInfo] = QRegExp(pattern.sprintf("^%d ", CLIP_WHO_INFO));
	pat[WhoEnde] = QRegExp(pattern.sprintf("^%d$", CLIP_WHO_END));
	pat[MotdBeg] = QRegExp(pattern.sprintf("^%d" , CLIP_MOTD_BEGIN));
	pat[MotdEnd] = QRegExp(pattern.sprintf("^%d" , CLIP_MOTD_END));
	pat[MsgPers] = QRegExp(pattern.sprintf("^%d ", CLIP_MESSAGE));
	pat[MsgDeli] = QRegExp(pattern.sprintf("^%d ", CLIP_MESSAGE_DELIVERED));
	pat[MsgSave] = QRegExp(pattern.sprintf("^%d ", CLIP_MESSAGE_SAVED));
	pat[ChatSay] = QRegExp(pattern.sprintf("^%d ", CLIP_SAYS));
	pat[ChatSht] = QRegExp(pattern.sprintf("^%d ", CLIP_SHOUTS));
	pat[ChatWis] = QRegExp(pattern.sprintf("^%d ", CLIP_WHISPERS));
	pat[ChatKib] = QRegExp(pattern.sprintf("^%d ", CLIP_KIBITZES));
	pat[SelfSay] = QRegExp(pattern.sprintf("^%d ", CLIP_YOU_SAY));
	pat[SelfSht] = QRegExp(pattern.sprintf("^%d ", CLIP_YOU_SHOUT));
	pat[SelfWis] = QRegExp(pattern.sprintf("^%d ", CLIP_YOU_WHISPER));
	pat[SelfKib] = QRegExp(pattern.sprintf("^%d ", CLIP_YOU_KIBITZ));
	pat[UserLin] = QRegExp(pattern.sprintf("^%d ", CLIP_LOGIN));
	pat[UserLot] = QRegExp(pattern.sprintf("^%d ", CLIP_LOGOUT));

	pat[NoLogin] = QRegExp("\\*\\* Unknown command: 'login'");
	pat[BegRate] = QRegExp("^rating calculation:$");
	pat[EndRate] = QRegExp("^change for ");
	pat[HTML_lt] = QRegExp("<");
	pat[HTML_gt] = QRegExp(">");
	pat[BoardSY] = QRegExp("^Value of 'boardstyle' set to 3");
	pat[BoardSN] = QRegExp("^Value of 'boardstyle' set to [^3]");
	pat[WhoisBG] = QRegExp("^Information about ");
	pat[WhoisE1] = QRegExp("^  No email address\\.$");
	pat[WhoisE2] = QRegExp("^  Email address: ");
	pat[SelfSlf] = QRegExp("^You say to yourself:");
	pat[Goodbye] = QRegExp("^                             Goodbye\\.");
	pat[GameSav] = QRegExp("The game was saved\\.$");
	pat[RawBord] = QRegExp("^board:");
	pat[YouTurn] = QRegExp("^It's your turn\\. Please roll or double");
	pat[PlsMove] = QRegExp("^Please move [1-6]+ pie");
	pat[EndWtch] = QRegExp("^You stop watching ");
	pat[BegWtch] = QRegExp("^You're now watching ");
	pat[BegGame] = QRegExp("^Starting a new game with ");
	pat[Reload1] = QRegExp("^You are now playing with ");
	pat[Reload2] = QRegExp(" has joined you. Your running match was loaded\\.$");
	pat[OneWave] = QRegExp(" waves goodbye.$");
	pat[TwoWave] = QRegExp(" waves goodbye again.$");
	pat[YouWave] = QRegExp("^You wave goodbye.$");
	pat[GameBG1] = QRegExp("start a [0-9]+ point match");
	pat[GameBG2] = QRegExp("start an unlimited match");
	pat[GameRE1] = QRegExp("are resuming their [0-9]+-point match");
	pat[GameRE2] = QRegExp("are resuming their unlimited match");
	pat[GameEnd] = QRegExp("point match against");
	pat[TabChar] = QRegExp("\\t");
	pat[PlsChar] = QRegExp("\\+");
	pat[Invite0] = QRegExp(" wants to play a [0-9]+ point match with you\\.$");
	pat[Invite1] = QRegExp("^.+ wants to play a ");
	pat[Invite2] = QRegExp(" wants to resume a saved match with you\\.$");
	pat[Invite3] = QRegExp(" wants to play an unlimited match with you\\.$");
	pat[TypJoin] = QRegExp("^Type 'join ");
	pat[OneName] = QRegExp("^ONE USERNAME PER PERSON ONLY!!!");
	pat[YouAway] = QRegExp("^You're away. Please type 'back'");
	pat[YouBack] = QRegExp("^Welcome back\\.$");
	pat[YouMove] = QRegExp("^It's your turn to move\\.");
	pat[YouRoll] = QRegExp("^It's your turn to roll or double\\.");
	pat[TwoStar] = QRegExp("^\\*\\* ");
	pat[OthrNam] = QRegExp("^\\*\\* Please use another name\\. ");
	pat[BoxHori] = QRegExp("^ *\\+-*\\+ *$");
	pat[BoxVer1] = QRegExp("^ *\\|");
	pat[BoxVer2] = QRegExp("\\| *$");
	pat[YourNam] = QRegExp("Your name will be ");
	pat[GivePwd] = QRegExp("Please give your password:");
	pat[RetypeP] = QRegExp("Please retype your password:");
	pat[HelpTxt] = QRegExp("^NAME$");
	pat[MatchB1] = QRegExp(" has joined you for a [0-9]+ point match\\.$");
	pat[MatchB2] = QRegExp(" has joined you for an unlimited match\\.$");
	pat[EndLose] = QRegExp(" wins the [0-9]+ point match [0-9]+-[0-9]+");
	pat[EndVict] = QRegExp(" win the [0-9]+ point match [0-9]+-[0-9]+");
	pat[RejAcpt] = QRegExp("Type 'accept' or 'reject'\\.$");
	pat[YouAcpt] = QRegExp("^You accept the double\\. The cube shows [0-9]+\\.");

	pat[KeepAlv] = QRegExp("^\\*\\* Unknown command: 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'");
	pat[RatingY] = QRegExp("You'll see how the rating changes are calculated\\.$");
	pat[RatingN] = QRegExp("You won't see how the rating changes are calculated\\.$");

	// FIXME same problem as in previous line
	// mpgnu accepts the double.5 arthur_tn - gnu 1 0 1243.32 365 6 983722411  adsl-61-168-141.bna.bellsouth.net - -

	// FIXME: <PLAYER> can't move. -- needs board reload...

        /*

        opponent matchlength score (your points first)
        **gnu 1 0 - 0
        *blah 1 0 - 0
        kraut 1 0 - 0

        logged in and ready **
        logged in *
        otherwise " "

        */

	pat[ConLeav] = QRegExp("^Type 'join' if you want to play the next game, type 'leave' if you don't\\.$");
	pat[GreedyY] = QRegExp("^\\*\\* Will use automatic greedy bearoffs\\.");
	pat[GreedyN] = QRegExp("^\\*\\* Won't use automatic greedy bearoffs\\.");
	pat[BegBlnd] = QRegExp("^\\*\\* You blind ");
	pat[EndBlnd] = QRegExp("^\\*\\* You unblind ");
        pat[MatchB3] = QRegExp("^\\*\\* You are now playing a [0-9]+ point match with ");
        pat[MatchB4] = QRegExp("^\\*\\* You are now playing an unlimited match with ");
	pat[RejCont] = QRegExp("^You reject\\. The game continues\\.");
	pat[AcptWin] = QRegExp("^You accept and win ");
	pat[YouGive] = QRegExp("^You give up\\.");
	pat[DoubleY] = QRegExp("^\\*\\* You will be asked if you want to double\\.");
	pat[DoubleN] = QRegExp("^\\*\\* You won't be asked if you want to double\\.");
}

/*
 * Parse an incoming line and notify all interested parties - first match
 * decides.
 */
void KBgEngineFIBS::handleServerData(QString &line)
{
	QString rawline = line; // contains the line before it is HTML'fied

	/*
	 * Fix-up any HTML-like tags in the line
	 */
	line.replace(pat[HTML_lt], "&lt;");
	line.replace(pat[HTML_gt], "&gt;");

	/*
	 * FIBS sometimes sends tabs, where it should send 8 spaces...
	 */
	line.replace(pat[TabChar], "        ");

	switch (rxStatus) {

	case RxConnect:
		handleMessageConnect(line, rawline);
		break;

	case RxMotd:
		handleMessageMotd(line);
		return;

	case RxWhois:
		handleMessageWhois(line);
		break;

	case RxRating:
		handleMessageRating(line);
		break;

	case RxNewLogin:
		handleMessageNewLogin(line);
		break;

	case RxIgnore:
		/*
		 * Ignore _ALL_ incoming strings - this is needed during the
		 * login phase, when the message box is open.
		 */
		break;

	case RxGoodbye:
		/*
		 * Receive the logout sequence. The string will be flushed by the
		 * disconnectFIBS() callback
		 */
		rxCollect += QString("<font color=\"blue\"><pre>") + line + "</pre></font><br>";
		break;

	case RxNormal:
		handleMessageNormal(line, rawline);
		break;

	default:
		/*
		 * This is a serious problem - latin1() is fine since the line comes from FIBS.
		 */
		std::cerr << "PROBLEM in KBgEngineFIBS::handleServerData: " << line.latin1() << std::endl;
	}
}

/*
 * Handle messages during the RxWhois state
 */
void KBgEngineFIBS::handleMessageWhois(const QString &line)
{
	rxCollect += "<br>&nbsp;&nbsp;&nbsp;&nbsp;" + line;
	if (line.contains(pat[WhoisE1]) || line.contains(pat[WhoisE2])) {
		rxStatus = RxNormal;
		emit infoText("<font color=\"darkgreen\">" + rxCollect + "<br></font>");
	}
}

/*
 * Handle messages during the RxRating state
 */
void KBgEngineFIBS::handleMessageRating(const QString &line)
{
	rxCollect += "<br>" + line;
	if (line.contains(pat[EndRate]) && ++rxCount == 2) {
		emit infoText("<font color=\"blue\">" + rxCollect + "<br></font>");
		rxStatus = RxNormal;
	}
}

/*
 * Handle messages during the RxMotd state
 */
void KBgEngineFIBS::handleMessageMotd(const QString &line)
{
	if (line.contains(pat[MotdEnd])) {
		rxStatus = RxNormal;
		emit infoText("<font color=\"blue\"><pre>" + rxCollect + "</pre></font>");
		/*
		 * just to be on the safe side, we set the value of boardstyle.
		 * we do it here, since this is reasonably late in the login
		 * procedure
		 */
		emit serverString("set boardstyle 3");
	} else {
		QString tline = line;
		tline.replace(pat[BoxHori], "<br><hr>");
		tline.replace(pat[BoxVer1], "");
		tline.replace(pat[BoxVer2], "");
		rxCollect += "<br>" + tline;
	}
}

/*
 * Handle messages during the RxConnect state
 */
void KBgEngineFIBS::handleMessageConnect(const QString &line, const QString &rawline)
{
	/*
	 * Two possibilities: either we are logged in or we sent bad password/login
	 */
	if (line.contains("login:")) {
		/*
		 * This can only happen if the password/login is wrong.
		 */
		if (rxCollect.isEmpty()) {
			rxStatus = RxIgnore;
			int ret = KMessageBox::warningContinueCancel
				((QWidget *)parent(), i18n("There was a problem with "
							   "your login and password. "
							   "You can reenter\n"
							   "your login and password and "
							   "try to reconnect."),
				 i18n("Wrong Login/Password"),
				 i18n("Reconnect"));
			if (ret == KMessageBox::Continue) {
				infoFIBS[FIBSUser] = "";
				infoFIBS[FIBSPswd] = "";
				login = true;
				connectFIBS(); // will reset the rxStatus
			} else {
				rxStatus = RxConnect;
				emit serverString("");
				emit serverString("");
			}
			return;
		}
		emit infoText("<hr><pre>" + rxCollect + "</pre><br>");
		rxCollect = "";
		return;
	}

	/*
	 * Ok, we are logged in! Now receive personal information. These
	 * are completely useless but what the heck.
	 */
	if (line.contains(pat[Welcome])) {
		char p[3][256];
		time_t tmp;
		// Using latin1() is okay, since the string comes from FIBS.
		int words = sscanf (line.latin1(), "%255s%255s%li%255s", p[0], p[1], &tmp, p[2]);
		if (words >= 4) {
			QDateTime d; d.setTime_t(tmp);
			QString text = i18n("%1, last logged in from %2 at %3.").arg(p[1]).arg(p[2]).arg(d.toString());
			emit infoText("<hr><br>" + text);
			playerlist->setName(p[1]);
		}
		return;
	}

	/*
	 * Initial parsing of user options and making sure that settings needed
	 * by us are at the correct value. We use and ignore values according
	 * to the following list:
	 *
	 *     p[ 0]   - CLIP_OWN_INFO
	 *     p[ 1]   - name        -- IGNORE
	 *   OptAllowPip
	 *     n[ 0]   - autoboard   -- IGNORE
	 *   OptAutoDouble
	 *   OptAutoMove
	 *     n[ 1]   - away        -- IGNORE
	 *     n[ 2]   - bell        -- IGNORE
	 *   OptCrawford
	 *     n[ 3]   - double      -- IGNORE
	 *     n[ 4]   - expierience -- IGNORE
	 *   OptGreedy
	 *     n[ 6]   - moreboards  -- IGNORE and set to YES
	 *   OptMoves
	 *     n[ 8]   - notify      -- IGNORE and set to YES
	 *     rating  - rating      -- IGNORE
	 *   OptRatings
	 *   OptReady
	 *     n[10]   - redoubles   -- IGNORE
	 *     n[11]   - report      -- IGNORE and set to YES
	 *   OptSilent
	 *     p[3]    - timezone
	 *
	 */
	if (line.contains(pat[OwnInfo])) {

		rxStatus = RxNormal;

		int fibsOptions[NumFIBSOpt];

		char p[3][256];
		int n[12];
		double rating;

		// Using latin1() is okay, since the string comes from FIBS.
		int words = sscanf (line.latin1(), "%255s%255s%i%i%i%i%i%i%i%i%i%i%i%i%i%lf%i%i%i%i%i%255s",
				    p[0], p[1],
				    &fibsOptions[OptAllowPip],
				    &n[0],
				    &fibsOptions[OptDouble],
				    &fibsOptions[OptAutoMove], // equivalent to OptDouble, can be ignored
				    &n[1], &n[2],
				    &fibsOptions[OptCrawford],
				    &n[3], &n[4],
				    &fibsOptions[OptGreedy],
				    &n[6],
				    &fibsOptions[OptMoves],
				    &n[8],
				    &rating,
				    &fibsOptions[OptRatings],
				    &fibsOptions[OptReady],
				    &n[10], &n[11],
				    &fibsOptions[OptSilent],
				    p[2]);

		if (words >= 22 && n[6] != 1) {
			/*
			 * need to get boards after new dice have arrived
			 */
			emit infoText("<font color=\"red\">" + i18n("The moreboards toggle has been set.") + "</font>");
			emit serverString("toggle moreboards");
		}
		if (words >= 22 && n[8] != 1) {
			/*
			 * need to know who logs out
			 */
			emit infoText("<font color=\"red\">" + i18n("The notify toggle has been set.") + "</font>");
			emit serverString("toggle notify");
		}
		if (words >= 22 && n[11] != 1) {
			/*
			 * want to know who starts playing games
			 */
			emit infoText("<font color=\"red\">" + i18n("The report toggle has been set.") + "</font>");
			emit serverString("toggle report");
		}

		/*
		 * Set the correct toggles in the options menu
		 */
		fibsOpt[OptReady]->setChecked(fibsOptions[OptReady]);
		fibsOpt[OptDouble]->setChecked(!fibsOptions[OptDouble]);
		fibsOpt[OptRatings]->setChecked(fibsOptions[OptRatings]);

		return;
	}

	/*
	 * The beginning of a new login procedure starts starts here
	 */
	if (line.contains(pat[OneName])) {
		rxStatus = RxNewLogin;
		emit infoText(QString("<font color=\"red\">") + rxCollect + "</font>");
		rxCollect = "";
		QString tmp = rawline;
		handleServerData(tmp);
		return;
	}

	/*
	 * Still in the middle of the login sequence, still collecting information
	 */
	rxCollect += "<br>" + line;
}

/*
 * Handle messages during the RxNewLogin state
 */
void KBgEngineFIBS::handleMessageNewLogin(const QString &line)
{
	/*
	 * Request the new login
	 */
	if (line.contains(pat[OneName])) {
		emit serverString(QString("name ") + infoFIBS[FIBSUser]);
		return;
	}
	/*
	 * Ooops, user name already exists
	 */
	if (line.contains(pat[OthrNam])) {
		QString text = i18n("The selected login is alreay in use! Please select another one.");
		bool ret, first = true;
		QString msg;

		do {
			msg = (KLineEditDlg::getText(text, infoFIBS[FIBSUser], &ret,
						     (QWidget *)parent())).stripWhiteSpace();
			if (first) {
				text += i18n("\n\nThe login may not contain spaces or colons!");
				first = false;
			}
		} while (msg.contains(' ') || msg.contains(':'));

		if (ret) {
			infoFIBS[FIBSUser] =  msg;
			emit serverString("name " + msg);
		} else
			emit serverString("bye");

		return;
	}
	/*
	 * first time we send the password
	 */
	if (line.contains(pat[YourNam])) {
		emit serverString(infoFIBS[FIBSPswd]);
		return;
	}
	/*
	 * second time we send the password
	 */
	if (line.contains(pat[GivePwd])) {
		emit serverString(infoFIBS[FIBSPswd]);
		return;
	}
	/*
	 * at this point we are done creating the account
	 */
	if (line.contains(pat[RetypeP])) {

		QString text = i18n("Your account has been created. Your new login is <u>%1</u>. To fully activate "
				    "this account, I will now close the connection. Once you reconnect, you can start "
				    "playing backgammon on FIBS.").arg(infoFIBS[FIBSUser]);
		emit infoText("<br><hr><font color=\"blue\">" + text + "</font><br><hr>");
		emit serverString("bye");
		rxStatus = RxNormal;
		rxCollect = "";
		return;
	}
	return;
}

/*
 * Handle all normal messages - during the RxNormal state
 */
void KBgEngineFIBS::handleMessageNormal(QString &line, QString &rawline)
{

    // - ignored ----------------------------------------------------------------------

    /*
     * For now, the waves are ignored. They should probably go into
     * the chat window -- but only optional
     */
    if (line.contains(pat[OneWave]) || line.contains(pat[TwoWave]) || line.contains(pat[YouWave])) {

        return;
    }

    /*
     * These messages used to go into the games window. If KBackgammon
     * ever gets a games window, they should be in there. For now, they
     * are ignored.
     */
    else if (line.contains(pat[GameBG1]) || line.contains(pat[GameBG2]) || line.contains(pat[GameRE1]) ||
             line.contains(pat[GameRE2]) || line.contains(pat[GameEnd])) {

        return;
    }

    /*
     * Artefact caused by the login test procedure utilized.
     */
    else if (line.contains(pat[NoLogin])) {

        return;
    }

    /*
     * Connection keep-alive response
     */
    else if (line.contains(pat[KeepAlv])) {

        return;
    }

    // --------------------------------------------------------------------------------

    /*
     * Chat and personal messages - note that the chat window sends these messages
     * back to us so we can display them if the user wants that.
     */
    else if (line.contains(pat[ChatSay]) || line.contains(pat[ChatSht]) || line.contains(pat[ChatWis]) ||
             line.contains(pat[ChatKib]) || line.contains(pat[SelfSay]) || line.contains(pat[SelfSht]) ||
             line.contains(pat[SelfWis]) || line.contains(pat[SelfKib]) || line.contains(pat[SelfSlf]) ||
             line.contains(pat[MsgPers]) || line.contains(pat[MsgDeli]) || line.contains(pat[MsgSave])) {

        emit chatMessage(line);
        return;
    }

    // --------------------------------------------------------------------------------

    /*
     * Beginning of games. In all these cases we are playing and not watching.
     */
    else if (line.contains(pat[MatchB1]) || line.contains(pat[MatchB2])) {

        if (useAutoMsg[MsgBeg] && !autoMsg[MsgBeg].stripWhiteSpace().isEmpty())
            emit serverString("kibitz " + autoMsg[MsgBeg]);
    }
    else if (line.contains(pat[MatchB3]) || line.contains(pat[MatchB4])) {

        if (useAutoMsg[MsgBeg] && !autoMsg[MsgBeg].stripWhiteSpace().isEmpty())
            emit serverString("kibitz " + autoMsg[MsgBeg]);
        line = "<font color=\"red\">" + line + "</font>";
    }

    // --------------------------------------------------------------------------------

	/*
	 * The help should be handled separately. A fairly complete implementation of a
	 * help parsing can be found in KFibs.
	 */
	else if (line.contains(pat[HelpTxt])) {

		// do nothing
	}

	// --------------------------------------------------------------------------------

	/*
	 * Simple cases without the need for many comments...
	 */
	else if (line.contains(pat[RawBord])) {

		/*
		 * Save the board string and create a new game state
		 */
		KBgStatus *st = new KBgStatus(currBoard = rawline);

		/*
		 * Save important state data and stop the timeout
		 */
		ct->stop();
		undoCounter = 0;

		pname[US  ] = st->player(US);
		pname[THEM] = st->player(THEM);

		playing = (QString("You") == pname[US]);

		toMove = st->moves();

		/*
		 * Update the caption string
		 */
		if (st->turn() < 0)
			caption = i18n("%1 (%2) vs. %3 (%4) - game over").arg(pname[US]).
				arg(st->points(US)).arg(pname[THEM]).arg(st->points(THEM));
		else if (st->length() < 0)
			caption = i18n("%1 (%2) vs. %3 (%4) - unlimited match").arg(pname[US]).
				arg(st->points(US)).arg(pname[THEM]).arg(st->points(THEM));
		else
			caption = i18n("%1 (%2) vs. %3 (%4) - %5 point match").arg(pname[US]).
				arg(st->points(US)).arg(pname[THEM]).arg(st->points(THEM)).
				arg(st->length());

		emit statText(caption);

		/*
		 * Emit information and drop the state object
		 */
		emit allowMoving(playing && (st->turn() == US));
		emit newState(*st);

		delete st;

		/*
		 * Set the actions correctly
		 */
		emit allowCommand(Load, true );
		emit allowCommand(Undo, false);
		emit allowCommand(Redo, false);
		emit allowCommand(Done, false);

		return;
	}
	else if (line.contains(pat[PlsMove]) || line.contains(pat[YouMove])) {

		KNotifyClient::event("move", i18n("Please make your move"));

	}

	// --------------------------------------------------------------------------------

	/*
	 * Being away and coming back
	 */
	else if (line.contains(pat[YouAway])) {

		emit changePlayerStatus(infoFIBS[FIBSUser], KFibsPlayerList::Away, true);
		actBack->setEnabled(true);
		line += "<br><pre>  </pre>" + i18n("(or use the corresponding menu entry to join the match)");
	}
	else if (line.contains(pat[YouBack])) {

		emit changePlayerStatus(infoFIBS[FIBSUser], KFibsPlayerList::Away, false);
		actBack->setEnabled(false);
		actAway->setEnabled(true);
	}

	// --------------------------------------------------------------------------------

	/*
	 * Catch the response of the user responding to double or resign
	 */
	else if (line.contains(pat[YouGive]) || line.contains(pat[RejCont]) || line.contains(pat[AcptWin])) {

		actAccept->setEnabled(false);
		actReject->setEnabled(false);
	}

	// --------------------------------------------------------------------------------

	/*
	 * Catch the responses to newly set toggles
	 */
	else if (line.contains(pat[GreedyY]) || line.contains(pat[GreedyN])) {

		fibsOpt[OptGreedy]->setChecked(line.contains(pat[GreedyY]));
		line = "<font color=\"red\">" + line + "</font>";
	}
	else if (line.contains(pat[DoubleY]) || line.contains(pat[DoubleN])) {

		fibsOpt[OptDouble]->setChecked(line.contains(pat[DoubleY]));
		line = "<font color=\"red\">" + line + "</font>";
	}

	else if (line.contains(pat[RatingY]) || line.contains(pat[RatingN])) {

		fibsOpt[OptRatings]->setChecked(line.contains(pat[RatingY]));
		line = "<font color=\"red\">" + line + "</font>";
	}

	// --------------------------------------------------------------------------------

	/*
	 * It's our turn to roll or double
	 */
	else if (line.contains(pat[YouTurn]) || line.contains(pat[YouRoll])) {

		emit allowCommand(Cube, playing);
		emit allowCommand(Roll, playing);

		emit statText(caption); // force a pip count recomputation by the board

		KNotifyClient::event("roll or double", i18n("It's your turn to roll the dice or double the cube"));
	}

	// --------------------------------------------------------------------------------

	/*
	 * Got an invitation for a match
	 */
	else if (line.contains(pat[Invite0]) || line.contains(pat[Invite2]) || line.contains(pat[Invite3])) {

		rxCollect = rawline.left(rawline.find(' '));
		emit serverString("rawwho " + rxCollect);

		if (line.contains(pat[Invite0])) {
			rawline.replace(pat[Invite1], "");
			rawline = rxCollect + " "+ rawline.left(rawline.find(' '));
		} else if (line.contains(pat[Invite2])) {
			rawline = rxCollect + " r";
		} else if (line.contains(pat[Invite3])) {
			invitations += rxCollect + " u";
		}
		invitations += rawline;
		return; // will be printed once the rawwho is received
	}

	// - rx status changes ------------------------------------------------------------

	else if (line.contains(pat[WhoisBG])) {
		rxStatus = RxWhois;
		rxCollect = QString("<br><u>") + line + "</u>";
		return;
	}
	else if (line.contains(pat[MotdBeg])) {
		rxStatus = RxMotd;
		rxCollect = "";
		return;
	}
	else if (line.contains(pat[BegRate])) {
		rxStatus = RxRating;
		rxCount = 0;
		rxCollect = "<br>" + line;
		return;
	}
	else if (line.contains(pat[Goodbye])) {
		rxStatus = RxGoodbye;
		rxCollect = "<br><hr><br>";
		handleServerData(rawline); // danger: recursion!
		return;
	}

	// --------------------------------------------------------------------------------
	// --------------------------------------------------------------------------------
	// --------------------------------------------------------------------------------
	// --------------------------------------------------------------------------------

	/*
	 * Continue a mutli game match? We have to either leave or continue
	 */
	else if (line.contains(pat[ConLeav])) {
		actConti->setEnabled(true);
		actLeave->setEnabled(true);
		line.append("<br><pre>  </pre>" + i18n("(or use the corresponding menu "
						       "entry to leave or continue the match)"));
	}
	/*
	 * Beginning and end of user updates
	 */
	else if (line.contains(pat[WhoInfo])) {
		rawline.replace(pat[WhoInfo], "");
		if (rawline.contains(QRegExp("^" + infoFIBS[FIBSUser] + " "))) {
			int ready;
			// Using latin1() is fine, since the string is coming from FIBS.
			sscanf(rawline.latin1(), "%*s %*s %*s %i %*s %*s %*s %*s %*s %*s %*s %*s", &ready);
			fibsOpt[OptReady]->setChecked(ready);
		}
		emit fibsWhoInfo(rawline);
		return;
	}
	else if (line.contains(pat[WhoEnde])) {
		emit fibsWhoEnd();
		return;
	}
	/*
	 * This message is ignored. The instruction is given elsewhere (and slightly
	 * delayed in the flow of time).
	 */
	if (line.contains(pat[TypJoin])) {
		return;
	}
	/*
	 * Watching other players
	 */
	else if (line.contains(pat[BegWtch])) {
		emit allowCommand(Load, true);
		rawline.replace(pat[BegWtch], "");
		rawline.truncate(rawline.length()-1);
		emit fibsStartNewGame(rawline);
		load();
	}
	else if (line.contains(pat[EndWtch])) {
		emit gameOver();
	}
	/*
	 * Blinding of players, the actual blind is handled by
	 * the player list
	 */
	else if (line.contains(pat[BegBlnd])) {
		rawline.replace(pat[BegBlnd], "");
		rawline.truncate(rawline.length()-1);
		emit changePlayerStatus(rawline, KFibsPlayerList::Blind, true);
		line = "<font color=\"red\">" + line + "</font>";
	}
	else if (line.contains(pat[EndBlnd])) {
		rawline.replace(pat[EndBlnd], "");
		rawline.truncate(rawline.length()-1);
		emit changePlayerStatus(rawline, KFibsPlayerList::Blind, false);
		line = "<font color=\"red\">" + line + "</font>";
	}
	/*
	 * Starting or reloading games or matches
	 */
	else if (line.contains(pat[BegGame])) {
		rawline.replace(pat[BegGame], "");
		rawline.truncate(rawline.length()-1);
		emit fibsStartNewGame(rawline);
		fibsOpt[OptDouble]->setChecked(true);
		fibsOpt[OptGreedy]->setChecked(false);
		actConti->setEnabled(false);
		actLeave->setEnabled(false);
	}
	else if (line.contains(pat[Reload1])) {
		rawline.replace(pat[Reload1], "");
		rawline = rawline.left(rawline.find(' '));
		rawline.truncate(rawline.length()-1);
		emit fibsStartNewGame(rawline);
		fibsOpt[OptDouble]->setChecked(true);
		fibsOpt[OptGreedy]->setChecked(false);
		actConti->setEnabled(false);
		actLeave->setEnabled(false);
		load();
	}
	else if (line.contains(pat[Reload2])) {
		rawline.replace(pat[Reload2], "");
		emit fibsStartNewGame(rawline);
		fibsOpt[OptDouble]->setChecked(true);
		fibsOpt[OptGreedy]->setChecked(false);
		actConti->setEnabled(false);
		actLeave->setEnabled(false);
		load();
	}
	/*
	 * Opponent offered resignation or the cube. We have to accept
	 * or reject the offer.
	 */
	else if (line.contains(pat[RejAcpt])) {
		actAccept->setEnabled(true);
		actReject->setEnabled(true);
		line += "<br><pre>  </pre>" + i18n("(or use the corresponding menu "
						   "entry to accept or reject the offer)");
	}
	/*
	 * This is strange: FIBS seems to not send a newline at the
	 * end of this pattern. So we work around that.
	 */
	else if (line.contains(pat[YouAcpt])) {
		actAccept->setEnabled(false);
		actReject->setEnabled(false);
		rawline.replace(pat[YouAcpt], "");
		line.truncate(line.length()-rawline.length());
		if (!rawline.stripWhiteSpace().isEmpty()) {
			handleServerData(rawline);
		}
	}
	/*
	 * Ending of games
	 */
	else if (line.contains(pat[EndLose])) {
		if (playing) {
			KNotifyClient::event("game over l", i18n("Sorry, you lost the game."));
			if (useAutoMsg[MsgLos] && !autoMsg[MsgLos].stripWhiteSpace().isEmpty())
				emit serverString(QString("tell ") + pname[THEM] + " " + autoMsg[MsgLos]);
		}
		emit gameOver();
	}
	else if (line.contains(pat[EndVict])) {
		if (playing) {
			KNotifyClient::event("game over w", i18n("Congratulations, you won the game!"));
			if (useAutoMsg[MsgWin] && !autoMsg[MsgWin].stripWhiteSpace().isEmpty())
				emit serverString(QString("tell ") + pname[THEM] + " " + autoMsg[MsgWin]);
		}
		emit gameOver();
	}
	else if (line.contains(pat[GameSav])) {
		emit gameOver();
	}
	/*
	 * User logs out. This has to be signalled to the player
	 * list. Get the true user names by working on the rawline.
	 */
	else if (line.contains(pat[UserLot])) {
		rawline.replace(pat[UserLot], "");
		emit fibsLogout(rawline.left(rawline.find(' ')));
		return;
	}
	/*
	 * Emit the name of the newly logged in user.
	 */
	else if (line.contains(pat[UserLin])) {
		rawline.replace(pat[UserLin], "");
		emit fibsLogin(rawline.left(rawline.find(' ')));
		return;
	}
	/*
	 * Special attention has to be paid to the proper setting of
	 * the 'boardstyle' variable, since we will not be able to display
	 * the board properly without it.
	 */
	else if (line.contains(pat[BoardSY])) {
		// ignored
		return;
	}
	else if (line.contains(pat[BoardSN])) {
		emit serverString("set boardstyle 3");
		emit infoText(QString("<font color=\"red\"><br>")
			      + i18n("You should never set the 'boardstyle' variable "
				     "by hand! It is vital for proper functioning of "
				     "this program that it remains set to 3. It has "
				     "been reset for you.")
			      + "<br></font>");
		return;
	}
	/*
	 * This is the final fall through: if the line started with ** and
	 * hasn't been processed, make it red, since it is a server resp.
	 * to something we just did.
	 */
	else if (line.contains(pat[TwoStar])) {
		line = "<font color=\"red\">" + line + "</font>";
	}

	// --------------------------------------------------------------------------------

	/*
	 * Print whatever part of the line made it here
	 */
	emit infoText(line);
}

// EOF


// == constructor, destructor and setup ========================================

/*
 * Constructor
 */
KBgEngineFIBS::KBgEngineFIBS(QWidget *parent, QString *name, QPopupMenu *pmenu)
	: KBgEngine(parent, name, pmenu)
{
	/*
	 * No connection, not playing, ready for login
	 */
	connection = new QSocket(parent, "fibs connection");
	playing = false;
	login = true;

	connect(connection, SIGNAL(hostFound()),            this, SLOT(hostFound()));
	connect(connection, SIGNAL(connected()),            this, SLOT(connected()));
	connect(connection, SIGNAL(error(int)),             this, SLOT(connError(int)));
	connect(connection, SIGNAL(connectionClosed()),     this, SLOT(connectionClosed()));
	connect(connection, SIGNAL(delayedCloseFinished()), this, SLOT(connectionClosed()));
	connect(connection, SIGNAL(readyRead()),            this, SLOT(readData()));

	connect(this, SIGNAL(serverString(const QString &)), this, SLOT(sendData(const QString &)));

	/*
	 * No invitation dialog
	 */
	invitationDlg = 0;

	connect(this, SIGNAL(fibsWhoInfo(const QString &)), this, SLOT(changeJoin(const QString &)));
	connect(this, SIGNAL(fibsLogout (const QString &)), this, SLOT(cancelJoin(const QString &)));
	connect(this, SIGNAL(gameOver()), this, SLOT(endGame()));

	/*
	 * Creating, initializing and connecting the player list
	 */
	playerlist = new KFibsPlayerList(0, "fibs player list");

	connect(this, SIGNAL(fibsWhoInfo(const QString &)), playerlist, SLOT(changePlayer(const QString &)));
	connect(this, SIGNAL(fibsLogout (const QString &)), playerlist, SLOT(deletePlayer(const QString &)));
	connect(this, SIGNAL(fibsWhoEnd()),           playerlist, SLOT(stopUpdate()));
	connect(this, SIGNAL(fibsConnectionClosed()), playerlist, SLOT(stopUpdate()));
	connect(this, SIGNAL(changePlayerStatus(const QString &, int, bool)),
		playerlist, SLOT(changePlayerStatus(const QString &, int, bool)));
	connect(playerlist, SIGNAL(fibsCommand(const QString &)), this, SLOT(handleCommand(const QString &)));
	connect(playerlist, SIGNAL(fibsInvite(const QString &)), this, SLOT(fibsRequestInvitation(const QString &)));

	/*
	 * Create, initialize and connect the chat window
	 */
	chatWindow = new KBgChat(0, "chat window");

	connect(this, SIGNAL(chatMessage(const QString &)), chatWindow, SLOT(handleData(const QString &)));
	connect(this, SIGNAL(fibsStartNewGame(const QString &)), chatWindow, SLOT(startGame(const QString &)));
	connect(this, SIGNAL(gameOver()), chatWindow, SLOT(endGame()));
	connect(this, SIGNAL(fibsLogout (const QString &)), chatWindow, SLOT(deletePlayer(const QString &)));
	connect(chatWindow, SIGNAL(fibsCommand(const QString &)), this, SLOT(handleCommand(const QString &)));
	connect(chatWindow, SIGNAL(fibsRequestInvitation(const QString &)), this, SLOT(fibsRequestInvitation(const QString &)));
	connect(chatWindow, SIGNAL(personalMessage(const QString &)), this, SLOT(personalMessage(const QString &)));
	connect(playerlist, SIGNAL(fibsTalk(const QString &)), chatWindow, SLOT(fibsTalk(const QString &)));

	/*
	 * Creating, initializing and connecting the menu
	 * ----------------------------------------------
	 */
	respMenu = new QPopupMenu();
	joinMenu = new QPopupMenu();
	cmdMenu  = new QPopupMenu();
	optsMenu = new QPopupMenu();

	/*
	 * Initialize the FIBS submenu - this is also put in the play menu
	 */
	conAction  = new KAction(i18n("&Connect"),    0, this, SLOT(   connectFIBS()), this);
	newAction  = new KAction(i18n("New Account"), 0, this, SLOT(    newAccount()), this);
	disAction  = new KAction(i18n("&Disconnect"), 0, this, SLOT(disconnectFIBS()), this);

	conAction->setEnabled(true ); conAction->plug(menu);
	disAction->setEnabled(false); disAction->plug(menu);
	newAction->setEnabled(true ); newAction->plug(menu);

	menu->insertSeparator();

	(invAction  = new KAction(i18n("&Invite..."), 0, this, SLOT(inviteDialog()), this))->plug(menu);

	/*
	 * Create and fill the response menu. This is for all these: type this or
	 * that messages from FIBS.
	 */
	cmdMenuID = menu->insertItem(i18n("&Commands"), cmdMenu); {

		(actAway  = new KAction(i18n("Away"), 0, this, SLOT(away()), this))->plug(cmdMenu);
		(actBack  = new KAction(i18n("Back"), 0, this, SLOT(back()), this))->plug(cmdMenu);

		actAway->setEnabled(true);
		actBack->setEnabled(false);
	}

	/*
	 * Create the server side options. This is preliminary and needs more work.
	 * The available options are skewed, since they refelect the needs of the
	 * author. Contact jens@hoefkens.com if your favorite option is missing.
	 */
	optsMenuID = menu->insertItem(i18n("&Options"), optsMenu); {

		for (int i = 0; i < NumFIBSOpt; i++)
			fibsOpt[i] = 0;

		fibsOpt[OptReady]   = new KToggleAction(i18n("Ready to Play"),
							0, this, SLOT(toggle_ready()),   this);
		fibsOpt[OptRatings] = new KToggleAction(i18n("Show Rating Computations"),
							0, this, SLOT(toggle_ratings()), this);
		fibsOpt[OptRatings]->setCheckedState(i18n("Hide Rating Computations"));
		fibsOpt[OptGreedy]  = new KToggleAction(i18n("Greedy Bearoffs"),
							0, this, SLOT(toggle_greedy()),  this);
		fibsOpt[OptDouble]  = new KToggleAction(i18n("Ask for Doubles"),
							0, this, SLOT(toggle_double()),  this);

		for (int i = 0; i < NumFIBSOpt; i++)
			if (fibsOpt[i])
				fibsOpt[i]->plug(optsMenu);

	}

	/*
	 * Create and fill the response menu. This is for all these: type this or
	 * that messages from FIBS.
	 */
	respMenuID = menu->insertItem(i18n("&Response"), respMenu); {

		(actAccept  = new KAction(i18n("Accept"), 0, this, SLOT(accept()), this))->plug(respMenu);
		(actReject  = new KAction(i18n("Reject"), 0, this, SLOT(reject()), this))->plug(respMenu);

		actAccept->setEnabled(false);
		actReject->setEnabled(false);

		respMenu->insertSeparator();

		(actConti  = new KAction(i18n("Join"),  0, this, SLOT(match_conti()), this))->plug(respMenu);
		(actLeave  = new KAction(i18n("Leave"), 0, this, SLOT(match_leave()), this))->plug(respMenu);

		actConti->setEnabled(false);
		actLeave->setEnabled(false);
	}

	/*
	 * Create the join menu and do not fill it (this happens at first
	 * action setup.
	 */
	joinMenuID = menu->insertItem(i18n("&Join"), joinMenu); {
		numJoin = -1;

		actJoin[0] = new KAction("", 0, this, SLOT(join_0()), this);
		actJoin[1] = new KAction("", 0, this, SLOT(join_1()), this);
		actJoin[2] = new KAction("", 0, this, SLOT(join_2()), this);
		actJoin[3] = new KAction("", 0, this, SLOT(join_3()), this);
		actJoin[4] = new KAction("", 0, this, SLOT(join_4()), this);
		actJoin[5] = new KAction("", 0, this, SLOT(join_5()), this);
		actJoin[6] = new KAction("", 0, this, SLOT(join_6()), this);
		actJoin[7] = new KAction("", 0, this, SLOT(join_7()), this);
	}

	menu->setItemEnabled(joinMenuID, false);
	menu->setItemEnabled( cmdMenuID, false);
	menu->setItemEnabled(respMenuID, false);
	menu->setItemEnabled(optsMenuID, false);

	/*
	 * Continue with the FIBS menu
	 */
	menu->insertSeparator();

	(listAct = new KToggleAction(i18n("&Player List"), 0, this, SLOT(showList()), this))->plug(menu);
	(chatAct = new KToggleAction(i18n("&Chat"),        0, this, SLOT(showChat()), this))->plug(menu);

	connect(playerlist, SIGNAL(windowVisible(bool)), listAct, SLOT(setChecked(bool)));
	connect(chatWindow, SIGNAL(windowVisible(bool)), chatAct, SLOT(setChecked(bool)));

	/*
	 * Create message IDs. This sets up a lot of regular expressions.
	 */
	initPattern();

	/*
	 * Restore old settings
	 */
	readConfig();

	// FIXME: open the child windows in start() and not here

	/*
	 * Update the menu actions
	 */
	listAct->setChecked(playerlist->isVisible());
	chatAct->setChecked(chatWindow->isVisible());

	/*
	 * Initialize the keepalive timer FIXME: make this a setting
	 */
	keepalive = true;

	// FIXME: move the start to connect...

	keepaliveTimer = new QTimer(this);
	connect(keepaliveTimer, SIGNAL(timeout()), this, SLOT(keepAlive()));
	keepaliveTimer->start(1200000);
}

/*
 * Destructor deletes child objects if necessary
 */
KBgEngineFIBS::~KBgEngineFIBS()
{
	delete joinMenu;
	delete respMenu;
	delete cmdMenu;
	delete optsMenu;

	delete connection;
	delete invitationDlg;

	delete playerlist;
	delete chatWindow;
}


