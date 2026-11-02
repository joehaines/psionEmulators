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

  $Id: kbgfibs.h,v 1.9 2003/07/01 00:31:37 mueller Exp $

*/


#ifndef __KBGFIBS_H
#define __KBGFIBS_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <generic/kbgengine.h>

#include "kplayerlist.h"
#include "kbgfibschat.h"
#include "kbginvite.h"    // TODO

#include <qstring.h>
#include <qstringlist.h>
#include <qregexp.h>

#include <klocale.h>

class QTimer;
class QSocket;
class QPopupMenu;
class QCheckBox;

class KAction;
class KToggleAction;

/**
 *
 * Special backgammon engine for games on the First Internet Backgammon Server
 *
 * @short The FIBS backgammon engine
 * @author Jens Hoefkens <jens@hoefkens.com>
 *
 */
class KBgEngineFIBS : public KBgEngine
{
	Q_OBJECT

public:

	/**
	 * Constructor
	 */
	KBgEngineFIBS(QWidget *parent = 0, QString *name = 0, QPopupMenu *pmenu = 0);

	/**
	 * Destructor
	 */
	virtual ~KBgEngineFIBS();

	/**
	 * Fills the engine-specific page into the notebook
	 */
	virtual void getSetupPages(KDialogBase *nb);

	virtual void setupOk();
	virtual void setupDefault();
	virtual void setupCancel();

	/*
	 * Check with the engine if we can quit. This may require user
	 * interaction.
	 */
	virtual bool queryClose();

	/**
	 * About to be closed. Let the engine exit properly.
	 */
	virtual bool queryExit();

	virtual void start();


public slots:

	/**
	 * Read and save user settings to the config file
	 */
	virtual void readConfig();
	virtual void saveConfig();

        /**
	 * Roll dice for the player w
	 */
        virtual void rollDice(const int w);

	/**
	 * Double the cube of player w
	 */
	virtual void doubleCube(const int w);

	/**
	 * A move has been made on the board - see the board class
	 * for the format of the string s
	 */
	virtual void handleMove(QString *s);

	/**
	 * Undo the last move
	 */
        virtual void undo();

	/**
	 * Redo the last move
	 */
        virtual void redo();

	/**
	 * Commit a move
	 */
	virtual void done();


	// ###########################################################################
	//
	//
	//
	//                    TODO TODO TODO TODO TODO TODO TODO
	//
	//
	//
	// ###########################################################################

	/*
	 * Process the string cmd
	 */
	void handleCommand(const QString &cmd);

	void fibsRequestInvitation(const QString &player);

	void personalMessage(const QString &msg);



	/*
	 * Local configuration handling
	 */

	void keepAlive();

signals:

	void serverString(const QString &s);

	void fibsWhoInfo(const QString &line);
	void fibsWhoEnd();
	void fibsLogout(const QString &p);
	void fibsLogin(const QString &p);

	void fibsConnectionClosed();

	void changePlayerStatus(const QString &, int, bool);

	void chatMessage(const QString &msg);

	void fibsStartNewGame(const QString &msg);
	void gameOver();

protected slots:

        void invitationDone();
	void inviteDialog();
	void showList();
	void showChat();

	void endGame();

private:

	QTimer *keepaliveTimer;

	QString pname[2];

	QString currBoard, caption;

	//KBgStatus *currBoard
	//KBgFIBSBoard *boardHandler;

	QStringList invitations;

	/*
	 * special menu entries
	 */
	int respMenuID, cmdMenuID, joinMenuID, optsMenuID;
	QPopupMenu *respMenu, *cmdMenu, *joinMenu, *optsMenu;

	/*
	 * child windows
	 */
	KFibsPlayerList *playerlist;
	KBgChat       *chatWindow;
	KBgInvite       *invitationDlg;

	/*
	 * Other stuff
	 */
	QString lastMove;
	int toMove;

	QString lastAway;
	bool playing;
	bool redoPossible;
	int undoCounter;

	KAction *conAction, *disAction, *newAction, *invAction;

	KAction *actAccept, *actReject, *actConti, *actLeave, *actAway, *actBack;

	KToggleAction *chatAct, *listAct;


	// ###########################################################################
	//
	//
	//
	//                    DONE DONE DONE DONE DONE DONE DONE
	//
	//
	//
	// ###########################################################################

private:

	/**
	 * Actions for responding to invitations. numJoin is he current
	 * number of active actions. The max. number of pending invitations
	 * is eight and this is hardcoded in a lot of places (not the least
	 * of which are the slots join_N().
	 */
	KAction *actJoin[8];
	int numJoin;

protected slots:

        /**
	 * Handle rawwho information for the purposes of the invitation
	 * submenu and the join entries
	 */
        void changeJoin(const QString &info);

	/**
	 * A player will be removed from the menu of pending invitations
	 * if necessary.
	 */
	void cancelJoin(const QString &info);

	/**
	 * We have up to 8 names in the join menu. They are the
	 * players that invited us to play games. Each action
	 * has its own slot and all slots call the common backend
	 * join().
	 */
	void join(const QString &msg);

	void join_0();
	void join_1();
	void join_2();
	void join_3();
	void join_4();
	void join_5();
	void join_6();
	void join_7();

	/**
	 * Simple slots that toggle FIBS server-side settings. The
	 * names of the functions reflect the name of the toggle on
	 * FIBS.
	 */
	void toggle_greedy();
	void toggle_ready();
	void toggle_double();
	void toggle_ratings();

private:

	/**
	 * Toggle actions for the FIBS server-side settings
	 */
	enum FIBSOpt {OptReady, OptGreedy, OptDouble,
		      OptAllowPip, OptAutoMove, OptCrawford, OptSilent, OptRatings, OptMoves, NumFIBSOpt};
	KToggleAction *fibsOpt[NumFIBSOpt];

public slots:

        /*
	 * Connection handling
	 * -------------------
	 */

        // initiate asynchronous connection establishment
	void connectFIBS();

	// take the connection down
	void disconnectFIBS();

	// create a new account and connect
	void newAccount();

	// called when the connection is down
	void connectionClosed();

	// the hostname has been resolved
	void hostFound();

	// a connection error occurred
	void connError(int f);

	// connection has been established
	void connected();

	// data can be read from the socket
	void readData();

	// send the string s to the server
	void sendData(const QString &s);

protected:

	// get the connection parameters
	bool queryConnection(const bool newlogin);

private:

	// actual connection object
	QSocket *connection;

	// flag if we have login information or new account
	bool login;

protected slots:

        /*
	 * FIBS command slots
	 * ------------------
	 */

        // go away and leave a message
	void away();

	// come back after being away
	void back();

	// roll dice
        virtual void roll();

	// double the cube
        virtual void cube();

	// reload the board to the last known sane state
	virtual void load();

	// accept an offer
        void accept();

	// reject an offer
	void reject();

	// continue a multi game match
	void match_conti();

	// leave a multi game match
	void match_leave();

protected slots:

	/*
	 * All strings received from the server are given to handleServerData() for
	 * identification and processing. It implements a limited state machine to
	 * handle the incoming data correctly. The whole function could probably be
	 * made more efficient, but it is not time critical (and it appears to be
	 * easier to understand this way).
	 */
        void handleServerData(QString &line);

protected:

	enum RxStatus {RxIgnore, RxConnect, RxWhois, RxMotd, RxRating,
		       RxNewLogin, RxGoodbye, RxNormal};

	int rxStatus, rxCount;

	QString rxCollect;

	/*
	 * The following functions handle the individual states
	 * of the handleServerData() state machine,
	 */
	void handleMessageWhois(const QString &line);
	void handleMessageRating(const QString &line);
	void handleMessageMotd(const QString &line);
	void handleMessageNewLogin(const QString &line);
	void handleMessageConnect(const QString &line, const QString &rawline);
	void handleMessageNormal(QString &line, QString &rawline);

	/*
	 * The next enumeration and the array of regular expressions is needed for the
	 * message identification in handleServerData().
	 */
	enum Pattern {Welcome, OwnInfo, NoLogin, BegRate, EndRate, HTML_lt, HTML_gt,
		      BoardSY, BoardSN, WhoisBG, WhoisE1, WhoisE2, WhoEnde, WhoInfo,
		      MotdBeg, MotdEnd, MsgPers, MsgDeli, MsgSave, ChatSay, ChatSht,
		      ChatWis, ChatKib, SelfSay, SelfSlf, SelfSht, SelfWis, SelfKib,
		      UserLin, UserLot, Goodbye, GameSav, RawBord, YouTurn, PlsMove,
		      BegWtch, EndWtch, BegBlnd, EndBlnd, BegGame, OneWave, TwoWave,
		      YouWave, Reload1, Reload2, GameBG1, GameBG2, GameRE1, GameRE2,
		      GameEnd, EndLose, EndVict, MatchB1, MatchB2, MatchB3, MatchB4,
		      RejAcpt, YouAway, YouAcpt, HelpTxt, Invite0, Invite1, Invite2,
		      Invite3, ConLeav, TabChar, PlsChar, OneName, TypJoin, YouBack,
                      YouMove, YouRoll, TwoStar, BoxHori, BoxVer1, BoxVer2, OthrNam,
                      YourNam, GivePwd, RetypeP, GreedyY, GreedyN, RejCont, AcptWin,
                      YouGive, DoubleY, DoubleN, KeepAlv, RatingY, RatingN,
                      NumPattern};

	QRegExp pat[NumPattern];

	/*
	 * This function is simply filling the pat[] array with the proper values.
	 */
	void initPattern();

private:

	/*
	 * Local setup and config variables
	 * ================================
	 */

	/*
	 * Various options
	 */
	bool showMsg, whoisInvite;
	QCheckBox *cbp, *cbi;

	QCheckBox *cbk;
	bool keepalive;

	/*
	 * Connection setup
	 */
	enum FIBSInfo {FIBSHost, FIBSPort, FIBSUser, FIBSPswd, NumFIBS};
	QString infoFIBS[NumFIBS];
	QLineEdit *lec[NumFIBS];

	/*
	 * Auto messages
	 */
	enum AutoMessages {MsgBeg, MsgLos, MsgWin, NumMsg};
	QLineEdit *lem[NumMsg];
	QCheckBox *cbm[NumMsg];
	bool useAutoMsg[NumMsg];
	QString autoMsg[NumMsg];
};

#endif // __KBGFIBS_H
