/**
 * This file is part of the ksnake package
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
  note: the code to lookup and insert the pixmaps
  into the Options menu was copied and adapted
  from KReversi.
  thanks.
  */
#include <qdir.h>
#include <qregexp.h>


#include <klocale.h>
#include <kconfigdialog.h>

#include <kstdgameaction.h>
#include <kscoredialog.h>
#include <kstatusbar.h>
#include <kmenubar.h>

#include "rattler.h"
#include "game.h"
#include "startroom.h"
#include "levels.h"
#include "progress.h"
#include "view.h"
#include "general.h"
#include "appearance.h"
#include "settings.h"

#define SCORE 1
#define LIVES 2

Game::Game(QWidget *parent, const char *name) :  KMainWindow(parent,name)
{
	// create statusbar
	statusBar()->insertItem(i18n("Score: 0"), SCORE);
	statusBar()->insertItem(i18n("Lives: 0"), LIVES);

	levels = new Levels();

	view = new View(this, "View"); 
	rattler = view->rattler;
	rattler->reloadRoomPixmap();
	rattler->setFocus();

	connect(rattler, SIGNAL(setPoints(int)), this, SLOT(scoreChanged(int)));
	connect(rattler, SIGNAL(setTrys(int)), this, SLOT(setTrys(int)));
	connect(rattler, SIGNAL(rewind()), view->progress, SLOT(rewind()));
	connect(rattler, SIGNAL(advance()), view->progress, SLOT(advance()));
	connect(view->progress, SIGNAL(restart()), rattler, SLOT(restartTimer()));

	connect(rattler, SIGNAL(togglePaused()), this, SLOT(togglePaused()));
	connect(rattler, SIGNAL(setScore(int)), this, SLOT(gameEnd(int)));

	setCentralWidget(view);

	createMenu();
	setupGUI(KMainWindow::Save | StatusBar | Create );
}

Game::~Game()
{
	delete levels;
}

void Game::scoreChanged(int score){
	statusBar()->changeItem(QString(i18n("Score: %1")).arg(score), SCORE);
}

void Game::setTrys(int tries){
	statusBar()->changeItem(QString(i18n("Lives: %1")).arg(tries), LIVES);
}

void Game::gameEnd(int score){
	KScoreDialog di(KScoreDialog::Name | KScoreDialog::Score | KScoreDialog::Date, this);
	KScoreDialog::FieldInfo scoreInfo;
	QString date = QDateTime::currentDateTime().toString();
	scoreInfo.insert(KScoreDialog::Date, date);
	if (di.addScore(score, scoreInfo, true))
		di.exec();
}

void Game::showHighScores()
{
	KScoreDialog d(KScoreDialog::Name | KScoreDialog::Score | KScoreDialog::Date, this);
	d.exec();
}

void Game::createMenu()
{
	actionCollection()->setAutoConnectShortcuts(false);
	(void)new KAction(i18n("Move Up"), Key_Up, 0, 0, actionCollection(), "Pl1Up");
	(void)new KAction(i18n("Move Down"), Key_Down, 0, 0, actionCollection(), "Pl1Down");
	(void)new KAction(i18n("Move Right"), Key_Right, 0, 0, actionCollection(), "Pl1Right");
	(void)new KAction(i18n("Move Left"), Key_Left, 0, 0, actionCollection(), "Pl1Left");
	actionCollection()->setAutoConnectShortcuts(true);
	rattler->setActionCollection(actionCollection());

	KStdGameAction::gameNew(rattler, SLOT(restart()), actionCollection());
	pause = KStdGameAction::pause(rattler, SLOT(pause()), actionCollection());
	KStdGameAction::highscores(this, SLOT(showHighScores()), actionCollection());
	KStdGameAction::quit( this, SLOT(close()), actionCollection());

	KStdAction::preferences(this, SLOT(showSettings()), actionCollection());
	
	// TODO change and make custom function that pauses game or 
	// modify widget to pause when loosing focus and remove this
	KStdAction::keyBindings(guiFactory(), SLOT(configureShortcuts()), 
actionCollection());
}

void Game::togglePaused()
{
	static bool checked = FALSE;
	checked = !checked;
	pause->setEnabled(checked);
}

/**
 * Show Settings dialog.
 */
void Game::showSettings(){
  if(KConfigDialog::showDialog("settings"))
    return;
  
  KConfigDialog *dialog = new KConfigDialog(this, "settings", Settings::self());
  dialog->addPage(new General(0, "General"), i18n("General"), "package_settings");
  
  Appearance *a = new Appearance(0, "Appearance");
  

  QStringList list;

    if (rattler->backgroundPixmaps.count() == 0) {
	list.append(i18n("none"));
    } else {
        QStringList::ConstIterator it = rattler->backgroundPixmaps.begin();
        for(unsigned i = 0; it != rattler->backgroundPixmaps.end(); i++, it++) {
	    // since the filename may contain underscore, they
	    // are replaced with spaces in the menu entry
            QString s = QFileInfo( *it ).baseName();
	    s = s.replace(QRegExp("_"), " ");
	    list.append(s);
	}
    }
  
	a->kcfg_bgimage->insertStringList(list);
  
	dialog->addPage(a, i18n("Appearance"), "style");
  
	dialog->addPage(new StartRoom(0, "StartRoom"), i18n("First Level"), "folder_home");
	connect(dialog, SIGNAL(settingsChanged()), rattler, SLOT(loadSettings()));
	dialog->show();
}

#include "game.moc"
