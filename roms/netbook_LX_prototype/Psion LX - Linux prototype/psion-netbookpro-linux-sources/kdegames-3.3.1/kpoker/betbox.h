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


#ifndef BETBOX_H
#define BETBOX_H

#include <qgroupbox.h>

class QPushButton;


/**
  * This class provides a QGroupBox with several button
  *
  * The bet up / down buttons are used to change the player bet directly,
  *  the adjustBet and out buttons depend on the computers bet
  * @short This is a box with several buttons used to bet / raise
 **/
class BetBox : public QGroupBox
{
  Q_OBJECT

  public:
    BetBox(QWidget* parent = 0, const char* name = 0);
    ~BetBox();


    /**
      * Disables the usual bet up/down buttons and shows the adjust bet and fold buttons
      *
      * Used when the compputer player raised the players bet / raise
     **/
    void beginRaise();

    /**
      * Hides the Adjust bet / Fold buttons and enables the usual bet up / down buttons
     **/
    void stopRaise();

  signals:
    /**
      * This signal is emitted when the user clicks on a bet up / down button
      *
      * The Value of the Button is sent as a parameter (change)
     **/
    void betChanged(int change);

    /**
      * This signal is emitted when the user clicks on the adjust bet button
     **/
    void betAdjusted();

    /**
      * This signal is emitted when the user clicks on the fold button
     **/
    void fold();

  protected slots:
    /**
      * Emits the signal @ref betChanged(5)
     **/
    void bet5UpClicked();

    /**
      * Emits the signal @ref betChanged(10)
     **/
    void bet10UpClicked();

    /**
      * Emits the signal @ref betChanged(-5)
     **/
    void bet5DownClicked();

    /**
      * Emits the signal @ref betChanged(-10)
     **/
    void bet10DownClicked();

    /**
      * Emits the signal @ref betAdjusted()
     **/
    void adjustBetClicked();

    /**
      * Emits the signal @ref fold()
     **/
    void foldClicked();


  private:
    QPushButton  *bet5Up;
    QPushButton  *bet10Up;
    QPushButton  *bet5Down;
    QPushButton  *bet10Down;
    QPushButton  *adjustBet;
    QPushButton  *foldButton;
};


#endif
