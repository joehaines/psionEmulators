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

  $Id: kbgng.cpp,v 1.9 2003/09/23 16:20:40 aacid Exp $

*/

#include "kbgng.moc"
#include "kbgng.h"

#include <kapplication.h>
#include <kmessagebox.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <qlayout.h>
#include <kiconloader.h>
#include <kstdaction.h>
#include <qbuttongroup.h>
#include <qcheckbox.h>
#include <kconfig.h>
#include <klocale.h>
#include <kmainwindow.h>
#include <klineeditdlg.h>
#include <krandomsequence.h>
#include <qstringlist.h>
#include <qcstring.h>
#include <qtextstream.h>

#include <version.h>

#include <iostream>


/*
 * Constructor
 */
KBgEngineNg::KBgEngineNg(QWidget *parent, QString *name, QPopupMenu *pmenu)
    : KBgEngine(parent, name, pmenu)
{
    // get a new game
    initGame();

    // create actions and menus
    QString label[MaxTypes];

    label[Local    ] = i18n("Local Games");
    label[NetServer] = i18n("Offer Network Games");
    label[NetClient] = i18n("Join Network Games");

    QStringList list;
    for (int i = 0; i < MaxTypes; i++)
	list.append(label[i]);

    _gameSelect = new KSelectAction(i18n("&Types"), 0, this, SLOT(setGame()), this);
    _gameSelect->setItems(list);
    _gameSelect->plug(menu);

    menu->insertSeparator();

    _connectAction = new KAction(i18n("&Names..."), 0, this, SLOT(changeName()), this);
    _connectAction->plug(menu);

    // Restore last settings
    readConfig();

    // initialize to local games
    _player[0] = _player[1] = 0;
    _currGame = None;
    _gameSelect->setCurrentItem(Local);
    setGame();
}


/*
 * Switch the local game type. This is called by the menu...
 *
 * TODO: lots of work and testing needed...
 */
void KBgEngineNg::setGame()
{
    // shutdown old game
    switch (_currGame) {

    case Local:
	// nothing to do...
	break;

    case NetServer:
	_game->stopServerConnection();
	break;

    case NetClient:
	_game->disconnect();
	break;

    default:
	// ignore
	break;
    }

    // reset the game and delete the players
    delete _game;
    initGame();

    emit infoText("<br/>");

    // initialize a new game
    bool ret = false;
    QString label, port_s, host_s;
    Q_UINT16 port;

    switch (_currGame = _gameSelect->currentItem()) {

    case Local:

	_game->addPlayer(createPlayer(0, _name[0]));
	_game->addPlayer(createPlayer(1, _name[1]));
	break;

    case NetServer:
	label = i18n("Type the port number on which you want to listen to "
		     "connections.\nThe number should be between 1024 and "
		     "65535.");
	port_s.setNum(_port);
	do {
	    port_s = KLineEditDlg::getText(label, port_s, &ret, (QWidget *)parent());
	    if (!ret)
		return;
	    port = port_s.toUShort(&ret);
	} while (port_s.isEmpty() && !ret);

	if (_game->offerConnections(port))
	    emit infoText(i18n("Now waiting for incoming connections on port %1.").
			  arg(_port = port));
	else
	    emit infoText(i18n("Failed to offer connections on port %1.").arg(port));

	_game->addPlayer(createPlayer(0, _name[0]));
	break;

    case NetClient:
	label = i18n("Type the name of the server you want to connect to.");
	host_s = _host;
	do {
	    host_s = KLineEditDlg::getText(label, host_s, &ret, (QWidget *)parent());
	    if (!ret)
		return;
	} while (host_s.isEmpty());

	label = i18n("Type the port number on %1 you want to connect to.\nThe "
		     "number should be between 1024 and 65535.").arg(host_s);
	port_s.setNum(_port);
	do {
	    port_s = KLineEditDlg::getText(label, port_s, &ret, (QWidget *)parent());
	    if (!ret)
		return;
	    port = port_s.toUShort(&ret);
	} while (port_s.isEmpty() && !ret);

	/*
	 * Hi Martin: another thing you night want to try is to move this to the
	 * place marked by <HERE> (about 10 lines further down. If you do that, the
	 * players are created properly on the server, but a total of three players
	 * is created on the client.
	 */
	_game->addPlayer(createPlayer(0, _name[0]));

	if (_game->connectToServer(host_s, port))
	    emit infoText(i18n("Now connected to %1:%2.").arg(_host = host_s).
			  arg(_port = port));
	else
	    emit infoText(i18n("Failed to connect to %1:%2.").arg(_host = host_s).
			  arg(_port = port));

	// <HERE>

	break;

    default:
	kdDebug(true, PROG_COOKIE) << "setGame parameter invalid: "
				   << _currGame << endl;
	_currGame = None;
	return;
    }

    // we are still having problems with player creation...

    // FIXME - which status    _game->setGameStatus(KGame::End);
}

void KBgEngineNg::slotPlayerJoinedGame(KPlayer *p)
{
    emit infoText(i18n("Player %1 (%2) has joined the game.").arg(p->name()).arg(p->id()));
    cerr << i18n("Player %1 (%2) has joined the game.").arg(p->name()).arg(p->id()).latin1() << endl;
}

void KBgEngineNg::slotCreatePlayer(KPlayer *&p, int rtti, int io, bool v, KGame *g)
{
    Q_UNUSED(rtti)
    Q_UNUSED(g)
    Q_UNUSED(io)
    emit infoText(i18n("creating player. virtual=%1").arg(v));
    p = createPlayer(1);
}

void KBgEngineNg::slotClientConnected(Q_UINT32)
{
    cerr << "client has joint the game..." << endl;
}

void KBgEngineNg::slotClientDisconnected(Q_UINT32, bool)
{
    cerr << "KBgEngineNg::slotClientDisconnected" << endl;
}











































// == start and init games =====================================================

/*
 * Start a new game. ...
 */
void KBgEngineNg::newGame()
{
    // TODO
    cerr << "games are not yet working..." << endl;
}

/*
 * Finish the last move - called by the timer and directly by the used
 */
void KBgEngineNg::done()
{
    // empty
}

/*
 * Undo the last move
 */
void KBgEngineNg::undo()
{
    // TODO
}

/*
 * Redo the last move
 */
void KBgEngineNg::redo()
{
    // TODO
}

/*
 * Take the move string and make the changes on the working copy
 * of the state.
 */
void KBgEngineNg::handleMove(QString *s)
{
    Q_UNUSED(s)
    // TODO
}

/*
 * Roll random dice for the player whose turn it is
 */
void KBgEngineNg::roll()
{
    // empty
}

/*
 * If possible, roll random dice for player w
 */
void KBgEngineNg::rollDice(const int w)
{
    Q_UNUSED(w)
    // empty
}

/*
 * Double the cube for the player that can double  - asks player
 */
void KBgEngineNg::cube()
{
    // TODO
}

/*
 * Double the cube for player w
 */
void KBgEngineNg::doubleCube(const int)
{
	cube();
}

/*
 * Put the engine specific details in the setup dialog
 */
void KBgEngineNg::getSetupPages(KDialogBase *)
{
    // FIXME: do nothing...
}

/*
 * Called when the setup dialog is positively closed
 */
void KBgEngineNg::setupOk()
{
    // FIXME: do nothing...
}
void KBgEngineNg::setupDefault()
{
    // FIXME: do nothing...
}
void KBgEngineNg::setupCancel()
{
    // FIXME: do nothing...
}


// == various slots & functions ================================================

/*
 * Check with the user if we should really quit in the middle of a
 * game.
 */
bool KBgEngineNg::queryClose()
{
    return true;
}

/*
 * Quitting is fine at any time
 */
bool KBgEngineNg::queryExit()
{
    return true;
}

/*
 * Load the last known sane state of the board
 */
void KBgEngineNg::load()
{
    // TODO
}

/*
 * Store if cmd is allowed or not
 */
void KBgEngineNg::setAllowed(int cmd, bool f)
{
    switch (cmd) {
    case Roll:
	rollingAllowed = f;
	return;
    case Undo:
	undoPossible = f;
	return;
    case Cube:
	doublePossible = f;
	return;
    case Done:
	donePossible = f;
	return;
    }
}


// ********************************************************************************
// ********************************************************************************

// DONE

// ********************************************************************************
// ********************************************************************************


/*
 * Destructor.
 */
KBgEngineNg::~KBgEngineNg()
{
    saveConfig();
    delete _game;
}

/*
 * Restore settings
 */
void KBgEngineNg::readConfig()
{
    KConfig* config = kapp->config();
    config->setGroup("next generation engine");

    _port = config->readNumEntry("port", PROG_COOKIE);
    _host = config->readEntry("host", "localhost");

    _name[0] = config->readEntry("name_0", i18n("one"));
    _name[1] = config->readEntry("name_1", i18n("two"));
}

/*
 * Save the engine specific settings
 */
void KBgEngineNg::saveConfig()
{
    KConfig* config = kapp->config();
    config->setGroup("next generation engine");

    config->writeEntry("port", _port);
    config->writeEntry("host", _host);

    config->writeEntry("name_0", _name[0]);
    config->writeEntry("name_1", _name[1]);
}

/*
 * Read the users input from the command line and send it to all
 * players. Although the message gets the Cmd ID, it is currently
 * handled as a regular text message.
 */
void KBgEngineNg::handleCommand(const QString& text)
{
    QByteArray msg;
    QTextStream ts(msg, IO_WriteOnly);
    ts << text;
    if (!_game->sendMessage(msg, KBgGame::Cmd))
	kdDebug(true, PROG_COOKIE) << "couldn't send message: "
				   << text.latin1() << endl;
}

/*
 * Return a random integer between 1 and 6. Use the KGame random
 * number generator.
 */
int KBgEngineNg::getRandom()
{
    return 1+_game->random()->getLong(6);
}

/*
 * A player propert has changed - check if we care
 */
void KBgEngineNg::slotPropertyChanged(KGamePropertyBase *p, KPlayer *me)
{
    int player = (me->id() == _player[1]->id());

    switch (p->id()) {

    case KGamePropertyBase::IdName:
	emit infoText(i18n("Player %1 has changed the name to %2.")
		      .arg(_name[player]).arg(me->name()));
	_name[player] = me->name();
	break;

    default:
	kdDebug(true, PROG_COOKIE) << "KBgPlayer (" << me  << ") property change ("
				   << p->id() << ") ignored" << endl;
	break;
    }
}

/*
 * A game property has changed
 */
void KBgEngineNg::slotPropertyChanged(KGamePropertyBase *p, KGame *me)
{
    Q_UNUSED(me)
    switch (p->id()) {

    default:
	kdDebug(true, PROG_COOKIE) << "Change in GameProperty " << p->id()
				   << " has been ignored." << endl;
	break;
    }
}

/*
 * Change the names of all local players
 */
void KBgEngineNg::changeName()
{
    bool ok = false;
    QString name;

    for (int i = 0; i < 2; i++) {
	name = QString::null;
	while (!_player[i]->isVirtual() && name.isEmpty()) {
	    name = KLineEditDlg::getText(i18n("Type the name of the %1 player.").
					 arg(i ? "second" : "first"),
					 _name[i], &ok, (QWidget *)parent());
	    if (!ok) return;
	    _player[i]->setName(name);
	}
    }
}

/*
 * Receive data sent via KBgGame::sendMessage(...)
 */
void KBgEngineNg::slotNetworkData(int msgid, const QByteArray &msg, Q_UINT32 r, Q_UINT32 s)
{
    Q_UNUSED(r);
    Q_UNUSED(s);
    switch (msgid) {

    case KBgGame::Cmd:
	emit infoText(msg);
	emit infoText(i18n("Players are %1 and %2").arg(_player[0]->name())
		      .arg(_player[1]->name()));
	break;

    default:
	kdDebug(true, PROG_COOKIE) << "Ignored message ID: " << msgid << endl;
	break;
    }
}

/*
 * Create the i-th player
 */
KBgPlayer * KBgEngineNg::createPlayer(int i, QString name)
{
    KBgPlayer *p = new KBgPlayer();

    if (!name.isNull())
	p->setName(name);

    p->findProperty(KGamePropertyBase::IdName)->setEmittingSignal(true);

    connect(p, SIGNAL(signalPropertyChanged(KGamePropertyBase *, KPlayer *)),
	    this, SLOT(slotPropertyChanged(KGamePropertyBase *, KPlayer *)));

    return (_player[i] = p);
}

/*
 * Create and connect the game object
 */
void KBgEngineNg::initGame()
{
    _game = new KBgGame(PROG_COOKIE);
    _game->random()->setSeed(getpid()*time(NULL));

    connect(_game, SIGNAL(signalPlayerJoinedGame(KPlayer *)),
	    this, SLOT(slotPlayerJoinedGame(KPlayer *)));
    connect(_game, SIGNAL(signalCreatePlayer(KPlayer *&, int, int, bool, KGame *)),
	    this, SLOT(slotCreatePlayer(KPlayer *&, int, int, bool, KGame *)));

    connect(_game, SIGNAL(signalClientConnected(Q_UINT32)),
	    this, SLOT(slotClientConnected(Q_UINT32)));
    connect(_game, SIGNAL(signalClientDisconnected(Q_UINT32, bool)),
	    this, SLOT(slotClientDisconnected(Q_UINT32, bool)));

    connect(_game, SIGNAL(signalPropertyChanged(KGamePropertyBase *, KGame *)),
	    this, SLOT(slotPropertyChanged(KGamePropertyBase *, KGame *)));
    connect(_game, SIGNAL(signalNetworkData(int,const QByteArray &, Q_UINT32, Q_UINT32)),
	    this, SLOT(slotNetworkData(int,const QByteArray &, Q_UINT32, Q_UINT32)));
}

// EOF
