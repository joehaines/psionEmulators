/* Class AbTop: the toplevel widget of Kenolaba
 *
 * Josef Weidendorfer, 9/97
*/

#ifndef _ABTOP_H_
#define _ABTOP_H_

#include <kmainwindow.h>

#include "Move.h"


class QTimer;
class QPopupMenu;
class QLabel;

class KAction;
class KToggleAction;
class KSelectAction;

class Network;
class Board;
class BoardWidget;
class Move;
class EvalScheme;

#ifdef SPION
class Spy;
#endif



class AbTop: public KMainWindow
{
  Q_OBJECT

public:
  AbTop();
  ~AbTop();

  /* timer states */
  enum { noGame, gameOver, notStarted, moveShown,
	 showMove = 100, showSugg=200
  };

  void netPort(int p) { myPort = p; }
  void netHost(char* h) { hosts.append(h); }

protected:
  virtual void saveProperties( KConfig * );
  virtual void readProperties( KConfig * );


public slots:
  void timerDone();
  void newGame();
  void copy();
  void paste();
  void pastePosition(const char *);
  void stopGame();
  void continueGame();
  bool queryClose();
  void back();
  void forward();
  void suggestion();
  void stopSearch();
  void searchBreak();
  void moveChoosen(Move&);
  void savePosition();
  void restorePosition();
  void setSpy(int);
  void updateSpy(QString);
  void edited(int);
  void updateBestMove(Move&,int);
  void readConfig();
  void writeConfig();
  void rightButtonPressed(int,const QPoint&);

  void gameNetwork(bool);
  void editModify(bool);
  void optionMoveSlow(bool);
  void optionRenderBalls(bool);
  void optionShowSpy(bool);
  void toggleMenubar();
  void configure();
  void setLevel(int);
  void setIPlay(int);

private:
  void setupActions();
  void updateStatus();
  void userMove();
  void playGame();
  void loadPixmaps();
  void setupStatusBar();
  void updateActions();
  void setMoveNo(int, bool updateGUI = false);
  bool iPlayNow();
  Move haveHint();
  void readOptions(KConfig *);
  void writeOptions(KConfig *);

  Move actMove;
  Board* board;
  int actValue;
  BoardWidget *boardWidget;
  EvalScheme* currentEvalScheme;
  QTimer *timer;
  int timerState;
  int depth, moveNo;
  bool showMoveLong, stop, showSpy;
  bool editMode, renderBalls;
  int spyLevel;
  bool pastePossible, validShown;

  enum IPlay { Red = 0, Yellow, Both, None, Nb_IPlays };
  IPlay iplay;

  int stop_id, back_id, hint_id;
  int easy_id, normal_id, hard_id, challange_id, slow_id, level_id;
  int render_id;
  int yellow_id, red_id, both_id, none_id, iplay_id;
  int spy_id, paste_id, edit_id, forward_id, net_id;

  QLabel *validLabel, *ballLabel, *moveLabel, *statusLabel;
  QPixmap warningPix, okPix, redBall, yellowBall, noBall, netPix;
  QPixmap spy0, spy1, spy2, spy3;

  Network *net;
  int myPort;
  QStrList hosts;

  KAction *newAction, *stopAction, *backAction, *forwardAction, *hintAction, *pasteAction;
  KToggleAction *showMenubar, *renderBallsAction, *moveSlowAction,
      *showSpyAction, *editAction;
  KSelectAction *levelAction, *iplayAction;

  struct Data {
      const char *key, *label;
  };
  enum Level { Easy = 0, Normal, Hard, Challenge, Nb_Levels };
  static const Data LEVEL[Nb_Levels];
  static const Data IPLAY[AbTop::Nb_IPlays];

#ifdef SPION
  Spy* spy;
#endif

};

#endif /* _ABTOP_H_ */
