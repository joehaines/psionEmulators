/* Yo Emacs, this -*- C++ -*-

  Copyright (C) 1999-2001 Jens Hoefkens
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


/*

  This file contains the declaration and definition of a couple of
  classes related to the KBgBoard class.

  Effort has been made to keep this class general. Please comment on that
  if you want to use it in your own project. Most of the stuff is private
  and/or in utility classes that shouldn't be used directly. All public
  interfaces are in teh beginning of the file.

*/

#ifndef KBGBOARD_H
#define KBGBOARD_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <qspinbox.h>
#include <qpushbutton.h>
#include <qcheckbox.h>
#include <qdialog.h>
#include <qstring.h>
#include <qpainter.h>
#include <qpixmap.h>
#include <qbitmap.h>
#include <qregexp.h>
#include <qradiobutton.h>
#include <qwidget.h>
#include <qptrlist.h>
#include <qtimer.h>
#include <qlabel.h>
#include <qcursor.h>
#include <qpopupmenu.h>
#include <qcombobox.h>
#include <kdialogbase.h>
#include <kfontdialog.h>

#include "kbgstatus.h"

/*
 * Just some internal constants and classes
 */
const int US   = KBgStatus::US;
const int THEM = KBgStatus::THEM;
const int BOTH = KBgStatus::BOTH;

const int HOME_US_LEFT     = 101;
const int HOME_US_RIGHT    = 102;
const int HOME_THEM_LEFT   = 103;
const int HOME_THEM_RIGHT  = 104;
const int BAR_US           = 105;
const int BAR_THEM         = 106;
const int HOME_US          = 107;
const int HOME_THEM        = 108;

const int SHORT_MOVE_NONE   = 0;
const int SHORT_MOVE_SINGLE = 1;
const int SHORT_MOVE_DOUBLE = 2;

class KBgBoard;
class KBgBoardCell;
class KBgBoardMove;
class KBgBoardHome;
class KBgBoardBar;
class KBgBoardField;
class KBgBoardQDice;


/**
 * The KBgBoard class.
 *
 * This class handles all game operations of the client.
 */
class KBgBoard : public QWidget
{
	Q_OBJECT

	friend class KBgBoardCell;
	friend class KBgBoardHome;
	friend class KBgBoardBar;
	friend class KBgBoardField;

 public:

	/**
	 * Constructor and destructor. Parameter as usual.
	 */
	KBgBoard(QWidget *parent = 0, const char *name = 0,
			 QPopupMenu *menu = 0);
	virtual ~KBgBoard();

	/**
	 * Returns the current read/write status of the board.
	 */
	bool movingAllowed() const;

	/**
	 * Returns the current up-to-the-second pip count (including
	 * the moves that have been done.
	 */
	int getPipCount(const int& w) const;

	/**
	 * Sets the momentary short move mode. The parameter should
	 * be one of the constanst SHORT_MOVE_NONE, SHORT_MOVE_SINGLE
	 * or SHORT_MOVE_DOUBLE. This the automatic moving of a checker
	 * with the shortest possible move away from the current field.
	 */
	void setShortMoveMode(int m);

	/**
	 * Returns the current short move mode.
	 */
	int getShortMoveMode();

	/**
	 * Sets the background color and passes the info to the
	 * child widgets
	 */
	virtual void setBackgroundColor(QColor col);

	/**
	 * Prints the baord along with some basic info onto the
	 * painetr p. It is assumed that this painter is a postscript
	 * printer. Hence the plot is black/white only.
	 */
	void print(QPainter *p);

	/**
	 * Get whose turn it is - US, THEM or 0
	 */
	int getTurn() const;

	/**
	 * Allows for overriding the current turn color in edit mode.
	 */
	void storeTurn(const int pcs);

	/**
	 * Retrurns the current edit mode status.
	 */
	bool getEditMode() const;

	/**
	 * Get a new value for the cube from the user - this opens a dialog
	 */
	void queryCube();

	/**
	 * Get the font the board cells should use for the display of
	 * numbers and cube value.
	 */
	QFont getFont();

	/**
	 * This function has to be reimplemented to provide a minimum size for
	 * the playing area.
	 */
	QSize minimumSizeHint() const;
public slots:

	/**
	 * This allows the users of this widget to dis- and enable
	 * mouse events. In effect this triggers the read/write
	 * flag of the board.
	 */
	void allowMoving(const bool fl);

	/**
	 * Overwritten resize event handler.
	 *
	 * We overwrite the handler to make sure that all the cells are
	 * repainted as well.
	 */
	virtual void resizeEvent(QResizeEvent *);

        /**
	 * Undo the last move.
	 */
	void undoMove();

        /**
	 * Redo a previously undone move
	 */
	void redoMove();

	/**
	 * This is the most important public member. It takes
	 * a board status - s. the KBgBoardStatus class -
	 * and sets the board accordingly.
	 */
	void setState(const KBgStatus &);

	/**
	 * Set the context menu
	 */
	void setContextMenu(QPopupMenu *menu);

	/**
	 * Get the current state of the board.
	 */
	KBgStatus *getState(KBgStatus *st) const;

	/**
	 * Sets the edit mode of the board. In that mode the board can be
	 * modified arbitrarily.
	 */
	void setEditMode(const bool m);

	/**
	 * Allows the users of the board classe to set the font to be used
	 * on the board. Note that the fontsize is dynamically set
	 */
	void setFont(const QFont& f);

	/**
	 * Write the current configuration to the application's data base
	 */
	void saveConfig();

	/**
	 * Restore the stored configuration or start with reasonable defaults
	 */
	void readConfig();

 signals:

	/**
	 * The text identifies the current game status - could be put
	 * into the main window caption
	 */
	void statText(const QString &msg);

	/**
	 * The cells connect to this signal and it tells them that it is
	 * time to update their content now if necessary.
	 */
	void finishedUpdate();

	/**
	 * The user has requested the dice to be rolled. Emit the
	 * request to somebody who knows how to do that.
	 */
	void rollDice(const int w);

	/**
	 * Ask the server to double
	 */
	void doubleCube(const int w);

	/**
	 * Once the moves are all made, build a server command and send
	 * them out.
	 */
	void currentMove(QString *s);

	/* ************************************************** */
	/* ************************************************** */

	/*    Everything below this line is private and it    */
	/*    shouldn't be used by users of this widget.      */

	/*    This means the whole file! All following        */
	/*    classes and members are private.                */

	/* ************************************************** */
	/* ************************************************** */

protected:
  virtual QSize sizeHint() const;

	QColor baseColors[2];
	QFont boardFont;
        KBgBoardCell* cells[30];
	bool computePipCount;

 private:

	/**
	 * Emits a currentMove string to whomever cares.
	 */
	void sendMove();

	/**
	 * Emit a request for doubling.
	 */
	void getDoubleCube(const int w);

	/**
	 * Get pieces on who's bar - US or THEM
	 */
	int getOnBar( int who ) const;

	/**
	 * Get who's dice num - who = US or THEM, num = 0 or 1
	 */
	int getDice(  int w, int n ) const;

	/**
	 * Get the number on the cube
	 */
	int getCube() const;

	/**
	 * Ask the server for rolling
	 */
	void getRollDice(const int w);

	/**
	 * Tell the board that we kicked a piece off and store
	 * the information
	 */
	void kickedPiece();

	/**
	 * Check whether a move off is possible by checking that all pieces
	 * are either in the home board or already off
	 */
	bool moveOffPossible() const;

	/**
	 * Check whether who (US or THEM) can double
	 */
	bool canDouble( int who ) const;

	/**
	 * The cells have to tell us if the change the number of pieces, since
	 * we check that sometimes
	 */
	void updateField( int f, int v );

	/**
	 * Convert an ID from to the board numbers
	 */
	int IDtoNum(const int ID ) const;

	/**
	 * Checks if there is a possibility to get from src to dest.
	 */
	int checkMultiMove( int src, int dest, int m[4] );

	/**
	 * Checks whether the dice allow a move from src to dest (ID's)
	 */
	bool diceAllowMove( int src, int dest );

	/**
	 * Make a move from src to dest. The numbers are cellID's.
	 */
	void makeMove( int src, int dest );

	/**
	 * Translates a field number to a pointer to the cell.
	 */
	KBgBoardCell* getCell(int num);

	/**
	 * Draws a piece on the painter p, with the upper left corner
	 * of the enclosing rectangle being (x,y)
	 */
	void drawCircle(QPainter *p, int x, int y, int pcs, int diam,
			int col, bool upper, bool outer) const;

	/**
	 * Draws an anti-aliased checker on the painter p.
	 */
	void drawChecker(QPainter *p, int x, int y, int pcs, int diam,
			 int col, bool upper) const;

	/**
	 * Draws a simple 2-color checker on the painter p. This is intended
	 * for printing.
	 */
	void drawSimpleChecker(QPainter *p, int x, int y, int pcs,
			       int diam) const;

	/**
	 * Given a position on the board, return the cell under the mouse pointer
	 */
	KBgBoardCell* getCellByPos(const QPoint& p) const;

	/**
	 * Name says it all, doesn't it?
	 */
	void showContextMenu();

	/**
	 * Temporary replace the cursor, saves the old one
	 */
	void replaceCursor(const QCursor& c);

	/**
	 * Restore the previously stored cursor.
	 */
	void restoreCursor();

	/**
	 * Given the sign of p, return the current base color
	 */
	QColor getCheckerColor(int p) const;

	/**
	 * Small utility function for makeMove - just for readability
	 */
	int makeMoveHelper(int si, int sf, int delta);

	/**
	 * Private data members - no description needed
	 */
	QPopupMenu *contextMenu;
	QPtrList<KBgBoardMove> moveHistory;
	QPtrList<KBgBoardMove> redoHistory;
	int direction, color;
	int hasmoved;
	bool allowmoving, editMode;
	int storedTurn;
	int onbar[2];
	int onhome[2];
	int board[26];
	int dice[2][2];
	int possMoves[7];
	int cube;
	int checkerDiam;
	bool cubechanged;
	bool maydouble[2];
	int shortMoveMode;
	QCursor *savedCursor;
};

/**
 * Base class for the cells on the board
 *
 * This base class provides all the necessary functions of a cell
 * on a backgammon board. It has a bunch of virtual functions that
 * are overloaded in the derived classes.
 */
class KBgBoardCell : public QLabel
{
      	Q_OBJECT

 public:

	/**
	 * Constructor and destructor
	 */
	KBgBoardCell(QWidget * parent, int numID);
	virtual ~KBgBoardCell();

	/**
	 * sets the number and color of checkers on this cell
	 * takes care of repainting
	 */
	virtual void cellUpdate(const int p, const bool cubechanged = false) = 0;

	/**
	 * Draws the content of the cell on the painter *p
	 */
	virtual void paintCell(QPainter *p, int xo = 0, int yo = 0,
			       double sf = 1.0) const;

	/**
	 * Updates all the status variables at once
	 */
	virtual void statusUpdate(int dir, int col);

protected:

	/**
	 * Draw vertical lines around the board.
	 */
	void drawVertBorder(QPainter *p, int xo, int yo, double sf = 1.0) const;
	void drawOverlappingCheckers(QPainter *p, int xo, int yo,
				     double sf = 1.0) const;
	void drawCube(QPainter *p, int who, int xo, int yo, double sf = 1.0) const;

	/**
	 * Puts a piece of color on a field
	 */
	void putPiece( int newColor );

	/**
	 * Removes a piece from a field. Returns true if success or false else
	 * (i.e. there is no piece on this field.
	 */
	bool getPiece();

	/**
	 * Return the number of this cell
	 */
	virtual int getNumber() const;

	/**
	 * Return the suggested diameter of a piece
	 */
	int getCheckerDiameter() const;
	int getCellColor();

	/**
	 * Do we allow a drop of the DragEvent ? This checks the payload and
	 * reacts on it.
	 */
	virtual bool dropPossible(int fromCellID, int newColor) = 0;

 protected:

	/**
	 * Overwrite how a cell draws itself
	 */
	virtual void drawContents(QPainter *);

	/**
	 * Status numbers that store the current board status.
	 */
	int mouseButton;
	int direction;
	int color;

	/**
	 * How many pieces are we currently holding ?
	 */
	int pcs;

	/**
	 * Our own ID
	 */
	int cellID;

	/**
	 * Indicates whether this cell needs to repaint itself after
	 * the board has been processed.
	 */
	bool stateChanged;
	bool colorChanged, directionChanged;

	/**
	 * the board and the pieces are one unit (none makes
	 * sense without the other). So the pieces know and access their parent.
	 */
	KBgBoard *board;
 	void checkAndMakeShortMove(QMouseEvent *e, int m);

	/**
	 * Returns the bounding rectangle of the cube on this cell
	 */
	QRect cubeRect( int who, bool big, double sf = 1.0 ) const;

	/**
	 * Returns the bounding rectangle of the dice i on this cell
	 */
	QRect diceRect(int i, bool big, double sf = 1.0, double scale = 0.45) const;
	bool dragInProgress;

 protected slots:
        /**
	 * Refreshes the widget. This is essentially a call to update().
	 */
        virtual void refresh();

	/**
	 * Can we currently drag from this field ?
	 */
	virtual bool dragPossible() const = 0;

	/**
	 * Possibly initiate a drag.
	 */
	virtual void mouseMoveEvent( QMouseEvent * );
	virtual void mousePressEvent(QMouseEvent *e);

	/**
	 * Make the shortes possible move away from this cell
	 */
	void makeShortMove();
	void makeShortMoveHelper(int s, int d);

	/**
	 * Catch a single left click and perhapes make a move.
	 */
	virtual void mouseReleaseEvent( QMouseEvent *e );

	/**
	 * Catch a double left click and perhapes make a move.
	 */
	virtual void mouseDoubleClickEvent( QMouseEvent *e );
};

/**
 * The homes are derived from the regular cells. They just overwrite
 * some members.
 */
class KBgBoardHome : public KBgBoardCell
{
      	Q_OBJECT

 public:
	/**
	 * sets the number and color of checkers on this cell
	 * takes care of repainting
	 */
	virtual void cellUpdate(const int p, const bool cubechanged = false);

	/*
	 * Draws the content of the cell on the painter *p
	 */
	virtual void paintCell(QPainter *p, int xo = 0, int yo = 0,
			       double sf = 1.0) const;

	/**
	 * Constructor and destructor
	 */
	KBgBoardHome( QWidget * parent, int numID);
	virtual ~KBgBoardHome();

	/**
	 * Check whether a drop on the home cell is possible.
	 */
	virtual bool dropPossible(int fromCellID, int newColor);

 protected:
	/**
	 * Determine whether a drag from the home is possible.
	 */
	virtual bool dragPossible() const;

	/**
	 * Get the double clicks
	 */
	virtual void mouseDoubleClickEvent( QMouseEvent *e );

        /**
	 * The homes contain dice and cube. This draws them.
	 */
        void drawDiceAndCube(QPainter *p, int who, int xo, int yo,
			      double sf) const;

	void drawDiceFrame(QPainter *p, int col, int num, int xo, int yo,
			   bool big, double sf) const;
	void drawDiceFace(QPainter *p, int col, int num, int who, int xo,
			  int yo, double sf) const;

 private:
	/**
	 * Save old dice to avoid repainting
	 */
	int savedDice[2];

};

/**
 * The bars are derived from the regular cells. They just overwrite
 * some members.
 */
class KBgBoardBar : public KBgBoardCell
{
       	Q_OBJECT

 public:
	/**
	 * sets the number and color of checkers on this cell
	 * takes care of repainting
	 */
	virtual void cellUpdate(const int p, const bool cubechanged = false);

	/**
	 * Draws the content of the cell on the painter *p
	 */
	virtual void paintCell(QPainter *p, int xo = 0, int yo = 0,
			       double sf = 1.0) const;

	/**
	 * Constructor
	 */
	KBgBoardBar( QWidget * parent, int numID );

	/**
	 * Destructor
	 */
	virtual ~KBgBoardBar();

	/**
	 * Check whether a drop on the bar cell is possible.
	 */
	virtual bool dropPossible(int fromCellID, int newColor);

 protected:
	/**
	 * Determine whether a drag from the bar is possible.
	 */
	virtual bool dragPossible() const;
	/**
	 * Get the double clicks
	 */
	virtual void mouseDoubleClickEvent(QMouseEvent *e);
};

/**
 * The fields are derived from the regular cells. They just overwrite
 * some members.
 */
class KBgBoardField : public KBgBoardCell
{
	Q_OBJECT

 public:
	/**
	 * Constructor and destructor
	 */
	KBgBoardField( QWidget * parent, int numID);
	virtual ~KBgBoardField();

	/**
	 * sets the number and color of checkers on this cell
	 * takes care of repainting
	 */
	virtual void cellUpdate(const int p, const bool cubechanged = false);

	/**
	 * Draws the content of the cell on the painter *p
	 */
	virtual void paintCell(QPainter *p, int xo = 0, int yo = 0,
			       double sf = 1.0) const;

	/**
	 * Check whether a drop on the field cell is possible.
	 */
	virtual bool dropPossible(int fromCellID, int newColor);

 protected:
	/**
	 * Determine whether a drag from the field is possible.
	 */
	virtual bool dragPossible() const;

	/**
	 * Return the y-coordinate of the number of the field.
	 */
	int numberBase() const;
};

/**
 * Internal class for storing a move in the undo history buffer.
 */
class KBgBoardMove
{
 public:
	/**
	 * Accepts source, destination and the dice that made this move
	 * possible. Set the kicked flag to false.
	 */
	KBgBoardMove(int src, int dest, int delta)
		{s = src; d = dest; l = delta; k = false;}

	/**
	 * Set this move to be a kick
	 */
	void setKicked(bool kicked) {k = kicked;}

	/**
	 * Look up the source
	 */
	int source() const {return s;}

	/**
	 * Look up the destination
	 */
	int destination() const {return d;}

	/**
	 * Look up the dice that made this move
	 */
	int length() const {return l;}

	/**
	 * Check whether the move kicked a piece
	 */
	bool wasKicked() const {return k;}

 private:
	/**
	 * Source, destination, dice
	 */
	int s, d, l;

	/**
	 * Kicked move ?
	 */
	bool k;
};

/**
 * Simple dialog that allows to query the user for dice values.
 *
 * A very simple dialog with two SpinBoxes and two buttons.
 */
class KBgBoardQDice : public QDialog
{
	Q_OBJECT

public:

	/**
	 * Constructor and destructor
	 */
	KBgBoardQDice(const char *name = 0);
	virtual ~KBgBoardQDice();

protected:

	/**
	 * Spin boxes and buttons are children
	 */
	QSpinBox     *sb[2];
	QPushButton  *ok;
	QPushButton  *cancel;

public slots:

	/**
	 * Get the face values.
	 */
        int getDice(int n);
};


/**
 * Simple dialog that allows to query the user for the cube value.
 */
class KBgBoardQCube : public QDialog
{
	Q_OBJECT

public:

	/**
	 * Constructor and destructor
	 */
	KBgBoardQCube(int val, bool us, bool them);
	virtual ~KBgBoardQCube();

protected:

	/**
	 * Spin boxes and buttons are children
	 */
	QComboBox    *cb[2];
	QPushButton  *ok;
	QPushButton  *cancel;

public slots:

	/**
	 * Get the face values.
	 */
        int getCubeValue();
        int getCubeOwner();

protected slots:

        /**
         * These slots are needed to get consistent relations
	 * between the two combo boxes.
         */
        void changePlayer(int val);
	void changeValue(int player);

};


/**
 * Extension of the KBgBoard class that can add itself
 * to a QTabDialog for configuration.
 */
class KBgBoardSetup : public KBgBoard
{
	Q_OBJECT

public:

	/**
	 * Constructor
	 */
	KBgBoardSetup(QWidget *parent = 0, const char *name = 0,
			      QPopupMenu *menu = 0);

	/**
	 * Lets the board put its setup pages into the notebook nb
	 */
	void getSetupPages(KDialogBase *nb);

public slots:

        /**
	 * Setup changes are confirmed. Store them.
	 */
	void setupOk();

	/**
	 * Setup has been cancelled. Undo the color changes
	 */
	void setupCancel();

	/**
	 * Load default values for user settings
	 */
	void setupDefault();

protected slots:

        /**
	 * Open a color dialog for the background color
	 */
	void selectBackgroundColor();

        /**
	 * Open a color dialog for the first checker color
	 */
	void selectBaseColorOne();

        /**
	 * Open a color dialog for the second checker color
	 */
	void selectBaseColorTwo();

private:

	/**
	 * Save settings before the user changed them
	 */
	KFontChooser *kf;

	QRadioButton *rbMove[3];

	QColor saveBackgroundColor;
	QColor saveBaseColors[2];

	/**
	 * Need these to change their colors
	 */
	QPushButton *pbc_1, *pbc_2, *pbc_3;
	QCheckBox *cbp;
};

#endif // KBGBOARD_H
