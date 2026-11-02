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

#include <qbitmap.h>
#include <qimage.h>

#include "level.h"
#include "board.h"
#include "bitmaps.h"
#include "levels.h"

const uchar *numbers[10] = { zero_bits, one_bits, two_bits,
				 three_bits, four_bits, five_bits,
				 six_bits, seven_bits, eight_bits, nine_bits
};

Level::Level(Board *b)
{
	board = b;
	create(Intro);
}

void Level::nextLevel()
{
	if (level < leV->max())
		level++;
}

void Level::create(Img img)
{
	switch(img){
		case Banner:
			createBanner();
		break;
		case Room:
			createRoom();
		break;
		case Intro:
			makeImageFromData(intro_bits);
		break;
		case GameOver:
			makeImageFromData(gameover_bits);
		break;
	}
}

void Level::createRoom()
{
	QImage image = leV->getImage(level);
	initBoard(image);
}

void Level::makeImageFromData(const uchar *buf)
{
	QBitmap bitmap(BoardWidth, BoardWidth, buf, TRUE);
	QImage image = bitmap.convertToImage();
	initBoard (image);
}

void Level::initBoard(const QImage &image)
{
	int index = 0;
	uchar *b;

	for ( int y = 0;y < image.height();y++ ) {
		b = image.scanLine(y);
		for ( int x = 0;x < image.width();x++ ) {

			if ( image.bitOrder() == QImage::BigEndian ) {
				if (((*b >> (7 - (x & 7))) & 1) == 1)
					board->set(index, brick);
				else board->set(index, empty);
				} else {
				if (((*b >> (x & 7)) & 1) == 1)
					board->set(index, brick);
				else board->set(index, empty);
			}

			if ( (x & 7) == 7 )
				b++;
			index++;
		}
	}
}

void Level::createBanner()
{
	makeImageFromData(level_bits);

	QString num;
	num.setNum(level);
	if(level < 10)
		num.insert(0,'0');

	QString left = num.left(1);
	QString right = num.right(1);

	drawNumber ( 606, numbers[left.toInt()] );
	drawNumber ( 614, numbers[right.toInt()] );
}

void Level::drawNumber(int beginAt, const uchar *buf)
{
	QBitmap bitmap(7, 9, buf, TRUE);
	QImage image = bitmap.convertToImage();

	int index = beginAt;
	uchar *b;

	for ( int y = 0;y < image.height();y++ )
	{
		b = image.scanLine(y);
		for ( int x = 0;x < image.width();x++ )
		{
			if ( image.bitOrder() == QImage::BigEndian )
			{
				if (((*b >> (7 - (x & 7))) & 1) == 1)
					board->set(index, brick);
			} else {
				if (((*b >> (x & 7)) & 1) == 1)
					board->set(index, brick);
			}
			if ( (x & 7) == 7 )
				b++;
			index++;
		}
		index += 28;
	}
}

