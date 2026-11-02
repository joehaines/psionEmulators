/*
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */


#ifndef __PokerWindow__
#define __PokerWindow__


#include <kmainwindow.h>


class QLabel;
class KToggleAction;
class kpok;


class PokerWindow : public KMainWindow
{
  Q_OBJECT

  public:
    PokerWindow();
    ~PokerWindow();

  protected:
    virtual bool queryClose();
    bool  eventFilter(QObject*, QEvent*);
    void  initKAction();
    void  readOptions();

  protected slots:
    //     void saveProperties(KConfig*);
    //     void readProperties(KConfig*);
    void  setHand(QString newHand, bool lastHand = true);
    void  showClickToHold(bool show);
    void  statusBarMessage(QString);
    void  clearStatusBar();
    void  saveOptions();
    void  toggleMenubar();
    void  toggleStatusbar();

  private:
    kpok           *m_kpok;

    KToggleAction  *soundAction;
    KToggleAction  *blinkingAction;
    KToggleAction  *adjustAction;
    KToggleAction  *showMenubarAction;
    KToggleAction  *showStatusbarAction;

    // statusbar elements:
    QLabel         *LHLabel;

    bool            clickToHoldIsShown;
};

#endif
