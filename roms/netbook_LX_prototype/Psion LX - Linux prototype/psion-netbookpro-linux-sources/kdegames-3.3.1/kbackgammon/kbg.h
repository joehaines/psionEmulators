/* Yo Emacs, this -*- C++ -*-

  Copyright (C) 1999-2001 Jens Hoefkens
  hoefkens@pilot.msu.edu

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

  $Id: kbg.h,v 1.16 2004/05/22 23:26:49 bmeyer Exp $

*/

#ifndef __KBG_H
#define __KBG_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <kmainwindow.h>

class QSplitter;
class QCheckBox;
class QPopupMenu;
class QLabel;
class KAction;
class KSelectAction;
class KLineEdit;
class KDialogBase;
class KDoubleNumInput;

class KBgEngine;
class KBgTextView;
class KBgBoardSetup;


class KBg : public KMainWindow
{
	Q_OBJECT

public:

	/**
	 * Constructor creates the full main window
	 */
	KBg();

	/**
	 * Destructor
	 */
	virtual ~KBg();

	/**
	 * Read various settings from the configuration files or
	 * set some reasonable defaults
	 */
	void readConfig();

public slots:

         /**
	 * Set the caption to KFIBS_NAME + string + pipcount (if requested by
	 * the user)
	 */
	void updateCaption(const QString &s);

	/**
	 * Slot to be called by the engine - it enables/disables buttons
	 * in the button bar
	 */
	void allowCommand(int cmd, bool f);

	/**
	 * Sets the backgammon engine to type
	 */
	void setupEngine();

	void startKCM(const QString &);

signals:

	/**
	 * Tell all listeners to write their settings to disk
	 */
	void saveSettings();

	/**
	 * Tell all listeners to restore their settings or use reasonable
	 * defaults
	 */
	void readSettings();

protected:

	/*
	 * Windows are to be hidden
	 */
	virtual void hideEvent(QHideEvent *);

	/*
	 * Redisplay the windows
	 */
	virtual void showEvent(QShowEvent *);

	/*
	 * Called before the window is closed. Check with the engine
	 * if that is okay.
	 */
	virtual bool queryClose();

protected slots:

        /**
	 * Show the button bar - or not - depending on the corresponding action
	 */
    void toggleMenubar();

	void configureToolbars();
	void newToolbarConfig();

	/**
	 * Starts the print dialog and asks the board to print itself
	 */
	void print();

	void openNew();

	/**
	 * Takes text from the commandline and hands it over to the
	 * current engine
	 */
	void handleCmd(const QString &);

	/**
	 * Saves the user settings to disk
	 */
	void saveConfig();

	/**
	 * Slots for the respective actions - called by the button bar
	 * and some global key shortcuts
	 */
	void undo();
	void redo();
	void roll();
	void cube();
	void load();
	void done();

	/**
	 * Opens and displays the respective home pages
	 */
	void showWWW(int t);

	void wwwFIBS();
	void wwwHome();
	void wwwRule();

	/**
	 * Show the big setup dialog
	 */
	void setupDlg();

	/**
	 * Save the settings
	 */
	void setupOk();

	/**
	 * Delete the setup dialog
	 */
	void setupDone();

	/**
	 * Load default values for user settings
	 */
	void setupDefault();

	/**
	 * Undo the settings
	 */
	void setupCancel();

private:
    KAction *newAction, *undoAction, *redoAction, *rollAction, *cubeAction, *endAction, *loadAction;

	/*
	 * Each engine has its own identifier.
	 */
	enum Engines {None = -1, Offline, FIBS, GNUbg, NextGen, MaxEngine};
	QString engineString[MaxEngine];
	KBgEngine *engine[MaxEngine];
	int currEngine;

	QPopupMenu *dummyPopup;
	enum HelpTpoics {FIBSHome, RuleHome, SelfHome, MaxHelpTopic};
	QString helpTopic[MaxHelpTopic][2];
	KSelectAction *engineSet;

	/**
	 * Notebook for the setup
	 */
	KDialogBase *nb;
	KDoubleNumInput *sbt;
	QCheckBox *cbt, *cbs, *cbm;

	/*
	 * UI elements
	 */
	QSplitter     *panner;
	KBgBoardSetup *board;
	KBgTextView   *status;
	KLineEdit     *cmdLine;
	QLabel        *cmdLabel;
	QString       baseCaption; // for user friendly printing, we keep it around
};

#endif // __KBG_H
