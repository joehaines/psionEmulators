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


// QT includes
#include <qlabel.h>
#include <qlayout.h>

// KDE includes
#include <klocale.h>
#include <knuminput.h>

// own includes
#include "optionsdlg.h"
#include "defines.h"


OptionsDlg::OptionsDlg(QWidget* parent, const char* name, int _players) 
		: KDialogBase(Plain, i18n("Options")/*?*/, Ok|Cancel, Ok, 
		parent, name, true, true)
{
  QVBoxLayout* topLayout = new QVBoxLayout(plainPage(), spacingHint());
  maxBet = 0;
  minBet = 0;

  if (_players <= 0)
    players = DEFAULT_PLAYERS;
  else
    players = _players;
 
  topLayout->addWidget(new QLabel(i18n("All changes will be activated in the next round."), plainPage()));

  drawDelay = new KIntNumInput(0, plainPage());
  drawDelay->setLabel(i18n("Draw delay:"));
  topLayout->addWidget(drawDelay);

  if (players > 1) {
    maxBet = new KIntNumInput(0, plainPage());
    maxBet->setLabel(i18n("Maximal bet:"));
    topLayout->addWidget(maxBet);
	 
    minBet = new KIntNumInput(0, plainPage());
    minBet->setLabel(i18n("Minimal bet:"));
    topLayout->addWidget(minBet);
  }
}


OptionsDlg::~OptionsDlg()
{
}


void OptionsDlg::init(int _drawDelay, int _maxBetOrCashPerRound, int _minBet)
{
  if (_minBet < 0)
    defaultMinBet = MIN_BET;
  if (_maxBetOrCashPerRound < 0 && players > 1)
    defaultMaxBet = MAX_BET;
  else if (_maxBetOrCashPerRound < 0)
    defaultCashPerRound = CASH_PER_ROUND;
  if (_drawDelay < 0)
    _drawDelay = DRAWDELAY;

  drawDelay->setValue(_drawDelay);
  if (maxBet)
    maxBet->setValue(_maxBetOrCashPerRound);
  if (minBet && players > 1)
    minBet->setValue(_minBet);
}


int OptionsDlg::getMaxBet()
{
  if (!maxBet || players <= 1)
    return defaultMaxBet;

  return maxBet->value();
}


int OptionsDlg::getMinBet()
{
  if (!minBet || players <= 1)
    return defaultMinBet;

  return minBet->value();
}


int OptionsDlg::getCashPerRound()
{
  if (!maxBet || players > 1)
    return defaultCashPerRound;

  return maxBet->value();
}


int OptionsDlg::getDrawDelay()
{
  return drawDelay->value();
}


#include "optionsdlg.moc"
