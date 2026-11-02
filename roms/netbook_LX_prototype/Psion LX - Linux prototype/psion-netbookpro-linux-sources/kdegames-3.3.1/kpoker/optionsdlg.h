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


#ifndef OPTIONSDLG_H
#define OPTIONSDLG_H

#include <kdialogbase.h>


class QLineEdit;
class QCheckBox;
class QLabel;
class QComboBox;

class KIntNumInput;


/**
  * This dialog is will set some options which mostly become active when a new 
  * round is started
  *
  * Here you can set some options like the maxBet, maybe names and so on 
  * @short The options dialog
 **/
class OptionsDlg : public KDialogBase
{
  Q_OBJECT

 public:
  OptionsDlg(QWidget* parent = 0, const char* name = 0, int _players = 1);
  ~OptionsDlg();

  void init(int _drawDelay, int _maxBetOrCashPerRound, int minBet = -1);
  int getMaxBet();
  int getMinBet();
  int getCashPerRound();
  int getDrawDelay();

 private:
  int players;
  int defaultMaxBet;
  int defaultMinBet;
  int defaultCashPerRound;
  int defaultDrawDelay;
  // QLineEdit* maxBet;
  // QLineEdit* minBet;
  // QLineEdit* drawDelay;
  KIntNumInput* maxBet;
  KIntNumInput* minBet;
  KIntNumInput* drawDelay;
};


#endif
