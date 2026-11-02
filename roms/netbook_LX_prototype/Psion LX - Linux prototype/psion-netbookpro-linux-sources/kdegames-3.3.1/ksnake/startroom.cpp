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
#include "startroom.h"

#include <qlabel.h>
#include <qpixmap.h>
#include <qwmatrix.h>
#include <qspinbox.h>
#include <klocale.h>
#include <qlayout.h>

#include "levels.h"

StartRoom::StartRoom( QWidget *parent, const char *name)
    : QWidget( parent, name )
{
    QGridLayout *Form1Layout = new QGridLayout( this, 1, 1, 11, 6, "Form1Layout"); 
    QSpacerItem* spacer = new QSpacerItem( 20, 61, QSizePolicy::Minimum, QSizePolicy::Expanding );
    Form1Layout->addItem( spacer, 2, 1 );

    QHBoxLayout *layout1 = new QHBoxLayout( 0, 0, 6, "layout1"); 
    QSpacerItem* spacer_2 = new QSpacerItem( 91, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
    layout1->addItem( spacer_2 );

    picture = new QLabel( this, "picture" );
    layout1->addWidget( picture );
    QSpacerItem* spacer_3 = new QSpacerItem( 41, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
    layout1->addItem( spacer_3 );

    Form1Layout->addMultiCellLayout( layout1, 0, 0, 0, 1 );

    roomRange = new QSpinBox( this, "kcfg_StartingRoom" );
    roomRange->setMaxValue( 25 );
    roomRange->setMinValue( 1 );

    Form1Layout->addWidget( roomRange, 1, 1 );

    QLabel *textLabel = new QLabel( this, "textLabel" );
    textLabel->setText(i18n("First level:"));
    Form1Layout->addWidget( textLabel, 1, 0 );

    connect( roomRange, SIGNAL(valueChanged(int)), SLOT(loadLevel(int)));
    loadLevel(1);
}

void StartRoom::loadLevel(int level)
{
	if(level < 1 || level > leV->max())
		return;

	QPixmap pixmap = leV->getPixmap(level);
	QWMatrix m;
	m.scale( (double)7, (double)7 );
	pixmap = pixmap.xForm( m );
	picture->setPixmap(pixmap);
}

#include "startroom.moc"

