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


#include <qpushbutton.h>
#include <qlayout.h>

#include <kglobal.h>
#include <klocale.h>

#include "betbox.h"


BetBox::BetBox(QWidget* parent, const char* name)
  : QGroupBox(parent, name)
{
  QVBoxLayout* topLayout = new QVBoxLayout(this, 1, 1);
  QGridLayout* g = new QGridLayout(topLayout, 2, 2, 1);
  QHBoxLayout* l = new QHBoxLayout(topLayout, 1);

  bet5Up = new QPushButton(this);
  g->addWidget(bet5Up, 0, 0);
  bet10Up = new QPushButton(this);
  g->addWidget(bet10Up, 0, 1);
  bet5Down = new QPushButton(this);
  g->addWidget(bet5Down, 1, 0);
  bet10Down = new QPushButton(this);
  g->addWidget(bet10Down, 1, 1);

  adjustBet = new QPushButton(this);
  l->addWidget(adjustBet, 0);
  l->addStretch(1);
  foldButton = new QPushButton(this);
  l->addWidget(foldButton, 0);

  bet5Up->setText(QString("+%1").arg(KGlobal::locale()->formatMoney(5)));
  bet10Up->setText(QString("+%1").arg(KGlobal::locale()->formatMoney(10)));
  bet5Down->setText(QString("-%1").arg(KGlobal::locale()->formatMoney(5)));
  bet10Down->setText(QString("-%1").arg(KGlobal::locale()->formatMoney(10)));
  adjustBet->setText(i18n("Adjust Bet"));
  foldButton->setText(i18n("Fold"));

  //connects
  connect(bet5Up,     SIGNAL(clicked()), SLOT(bet5UpClicked()));
  connect(bet10Up,    SIGNAL(clicked()), SLOT(bet10UpClicked()));
  connect(bet5Down,   SIGNAL(clicked()), SLOT(bet5DownClicked()));
  connect(bet10Down,  SIGNAL(clicked()), SLOT(bet10DownClicked()));
  connect(foldButton, SIGNAL(clicked()), SLOT(foldClicked()));
  connect(adjustBet,  SIGNAL(clicked()), SLOT(adjustBetClicked()));

  stopRaise();
}


BetBox::~BetBox()
{
  delete bet5Up;
  delete bet10Up;
  delete bet5Down;
  delete bet10Down;
  delete adjustBet;
  delete foldButton;
}


void BetBox::bet5UpClicked()
{
  emit betChanged(5);
}

void BetBox::bet10UpClicked()
{
  emit betChanged(10);
}

void BetBox::bet5DownClicked()
{
  emit betChanged(-5);
}

void BetBox::bet10DownClicked()
{
  emit betChanged(-10);
}


void BetBox::adjustBetClicked()
{
  emit betAdjusted();
}


void BetBox::foldClicked()
{
  emit fold();
}


void BetBox::beginRaise()
{
  adjustBet->setEnabled(true);
  foldButton->setEnabled(true);

  bet5Up->setEnabled(false);
  bet10Up->setEnabled(false);
  bet5Down->setEnabled(false);
  bet10Down->setEnabled(false);
}


void BetBox::stopRaise()
{
  adjustBet->setEnabled(false);
  foldButton->setEnabled(false);
 
  bet5Up->setEnabled(true);
  bet10Up->setEnabled(true);
  bet5Down->setEnabled(true);
  bet10Down->setEnabled(true);
}


#include "betbox.moc"
