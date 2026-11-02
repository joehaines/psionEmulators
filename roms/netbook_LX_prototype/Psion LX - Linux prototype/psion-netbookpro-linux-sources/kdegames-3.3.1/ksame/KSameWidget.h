/* Yo Emacs, this is -*- C++ -*- */
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

#ifndef _KSAMEWIDGET
#define _KSAMEWIDGET

#include <kmainwindow.h>

class KToggleAction;
class StoneWidget;

class KSameWidget: public KMainWindow {
     Q_OBJECT

public:
     KSameWidget(QWidget *parent=0, const char* name=0, WFlags fl=0);

private slots:
     /* File Menu */
     void m_new();
     void m_restart();
     void m_showhs();
     void m_undo();

     void configureNotifications();

     void sizeChanged();
     void gameover();
     void setColors(int colors);
     void setBoard(int board);
     void setScore(int score);
     void setMarked(int m);
     void stonesRemoved(int, int);
     

protected:
     void newGame(unsigned int board, int colors);

     virtual void saveProperties(KConfig *conf);
     virtual void readProperties(KConfig *conf);

     bool confirmAbort();

private:
     // Remember how many stones were last marked, since when
     // the stones are removed, board->marked() suddenly becomes
     // 0 (or doesn't otherwise reflect the number of stones just removed).
     int m_markedStones;

     StoneWidget *stone;
     KStatusBar *status;
     KToggleAction *random;
     KToggleAction *showNumberRemaining;
     KAction *restart;
     KAction *undo;

};

#endif // _KSAMEWIDGET

