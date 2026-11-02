/*
 *   ksame 0.4 - simple Game
 *   Copyright (C) 1997,1998  Marcus Kreutzberger
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */
#include <stdio.h>

#include <qwidget.h>

#include <kapplication.h>
#include <klocale.h>
#include <kcmdlineargs.h>

#include "version.h"
#include "KSameWidget.h"
#include <kaboutdata.h>
#include <kglobal.h>

static const char description[] = I18N_NOOP("Same Game - a little game about balls and how to get rid of them");
static const char copyright[] = "(c) 1997-1998 Marcus Kreutzberger";

int main( int argc, char **argv ) {
	KAboutData aboutData("ksame", I18N_NOOP("SameGame"), KSAME_VERSION,
                         description, KAboutData::License_GPL, copyright);
    aboutData.addAuthor("Marcus Kreutzberger", 0, "kreutzbe@informatik.mu-luebeck.de");
    KCmdLineArgs::init(argc, argv, &aboutData);

    KApplication::setColorSpec(QApplication::ManyColor+QApplication::CustomColor);
    KApplication a;
    KGlobal::locale()->insertCatalogue("libkdegames");

    KSameWidget *w = new KSameWidget;
    if (kapp->isRestored()) {
        if (KSameWidget::canBeRestored(1))
            w->restore(1);
    } else
        w->show();
    a.setMainWidget(w);

    return a.exec();
}
