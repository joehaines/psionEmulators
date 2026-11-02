/**
 * Some Implementation details for configEval Dialog
 *
 */

#ifndef _EVALDLGIMPL_H_
#define _EVALDLGIMPL_H_

#include "EvalDlg.h"

class EvalScheme;
class Board;

class EvalDlgImpl: public EvalDlg
{
 Q_OBJECT

 public:
  EvalDlgImpl(QWidget* parent, Board* board);
 ~EvalDlgImpl();

  EvalScheme* evalScheme() { return _scheme; }

 public slots:
 void deleteEntry();
 void saveas();
 void select(int i);
 void updateCount();
 void updateMove();
 void updateFields();
 void updateInARow();

 private:
  void updateEval();
  void updateWidgets();
  void connectEditLines();
  void disconnectEditLines();

  EvalScheme *_origScheme, *_scheme;
  Board* _board;
};

#endif // _EVALDLGIMPL_H_
    
