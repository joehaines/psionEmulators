/****************************************************************
Copyright (c) 1998 Sandro Sigala <ssigala@globalnet.it>.
All rights reserved.

Permission to use, copy, modify, and distribute this software
and its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
copies and that both that the copyright notice and this
permission notice and warranty disclaimer appear in supporting
documentation, and that the name of the author not be used in
advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

The author disclaim all warranties with regard to this
software, including all implied warranties of merchantability
and fitness.  In no event shall the author be liable for any
special, indirect or consequential damages or any damages
whatsoever resulting from loss of use, data or profits, whether
in an action of contract, negligence or other tortious action,
arising out of or in connection with the use or performance of
this software.
****************************************************************/

#include "config.h"

#include <kapplication.h>
#include <klocale.h>
#include <kconfig.h>
#include <kmenubar.h>
#include <kstatusbar.h>

#include "ksmiletris.h"
#include "gamewindow.h"
#include "gamewidget.h"
#include "kscoredialog.h"

#include <kaction.h>
#include <kstdgameaction.h>



const int default_width = 362;
const int default_height = 460;

GameWindow::GameWindow(QWidget *, const char *name)
        : KMainWindow(0, name)
{
        //New Games
        (void)KStdGameAction::gameNew(this,
                                  SLOT(menu_newGame()),
                                  actionCollection());

        //Pause Game
        (void)KStdGameAction::pause(this,
                                      SLOT(menu_pause()),
                                      actionCollection());

        //End Game
        (void)KStdGameAction::end(this,
                                  SLOT(menu_endGame()),
                                  actionCollection());

        //Highscores
        (void)KStdGameAction::highscores(this,
                                    SLOT(menu_highScores()),
                                    actionCollection());

        //Quit
        (void)KStdGameAction::quit(this,
                               SLOT(close()),
                               actionCollection());

        QStringList list;
        KSelectAction* piecesAct =
          new KSelectAction(i18n("&Pieces"), 0, this, SLOT(menu_pieces()),
                             actionCollection(), "settings_pieces");
        list.append(i18n("&Smiles"));
        list.append(i18n("S&ymbols"));
        list.append(i18n("&Icons"));
        piecesAct->setItems(list);

        (void)new KToggleAction(i18n("&Sounds"), 0, this,
		SLOT(menu_sounds()), actionCollection(), "settings_sounds");





	//connect(menu, SIGNAL(moved(menuPosition)),
	//	this, SLOT(movedMenu(menuPosition))); ?

	status = new KStatusBar(this);
	status->insertItem(i18n("Level: 99"), 1);
	status->insertItem(i18n("Score: 999999"), 2);
	status->changeItem("", 1);
	status->changeItem("", 2);

	game = new GameWidget(this);
	setCentralWidget(game);
	connect(game, SIGNAL(changedStats(int, int)),
		this, SLOT(updateStats(int, int)));
	connect(game, SIGNAL(gameOver()), this, SLOT(gameOver()));

        //keys
        (void)new KAction(i18n("Move Left"), Key_Left, game, SLOT(keyLeft()), actionCollection(), "left");
        (void)new KAction(i18n("Move Right"), Key_Right, game, SLOT(keyRight()), actionCollection(), "right");
        (void)new KAction(i18n("Rotate Left"), Key_Up, game, SLOT(keyUp()), actionCollection(), "up");
        (void)new KAction(i18n("Rotate Right"), Key_Down, game, SLOT(keyDown()), actionCollection(), "down");
        (void)new KAction(i18n("Drop Down"), Key_Space, game, SLOT(keySpace()), actionCollection(), "space");


        //Original
        //resize(default_width, default_height + menu->height() + status->height());
        //setMaximumSize(width(), height());
	//setMinimumSize(width(), height());


        resize(default_width, default_height + KMainWindow::menuBar()->height() + status->height());
	setMaximumSize(width(), height());
	setMinimumSize(width(), height());

	// Read configuration
	KConfig *config = kapp->config();
	config->setGroup("Options");
	PiecesType pieces_type = (PiecesType)config->readNumEntry("Pieces", static_cast<int>(Pieces_Smiles));
	game->setPieces(pieces_type);
        ((KSelectAction*)actionCollection()->action("settings_pieces"))->setCurrentItem((int)pieces_type);


	game->do_sounds = config->readBoolEntry("Sounds", true);
        ((KToggleAction*)actionCollection()->action("settings_sounds"))->setChecked(game->do_sounds);

	setupGUI(KMainWindow::Save | Keys | StatusBar | Create);
}

void GameWindow::menu_newGame()
{
        ((KToggleAction*)actionCollection()->action(KStdGameAction::stdName(KStdGameAction::Pause)))->setChecked(false);
	game->newGame();
}

void GameWindow::menu_pause()
{
	if (game->in_game) {
		game->in_pause = !game->in_pause;
                ((KToggleAction*)actionCollection()->action(KStdGameAction::stdName(KStdGameAction::Pause)))->setChecked(game->in_pause);
		game->repaintChilds();
	}
}

void GameWindow::menu_endGame()
{
	if (game->in_game) {
		game->in_game = false;
		game->repaintChilds();
                ((KToggleAction*)actionCollection()->action(KStdGameAction::stdName(KStdGameAction::Pause)))->setChecked(false);
		gameOver();
	}
}

void GameWindow::menu_highScores()
{
    KScoreDialog d(KScoreDialog::Name | KScoreDialog::Level | KScoreDialog::Score, this);
    d.exec();
}

void GameWindow::menu_pieces()
{
  int index = ((KSelectAction*)actionCollection()->action("settings_pieces"))->currentItem();
  game->setPieces((PiecesType)index);

  KConfig *config = kapp->config();
  config->setGroup("Options");
  config->writeEntry("Pieces", index);
}

void GameWindow::menu_sounds()
{
	game->do_sounds = !game->do_sounds;
        ((KToggleAction*)actionCollection()->action("settings_sounds"))->setChecked(game->do_sounds);

        KConfig *config = kapp->config();
	config->setGroup("Options");
	config->writeEntry("Sounds", game->do_sounds);
}

void GameWindow::updateStats(int level, int points)
{
	QString l, p;
	l.setNum(level);
	p.setNum(points);
	status->changeItem(i18n("Level: %1").arg(l), 1);
	status->changeItem(i18n("Score: %1").arg(p), 2);
}

void GameWindow::gameOver()
{
    KScoreDialog d(KScoreDialog::Name | KScoreDialog::Level | KScoreDialog::Score, this);

    KScoreDialog::FieldInfo scoreInfo;

    scoreInfo[KScoreDialog::Level].setNum(game->num_level);

    if (!d.addScore(game->num_points, scoreInfo))
       return;

    // Show highscore & ask for name.
    d.exec();
}


/* ?
void GameWindow::movedMenu(menuPosition pos)
{
	if (pos == Top) {
		setMinimumSize(default_width, default_height + menu->height() + status->height());
		setMaximumSize(default_width, default_height + menu->height() + status->height());
		resize(width(), height() + menu->height());
	} else {
		setMinimumSize(default_width, default_height + status->height());
		setMaximumSize(default_width, default_height + status->height());
		resize(width(), height() - menu->height());
	}
}

*/

#include "gamewindow.moc"
