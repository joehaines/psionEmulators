/* Yo Emacs, this -*- C++ -*-

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

*/

#include "kbgstatus.h"
#include "kbgstatus.moc"

#include <stdlib.h>
#include <stdio.h>


/*
 * Parse a rawboard description from FIBS and initialize members.
 */
KBgStatus::KBgStatus(const QString &rawboard)
{
	/*
	 * This is the format string from hell...
	 */
	const char *format = ("%*[^:]%*[:]%99[^:]%*[:]%99[^:]%*[:]%i%*[:]%i%*[:]%i%*[:]"
			      "%i%*[:]%i%*[:]%i%*[:]%i%*[:]%i%*[:]%i%*[:]%i%*[:]%i%*[:]%i%*[:]" 
			      "%i%*[:]%i%*[:]%i%*[:]%i%*[:]%i%*[:]%i%*[:]%i%*[:]%i%*[:]%i%*[:]"
			      "%i%*[:]%i%*[:]%i%*[:]%i%*[:]%i%*[:]%i%*[:]%i%*[:]%i%*[:]"
			      "%i%*[:]"
			      "%i%*[:]%i%*[:]%i%*[:]%i%*[:]%i%*[:]%i%*[:]%i%*[:]%i%*[:]%i%*[:]"
			      "%i%*[:]%i%*[:]%i%*[:]%i%*[:]%i%*[:]%i%*[:]%i%*[:]%i%*[:]%i%*[:]"
			      "%i%*[:]%i%*[:]");

	char opponent[100], player[100];

	QString cap;

	int board[26], ldice[2][2], maydouble[2], scratch[4], onhome[2], onbar[2];
	int points[2];
	int tomove, lturn, color, cube, direction, redoubles, bar, home, length;
	
	// split the incoming line at colons - latin1() is fine, since the string comes from FIBS.
	sscanf (rawboard.latin1(), format,
		player, opponent, 
		&length,
		&points[0], &points[1],
		&board[ 0], &board[ 1], &board[ 2], &board[ 3],	&board[ 4], &board[ 5], 
		&board[ 6], &board[ 7],	&board[ 8], &board[ 9], &board[10], &board[11],
		&board[12], &board[13], &board[14], &board[15],	&board[16], &board[17], 
		&board[18], &board[19],	&board[20], &board[21], &board[22], &board[23],
		&board[24], &board[25], 
		&lturn,
		&ldice[US  ][0], &ldice[US  ][1],	&ldice[THEM][0], &ldice[THEM][1],
		&cube,
		&maydouble[US], &maydouble[THEM],
		&doubled_,
		&color,
		&direction,
		&home, &bar,
		&onhome[US], &onhome[THEM],  // on home
		&onbar[US],  &onbar[THEM],   // on bar
		&tomove,
		&scratch[2], &scratch[3],    // forced move & did crawford
		&redoubles);

	player_[US]   = player;
	player_[THEM] = opponent;

	setCube(cube, maydouble[US], maydouble[THEM]);
	setDirection(direction);
	setColor(color);
	for (int i = 1; i < 25; i++) {
		if (board[i] == 0 || color == board[i]/abs(board[i]))
			setBoard(i, US, abs(board[i]));
		else
			setBoard(i, THEM, abs(board[i]));
	}
	setDice(US  , 0, ldice[US  ][0]); 
	setDice(US  , 1, ldice[US  ][1]);
	setDice(THEM, 0, ldice[THEM][0]); 
	setDice(THEM, 1, ldice[THEM][1]);	

	setHome(US,   onhome[US  ]); 
	setHome(THEM, onhome[THEM]);

	setBar(US,   board[   bar]); 
	setBar(THEM, board[25-bar]);

	setPoints(US,   points[0]);
	setPoints(THEM, points[1]);
	
	if (lturn == 0)
		setLength(-1);
	else
		setLength(length);

	int t = lturn*color;
	if (t  > 0) setTurn(US);
	if (t  < 0) setTurn(THEM);
	if (t == 0) setTurn(NONE);
}

/*
 * Constructor initializes the status to an empty board with cube one
 * and empty dice.
 */
KBgStatus::KBgStatus()
	: QObject()
{
	/*
	 * Initialize members
	 */
	for (int i = 0; i < 26; ++i)
		setBoard(i, US, 0);

	for (int i = US; i <= THEM; i++) {
		setDice  (i, 0, 0);
		setDice  (i, 1, 0);
		setHome  (i, 0);
		setBar   (i, 0);
		setPoints(i, -1);
		setPlayer(i, QString::null);
	}
	setColor(White, US);
	setCube(1, BOTH); // also initializes maydouble
	setDirection(1);
	setLength(-1);
	setTurn(NONE);

	// initialize members without assignment functions
	doubled_ = 0;
}

/*
 * Copy constructor calls private utility function.
 */
KBgStatus::KBgStatus(const KBgStatus &rhs)
	: QObject()
{
	copy(rhs);
}

/*
 * Destructor
 */
KBgStatus::~KBgStatus()
{
	// nothing to do
}

/*
 * Assignment operator shares a lot of code with the copy
 * constructor.
 */
KBgStatus& KBgStatus::operator=(const KBgStatus &rhs)
{
	if (this == &rhs) return *this;
	copy(rhs);
	return *this;
}

void KBgStatus::copy(const KBgStatus &rhs)
{
	for (int i = 0; i < 26; i++)
		board_[i] = rhs.board_[i];

	for (int i = US; i <= THEM; i++) {
		
		home_[i]    = rhs.home_[i];
		bar_ [i]    = rhs.bar_ [i];
		dice_[i][0] = rhs.dice_[i][0];
		dice_[i][1] = rhs.dice_[i][1];

		maydouble_[i] = rhs.maydouble_[i];
		player_   [i] = rhs.player_   [i];
		points_   [i] = rhs.points_   [i];
	}

	cube_      = rhs.cube_;
	direction_ = rhs.direction_;
	color_     = rhs.color_;
	turn_      = rhs.turn_;
	doubled_   = rhs.doubled_;
}


/*
 * Access functions 
 */
int KBgStatus::board(const int &i) const
{
	return ((0 < i && i < 25) ? color_*board_[i] : 0);
}

int KBgStatus::home(const int &w) const
{
	return ((w == US || w == THEM) ? color_*home_[w] : 0);
}

int KBgStatus::bar(const int &w) const
{
	return ((w == US || w == THEM) ? color_*bar_[w] : 0);
}

int KBgStatus::color(const int &w) const
{
	return ((w == THEM) ? -color_ : color_);
}

int KBgStatus::direction() const
{
	return direction_;
}

int KBgStatus::dice(const int &w, const int &n) const
{
	if ((w == US || w == THEM) && (n == 0 || n == 1))
		return dice_[w][n];
	else 
		return 0;
}

int KBgStatus::cube(const int &w) const
{
	if (w == US || w == THEM)
		return ((maydouble_[w]) ? cube_ : -cube_);
	return 0;
}

int KBgStatus::points(const int& w) const
{
	return ((w == US || w == THEM) ? points_[w] : -1);
}

QString KBgStatus::player(const int &w) const
{
	return ((w == US || w == THEM) ? player_[w] : QString::null);	
}

int KBgStatus::length() const
{
	return length_;
}

int KBgStatus::turn() const
{
	return turn_;
}

bool KBgStatus::doubled() const
{
	return doubled_;
}


/*
 * Assignment functions
 */
void KBgStatus::setBoard(const int &i, const int &w, const int &v)
{	
	if (0 < i && i < 25) {
		if (w == US)
			board_[i] =  abs(v);
		else if (w == THEM)
			board_[i] = -abs(v);
	}
}

void KBgStatus::setHome(const int &w, const int &v)
{
	if (w == US)
		home_[w] =  abs(v);
	else if (w == THEM)
		home_[w] = -abs(v);		
}

void KBgStatus::setBar(const int& w, const int& v)
{
	if (w == US)
		bar_[w] =  abs(v);
	else if (w == THEM)
		bar_[w] = -abs(v);
}

void KBgStatus::setColor(const int &c, const int &w)
{
	if (w == US)
		color_ = ((c < 0) ? Black : White);
	else if (w == THEM)
		color_ = ((c < 0) ? White : Black);
}

void KBgStatus::setDirection(const int &dir)
{
	direction_ = ((dir < 0) ? -1 : +1);
}

void KBgStatus::setDice(const int &w, const int &n, const int &v)
{
	if ((w == US || w == THEM) && (n == 0 || n == 1)) {
		if (0 <= v && v <= 6)
			dice_[w][n] = v;
		else
			dice_[w][n] = 0;
	}
}

void KBgStatus::setCube(const int &c, const bool &us, const bool &them)
{
	int             w = NONE;
	if (us)         w = US;
	if (them)       w = THEM;
	if (us && them)	w = BOTH;
	setCube(c, w);
}

void KBgStatus::setCube(const int &c, const int &w)
{
	// assume that int has at least 32 bits...
	for (int i = 0; i < 31; i++) {
		if (1<<i == (cube_ = c)) break;
		cube_ = 0;
	}
	maydouble_[US  ] = (w == US   || w == BOTH);
	maydouble_[THEM] = (w == THEM || w == BOTH);
}

void KBgStatus::setPoints(const int &w, const int &p)
{
	if (w == US || w == THEM)
		points_[w] = p;
}

void KBgStatus::setPlayer(const int &w, const QString &name)
{
	if (w == US || w == THEM) 
		player_[w] = name;
}

void KBgStatus::setLength(const int &l)
{
	length_ = l;
}

void KBgStatus::setTurn(const int &w) 
{
	if (w == US || w == THEM || w == BOTH)
		turn_ = w;
}
	

/*
 * Utility functions
 */
int KBgStatus::moves() const
{
	int start, dir;
	
	/*
	 * Return an error if it isn't anybodies turn.
	 */
	if ((turn() != US) && (turn() != THEM)) 
		return -1;

	/*
	 * Determine tha direction of the current move
	 */
	if ((turn() == US && direction() < 0) || (turn() == THEM && direction() > 0)) {
		start = 25;
		dir   = -1;
	} else {
		start =  0;
		dir   =  1;
	}

	/*
	 * Get the current dice transferred into the move[] array. The
	 * final zero is a marker
	 */
	int move[5] = {0, 0, 0, 0, 0};
	move[0] = dice(turn(), 0);
	move[1] = dice(turn(), 1);
	if (move[0] == move[1]) {
		move[3] = move[2] = move[0];
		// saves some work further down
		if (move[0] == 0) 
			return 0;

	}

	bool doubledice = (move[3] != 0);
	int count = 4;

	/*
	 * Get a copy of ourselves. That way we can mess around with
	 * the internals of the game.
	 */
	KBgStatus sc(*this);

	/*
	 * Start with getting all checkers off the bar
	 */
	while (count > 0 && sc.bar(turn()) != 0) {		
		if (move[--count] != 0) {			
			if (color(turn())*sc.board(start+dir*move[count]) >= -1) {
				sc.setBar(turn(), abs(sc.bar(turn()))-1);
				sc.setBoard(start + dir*move[count], turn(), 1);
				move[count] = 0;
			}
		}
	}

	/*
	 * Collect remaining moves in the beginning of the move array 
	 */
	int j = 0;
	for (int i = 0; i < 4; i++) {
		if ((move[j] = move[i])) 
			++j;
		if (i > j) move[i] = 0;
	}


	/*
	 * Find number of remaining moves
	 */
	int moves = 0;
	move[4] = 0;
	while (move[moves])
		++moves;
	
	/*
	 * Done or no more moves because the bar is not empty
	 */
	if (sc.bar(turn()) != 0 || move[0] == 0)
		return (move[3] ? 4 - moves : 2 - moves);

	/*
	 * Try to find possible moves on the board 
	 */
	if (moves == 1 || move[0] == move[1]) {

		/*
		 * Order doesn't matter, all moves are equal
		 */
		while (--moves >= 0 && movePossible(sc, move[moves], start, dir));
		moves++;
		return (doubledice ? 4 - moves : 2 - moves);

	} else {

		/*
		 * Order does matter; try both ways.
		 */
		moves = 0;
		for (int i = 0; i < 25; i++) {
			if (movePossible(sc, move[0], start + i*dir, dir)) {
				moves = 1;
				if (movePossible(sc, move[1], start, dir)) {
					return 2;
				}
			}
			// Restore scratch copy...
			sc = *this;
		}
		for (int i = 0; i < 25; i++) {

			if (movePossible(sc, move[1], start + i*dir, dir)) {
				moves = 1;
				if (movePossible(sc, move[0], start, dir)) {
					return 2;
				}
			}
			// Restore scratch copy...
			sc = *this;
		}
		return moves;
	}
}

bool KBgStatus::movePossible(KBgStatus &sc, int a, int start, int dir) const
{       
	/*
	 * Determine where the first checker in moving direction is
	 * located
	 */
	int first = (dir > 0) ?  1 : 24;
	int last  = (dir > 0) ? 25 :  0;
	while (first != last && color(turn())*sc.board(first) <= 0)
		first += dir;

	/*
	 * Are we moving off ?
	 */
	bool off = false;
	if ((dir > 0 && first > 18) || (dir < 0 && first <  7)) 
		off = true;

	/*
	 * Find a move by exhaustive search.
	 */
	while (true) {

		start += dir;
		int final = start+dir*a;
		
		/*
		 * Make absolutely sure that the loop terminates eventually
		 */
		if (start <= 0 || start >= 25)
			return false;

		if (color(turn())*sc.board(start) > 0) {
			
			if (0 < final && final < 25 && color(turn())*sc.board(final) >= -1) {
				sc.setBoard(start, turn(), abs(sc.board(start)) - 1);
				sc.setBoard(final, turn(), abs(sc.board(final)) + 1);
				return true;
			} else if (off && (final == 0 || final == 25)) {
				sc.setBoard(start, turn(), abs(sc.board(start)) - 1);
				sc.setHome(turn(), abs(sc.home(turn())) + 1);
				return true;
			} else if (off && first == start && (final > 24 || final < 1)) {
				sc.setBoard(start, turn(), abs(sc.board(start)) - 1);
				sc.setHome(turn(), abs(sc.home(turn())) + 1);
				return true;
			}
		}
	}
}

// EOF
