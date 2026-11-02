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


#ifndef NEWGAMEDLG_H
#define NEWGAMEDLG_H


#include <kdialogbase.h>


class KIntNumInput;

class QLineEdit;
class QCheckBox;
class QLabel;
class QComboBox;


/**
  * This dialog is shown on startup or when a new game is started
  *
  * Here you can set some options like the default money, players, the names and so on
  * @short The dialog shown on startup or when a new game is started
 **/
class NewGameDlg : public KDialogBase
{
  Q_OBJECT

 public:
  NewGameDlg(QWidget* parent = 0);
  ~NewGameDlg();

  /**
   * @return The player number the user has set
   **/
  int  getPlayers();

  /**
   * @return True if the user wants to read options from the config file false if not
   **/
  bool readFromConfigFile();

  /**
   * You can write the return into a config file which choses to start this dialog on startup
   * @return True if the user wants this dialog to be shown on startup
   **/
  bool showOnStartup();

  /**
   * @return The start money of all players
   **/
  int money();

  /**
   * @param nr The number of the player
   * @return The name of the player specified in nr
   **/
  QString name(int nr);

  /**
   * This method hides the button where the user can choose to read values from config file
   *
   * It is used when a new game is started - you can load with config file on startup only
   **/
  void hideReadingFromConfig();

  void setPlayerNames(int no = -1, QString playerName = 0);


 protected slots:
    /**
     * This slot hides the readFromConfigLabel when the user does not want to read from config or shows it if the user does
     *
     * The label warns that the other values are used as default values if reading from config fails
     **/
    void changeReadFromConfig( bool );


 private:
  QLabel        *readFromConfigLabel;
  QCheckBox     *readFromConfig;
  KIntNumInput  *players;
  QLineEdit     *moneyOfPlayers;
  QCheckBox     *showDialogOnStartup;
  QLineEdit     *player1Name;
  QComboBox     *computerNames;
};


#endif
