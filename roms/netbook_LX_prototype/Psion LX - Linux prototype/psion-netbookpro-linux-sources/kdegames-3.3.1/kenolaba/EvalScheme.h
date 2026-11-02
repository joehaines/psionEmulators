/**
 * EvalScheme
 *
 * Configuration options for a Evaluation Scheme.
 * Evaluation Schemes are used for evalution of a Abalone board position
 *
 * (C) JW, 2000
 */

#ifndef _EVALSCHEME_H_
#define _EVALSCHEME_H_

#include <qstring.h>

#include "Move.h"

class KConfig;

/*
 * The constructor gets a name, and tries to read the scheme
 * for the Kenolaba configuration file, if nothing found, use
 * default values
 */

class EvalScheme
{
 public:
  EvalScheme(QString);
  EvalScheme(EvalScheme&);
  ~EvalScheme() {}

  void setDefaults();
  void read(KConfig*);
  void save(KConfig*);

  static EvalScheme* create(QString);
  QString ascii();

  void setName(QString n) { _name = n; }
  void setRingValue(int ring, int value);
  void setRingDiff(int ring, int value);
  void setStoneValue(int stoneDiff, int value);
  void setMoveValue(int type, int value);
  void setInARowValue(int stones, int value);

  QString name() { return _name; }
  int ringValue(int r) { return (r>=0 && r<5) ? _ringValue[r] : 0; }
  int ringDiff(int r) { return (r>0 && r<5) ? _ringDiff[r] : 0; }
  int stoneValue(int s) { return (s>0 && s<6) ? _stoneValue[s] : 0; }
  int moveValue(int t) { return (t>=0 && t<Move::typeCount) ? _moveValue[t] : 0;}
  int inARowValue(int s) { return (s>=0 && s<InARowCounter::inARowCount) ? _inARowValue[s]:0; }

 private:
  int _ringValue[5], _ringDiff[5];
  int _stoneValue[6], _moveValue[Move::none];
  int _inARowValue[InARowCounter::inARowCount];

  QString _name;
};

#endif // _EVALSCHEME_H_
