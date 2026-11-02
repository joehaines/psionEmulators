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

#ifndef LEVEL_H
#define LEVEL_H

class Board;

enum Img {Banner, Room, Intro, GameOver};

class Level {

public:
	Level (Board *);
	void nextLevel();
	void setLevel(int level) { this->level = level; }
	int	getLevel() const { return level; }
	void create(Img boardType);

private:
	Board *board;
	QImage makeImage(char *);
	void makeImageFromData(const uchar *buf);
	void drawNumber(int beginAt, const uchar *buf);
	void initBoard(const QImage &image);
	void createRoom();
	void createBanner();

	// Can level be negative?
	int level;
};

#endif // LEVEL_H
