/*
  Copyright (C) 2001 Jens Hoefkens
  jens@hoefkens.com
  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

  $Id: kbgstatus.h,v 1.9 2003/07/01 00:31:37 mueller Exp $
  
*/

#ifndef KBGSTATUS_H 
#define KBGSTATUS_H 

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif 

#include <qobject.h>


/**
 * This is a the fundamental class of the KBg* hierarchy. It represents 
 * the state of backgammon games.
 *
 * The game states can be initialized in a variety of information and
 * are meant to be passed to the board. In addition to that, the class
 * has several utility functions that are helpful for engines that
 * maintain a local state.
 *
 * @short The backgammon status object
 * @author Jens Hoefkens <jens@hoefkens.com>
 * @version $Id: kbgstatus.h,v 1.9 2003/07/01 00:31:37 mueller Exp $
 */
class KBgStatus : public QObject
{

	Q_OBJECT

 public:

	/**
	 * These numbers are used to distinguish the two players. The
	 * notion of US and THEM is a leftover from ancient times.
	 */
	enum {NONE = -1, US = 0, THEM = 1, BOTH = 2};
	
	/**
	 * The names are just to distinguish the two different colors.
	 */
	enum {Black = -1, White = +1};

	/**
	 * The default constructor initializes the status to an "empty" 
	 * state. The board and dice are empty and the cube shows 1.
	 */
	KBgStatus();

	/**
	 * Constructor from a FIBS rawboard string
	 */
	KBgStatus(const QString &rawboard);

	/**
	 * Copy constructor
	 */
	KBgStatus(const KBgStatus &rhs);

	/**
	 * Assignment operator
	 */
	KBgStatus& operator=(const KBgStatus &rhs);
	
	/**
	 * Destructor
	 */
	virtual ~KBgStatus();


	/*
	 * The absolute value of the return value is the number of
	 * checkers on the i-th field (or zero if i is out of
	 * bounds). If the return value has the same sign as the
	 * current color of US, it belongs to US, otherwise it belongs
	 * to THEM.
	 */
	int board(const int &i) const;

	/*
	 * The absolute value of the return value is the number of
	 * checkers on the home of player w (or zero if i is out of
	 * bounds). If the return value has the same sign as the
	 * current color of US, it belongs to US, otherwise it belongs
	 * to THEM.
	 *
	 * The encoding of the color is slighly redundant. See also board(...).
	 */
	int home(const int &w = US) const;
	
	/*
	 * The absolute value of the return value is the number of
	 * checkers on the bar of player w (or zero if i is out of
	 * bounds). If the return value has the same sign as the
	 * current color of US, it belongs to US, otherwise it belongs
	 * to THEM.
	 *
	 * The encoding of the color is slighly redundant. See also board(...).
	 */
	int bar(const int &w = US) const;
		
	/*
	 * Return the current color of player w. If w is invalid, the
	 * color of US is returned. The return value will be either
	 * Black or White.
	 */
	int color(const int& w = US) const;

	/*
	 * Returns the current direction of the game. It is -1 if US
	 * plays from 24 to 1 and +1 if US plays from 1 to 24.
	 */
	int direction() const;

	/*
	 * Returns the n-th dice of player w. If w is invalid or if n
	 * is out of bounds, return zero. Also, if the dice haven't
	 * been set, zero is returned.
	 */
	int dice(const int &w, const int &n) const;

	/*
	 * Returns the value of the cube. If w can double, the return
	 * value is positive, if w may not double, the negative value
	 * of the cube is returned. If w is not legal, zero is
	 * returned.
	 */
	int cube(const int &w = US) const;

	/*
	 * Return the points of w in th ecurrent game. Negative values
	 * indicate that either w was not a legal player ID or that
	 * the engine doesn't have any information on points.
	 */
	int points(const int &w) const;

	/*
	 * Return the name of player w. If w is out of bounds or if
	 * the player names have not been set, QString::null is
	 * returned.
	 */
	QString player(const int &w = US) const;

	/*
	 * Return the length of the game. Negative values should be used to
	 * indicate that the game is over. Zero indicates that the game is
	 * unlimited.
	 */
	int length() const;

	/*
	 * Return whose turn it is. The possible return values are US,
	 * THEM, and NONE.
	 */
	int turn() const;

	/*
	 * Return true if the cube has just been offered. If this
	 * information is not available or if this is not the case,
	 * return false.
	 */
	bool doubled() const;

	/*
	 * Set the number of checkers of player w on the i-th field to
	 * the absolute value of v. If either i or w are out of bound,
	 * nothing is done.
	 *
	 * Internally, positive numbers are stored for US and negative
	 * ones for THEM. While this coding is redundant, it is
	 * consistent with the storing of board positions.
	 */
	void setBoard(const int &i, const int &w, const int &v);

	/*
	 * Set the number of checkers on the home of player w to the
	 * absolute value of v. If w is out of bound, nothing is done.
	 *
	 * Internally, positive numbers are stored for US and negative
	 * ones for THEM. While this coding is redundant, it is
	 * consistent with the storing of board positions. See also
	 * setBoard(...).
	 */
	void setHome(const int &w = US, const int &v = 0);

	/*
	 * Set the number of checkers on the bar of player w to the
	 * absolute value of v. If w is out of bound, nothing is done.
	 *
	 * Internally, positive numbers are stored for US and negative
	 * ones for THEM. While this coding is redundant, it is
	 * consistent with the storing of board positions. See also
	 * setBoard(...).
	 */
	void setBar(const int &w = US, const int &v = 0);

	/*
	 * This function sets the color of player w to c. Negative
	 * values of c translate to Black for US (and White for
	 * THEM). Non-negative values for c translate to White for US
	 * and Black for THEM.
	 */
	void setColor(const int& col, const int& w = US);

	/*
	 * Set the direction of the game. If dir is negative, US plays
	 * from 24 to 1. If dir is positive, US plays from 1 to 24.
	 */
	void setDirection(const int &dir);

	/*
	 * Set the n-th dice of player w to v. Nothing is done if w is
	 * invalid or if n is out of bounds. If v is invalid, the
	 * value zero is assigned (i.e., the dice is invalidated).
	 */
	void setDice(const int &w, const int &n, const int &v);

	/*
	 * Set the cube to c us indicates if US can double, them
	 * indicates if THEM can double.
	 *
	 * This function is depreciated.
	 */
	void setCube(const int &c, const bool &us, const bool &them);

	/*
	 * Set the cube to c, which must be a legal value (i.e., a
	 * power of 2). w indicates who can double. Legal values are
	 * NONE, US, THEM, and BOTH.
	 */
	void setCube(const int &c, const int &w);

	/*
	 * Set the points of w in the current game to p. Nothing is
	 * done if w is illegal.
	 */
	void setPoints(const int &w, const int &p);

	/*
	 * Set the name of player w to name. If w is out of bound,
	 * nothing is done.
	 */
	void setPlayer(const int &w, const QString &name);

	/*
	 * Set the length of the game. Negative values should be used to
	 * indicate that the game is over. Zero indicates that the game 
	 * is unlimited.
	 */
	void setLength(const int &l);

	/*
	 * Set the turn to w. Legal values for w are US, THEM, and
	 * NONE (which should indicate that the game is over).
	 */
	void setTurn(const int &w);

	/*
	 * Return the number of possible moves basesd on the current
	 * dice, checkers, etc.
	 */
	int moves() const;

 private:

	/*
	 * Determine if there is any possibility to move a steps
	 * anywhere starting from start or later into direction 
	 * dir in the game given by sc.
	 */
	bool movePossible(KBgStatus &sc, int a, int start, int dir) const;

	/*
	 * Copy constr. and assignment share a lot of code.
	 */
	void copy(const KBgStatus &rhs);

	/*
	 * Private variables with self-expalanatory names. 
	 */
	QString player_[2];

	int board_[26], home_[2], bar_[2], dice_[2][2], points_[2];
	int color_, direction_, cube_, length_, turn_;
	int doubled_;

	bool maydouble_[2];
};
  
#endif // KBGSTATUS_H 
