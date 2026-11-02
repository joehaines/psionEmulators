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

#include <qimage.h>
#include <qbitmap.h>

#include <kstandarddirs.h>

#include "levels.h"

Levels *leV = 0;

Levels::Levels()
{
    leV = this;
    list = KGlobal::dirs()->findAllResources("appdata", "levels/*");    
    list.prepend( "dummy" );
}

int Levels::max()
{
    return ( list.count() -1 );
}

QImage Levels::getImage(int at)
{
    QBitmap bitmap(*list.at(at));
    QImage image = bitmap.convertToImage();
    return image;
}

QPixmap Levels::getPixmap(int at)
{
    return QPixmap(*list.at(at));
}

