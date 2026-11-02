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

#ifndef GAME_H
#define GAME_H

#include <kmainwindow.h>

class KAction;
class Rattler;
class Levels;
class View;

class Game : public KMainWindow  {

Q_OBJECT

public:
	Game(QWidget *parent=0, const char *name=0);
	~Game();

private slots:
	void gameEnd(int score);
	void showHighScores();

	void showSettings();

	void togglePaused();
	void scoreChanged(int newScore);
	void setTrys(int);
	
private:
	View *view;
	Rattler *rattler;
	Levels *levels;
	KAction *pause;

	void createMenu();
};

#endif // GAME_H

