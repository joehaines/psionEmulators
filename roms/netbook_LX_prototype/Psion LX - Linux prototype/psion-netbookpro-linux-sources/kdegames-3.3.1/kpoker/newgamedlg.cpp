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
#include <qlineedit.h>
#include <qcheckbox.h>
#include <qcombobox.h>
#include <qlayout.h>

// KDE includes
#include <klocale.h>
#include <knuminput.h>
#include <kapplication.h>
#include <kconfig.h>
#include <knumvalidator.h>

// own includes
#include "defines.h"
#include "newgamedlg.h"


NewGameDlg::NewGameDlg(QWidget* parent)
  : KDialogBase(Plain, i18n("New Game"), 
		Ok|Cancel, Ok, parent, 0, true, true)
{
  QVBoxLayout  *topLayout = new QVBoxLayout(plainPage(), spacingHint());
  QHBoxLayout  *l         = new QHBoxLayout(topLayout);

  KConfig* conf = kapp->config();
  conf->setGroup("NewGameDlg");
  bool showNewGameDlg = conf->readBoolEntry("showNewGameDlgOnStartup", 
					    SHOWNEWGAME_DEFAULT);
  bool readConfig     = conf->readBoolEntry("readFromConfig", 
					    LOADGAME_DEFAULT);
  int playerNr        = conf->readNumEntry("players", DEFAULT_PLAYERS);
  int money           = conf->readNumEntry("startMoney", START_MONEY);
 
  readFromConfig = new QCheckBox(i18n("Try loading a game"), plainPage());
  readFromConfig->adjustSize();
  readFromConfig->setChecked(readConfig);
  l->addWidget(readFromConfig);

  readFromConfigLabel = new QLabel(i18n("The following values are used if loading from config fails"), plainPage());
  if (!readFromConfig->isChecked())
    readFromConfigLabel->hide();
  readFromConfigLabel->adjustSize();
  l->addWidget(readFromConfigLabel);
  connect(readFromConfig, SIGNAL(toggled(bool)), 
	  this,           SLOT(changeReadFromConfig(bool)));

  players = new KIntNumInput(playerNr, plainPage());
  players->setRange(1, MAX_PLAYERS);
  players->setLabel(i18n("How many players do you want?"));
  topLayout->addWidget(players);

  l = new QHBoxLayout(topLayout);
  l->addWidget(new QLabel(i18n("Your name:"), plainPage()));
  player1Name = new QLineEdit(plainPage());
  l->addWidget(player1Name);

  l = new QHBoxLayout(topLayout);
  l->addWidget(new QLabel(i18n("Players' starting money:"), plainPage()));
  moneyOfPlayers = new QLineEdit(QString("%1").arg(money), plainPage());
  moneyOfPlayers->setValidator( new KIntValidator( 0,999999,moneyOfPlayers ) );

  l->addWidget(moneyOfPlayers);

  l = new QHBoxLayout(topLayout);
  l->addWidget(new QLabel(i18n("The names of your opponents:"), plainPage()));
  computerNames = new QComboBox(true, plainPage());
  computerNames->setInsertionPolicy(QComboBox::AtCurrent);
  l->addWidget(computerNames);

  l = new QHBoxLayout(topLayout);
  l->addWidget(new QLabel(i18n("Show this dialog every time on startup"), 
			  plainPage()));
  showDialogOnStartup = new QCheckBox(plainPage());
  showDialogOnStartup->setChecked(showNewGameDlg);
  l->addWidget(showDialogOnStartup);

  setPlayerNames();
}


NewGameDlg::~NewGameDlg()
{
  if (result() == Accepted) {
    KConfig* conf = kapp->config(); 
    conf->setGroup("NewGameDlg"); // defaults for the newGameDlg only
    conf->writeEntry("showNewGameDlgOnStartup", showOnStartup());
    conf->writeEntry("readFromConfig", readFromConfigFile()); // just a default!
    conf->writeEntry("players", getPlayers());
    conf->writeEntry("startMoney", money());
  }

  //delete the visible elements:
  delete readFromConfigLabel;
  delete readFromConfig;
  delete players;
  delete moneyOfPlayers;
  delete showDialogOnStartup;
  delete player1Name;
  delete computerNames;

}


void NewGameDlg::setPlayerNames(int no, QString playerName)
{
  if (no < 0) {
    kapp->config()->setGroup("Save");
    player1Name->setText(kapp->config()->readEntry("Name_0", i18n("You")));
    computerNames->clear();
    for (int i = 1; i < MAX_PLAYERS; i++) {
      computerNames->insertItem(kapp->config()->readEntry(QString("Name_%1").arg(i), i18n("Computer %1").arg(i)));
    }
  } else if (no == 0) {
    player1Name->setText(playerName);
  } else {
    if (computerNames->count() > no)
      computerNames->insertItem(playerName, no-1);
    else
      computerNames->changeItem(playerName, no-1);
  }
}


void NewGameDlg::changeReadFromConfig(bool show)
{
  if (show)
    readFromConfigLabel->show();
  else
    readFromConfigLabel->hide();
}


bool NewGameDlg::showOnStartup()
{
  return showDialogOnStartup->isChecked();
}


int NewGameDlg::getPlayers()
{
  return players->value();
}


bool NewGameDlg::readFromConfigFile()
{
  return readFromConfig->isChecked(); 
}


int NewGameDlg::money()
{
  bool  ok    = true;
  int   money = moneyOfPlayers->text().toInt(&ok);
  if (ok) 
    return money;
  else
    return START_MONEY;
}


QString NewGameDlg::name(int nr)
{
  if (computerNames->currentText() != computerNames->text(computerNames->currentItem()))
    computerNames->changeItem(computerNames->currentText(), computerNames->currentItem());
	
  if (nr == 0)
    return player1Name->text();

  if (nr <= computerNames->count())
    return computerNames->text(nr-1);

  return i18n("Player");
}


void NewGameDlg::hideReadingFromConfig()
{
  readFromConfig->hide();
  readFromConfigLabel->hide();
  readFromConfig->setChecked(false);
}


#include "newgamedlg.moc"
