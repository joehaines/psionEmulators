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

#ifndef PIXSERVER_H
#define PIXSERVER_H

#include <qpixmap.h>

class Board;
enum SnakePix {TailUp, TailDown, TailRight, TailLeft,
	       HeadUp, HeadDown, HeadRight, HeadLeft,
	       AngleSw, AngleSe, AngleNw, AngleNe,
	       BodyHz, BodyVt,
	       HtailUp, HtailDown, HtailRight, HtailLeft };
enum PixMap { SamyPix, CompuSnakePix, ApplePix, BallPix };
enum Image {Snake, ComputerSnake};

class PixServer
{
public:
    PixServer (Board *);
    QPixmap levelPix() const { return cachePix; }

    void initRoomPixmap();
    void initBrickPixmap();
    void initPixmaps();
    void initbackPixmaps();

    void draw(int pos, PixMap pix, int i = 0);
    void erase(int pos);
    void restore(int pos);

private:
    Board *board;

    void drawBrick(QPainter *, int);

    QPixmap samyPix[18];
    QPixmap compuSnakePix[18];
    QPixmap ballPix[4];
    QPixmap applePix[2];

    QPixmap roomPix;
    QPixmap cachePix;
    QPixmap offPix;
    QPixmap backPix;

    bool plainColor;
    QColor backgroundColor;

};

#endif // PIXSERVER_H

