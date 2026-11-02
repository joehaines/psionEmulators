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

  This file contains the implementation of the KBgBoard class and
  all related utility classes.

  Effort has been made to keep this class general. Please comment on that
  if you want to use it in your own project.

*/

#include <kapplication.h>

#include "kbgboard.h"
#include "kbgboard.moc"

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <kcolordialog.h>
#include <klocale.h>
#include <qlayout.h>
#include <qgroupbox.h>
#include <qbuttongroup.h>
#include <kconfig.h>
#include <qwhatsthis.h>
#include <qvbox.h>
#include <kiconloader.h>
#include <ktabctl.h>
#include <kpushbutton.h>
#include <kstdguiitem.h>

#include "version.h"


const int CUBE_UPPER = 3;
const int CUBE_LOWER = 4;

static const int MINIMUM_CHECKER_SIZE = 10;

/*
 * Set the default settings in all user configurations
 */
void KBgBoardSetup::setupDefault()
{
	// default background color
	setBackgroundColor(QColor(200, 200, 166));
	pbc_1->setPalette(QPalette(backgroundColor()));

	// checker colors
	baseColors[0] = black;
	baseColors[1] = white;
	pbc_2->setPalette(QPalette(baseColors[0]));
	pbc_3->setPalette(QPalette(baseColors[1]));

	// default font
	setFont(QFont("times", 18, QFont::Normal));
	kf->setFont(getFont());

	// short moves
	setShortMoveMode(SHORT_MOVE_DOUBLE);
	for (int i = 0; i < 3; i++)
		rbMove[i]->setChecked(i == SHORT_MOVE_DOUBLE);

	// pip count
	cbp->setChecked(computePipCount = true);
}

/*
 * User committed the changes. Save them.
 */
void KBgBoardSetup::setupOk()
{
	// font selection
	setFont(kf->font());

	// move strategy
	for (int i = 0; i < 3; i++)
		if (rbMove[i]->isChecked()) setShortMoveMode(i);

	// pipcount
	computePipCount = cbp->isChecked();
}

/*
 * User cancelled the changes. Undo the color changes that become
 * visible right away.
 */
void KBgBoardSetup::setupCancel()
{
	// undo background color change
	setBackgroundColor(saveBackgroundColor);

	// undo checker color changes
	baseColors[0] = saveBaseColors[0];
	baseColors[1] = saveBaseColors[1];

	for (int i = 0; i < 30; i++)
		cells[i]->update();
}

/*
 * Fills configuration page in the dialog nb
 */
void KBgBoardSetup::getSetupPages(KDialogBase *nb)
{
	/*
	 * Main Widget
	 * ===========
	 */
	QVBox *vbp = nb->addVBoxPage(i18n("Board"), i18n("Here you can configure the backgammon board"),
				     kapp->iconLoader()->loadIcon(PROG_NAME, KIcon::Desktop));

	/*
	 * Need more than one page
	 */
	KTabCtl *tc = new KTabCtl(vbp, "board tabs");

	QWidget *w = new QWidget(tc);
	QGridLayout *gl = new QGridLayout(w, 3, 1, nb->spacingHint());

	/*
	 * Group boxes
	 * ===========
	 */
	QGroupBox    *ga = new QGroupBox(w);
	QButtonGroup *gm = new QButtonGroup(w);
	QGroupBox    *go = new QGroupBox(w);

	ga->setTitle(i18n("Colors"));
	gm->setTitle(i18n("Short Moves"));
	go->setTitle(i18n("Options"));

	gl->addWidget(ga, 0, 0);
	gl->addWidget(gm, 1, 0);
	gl->addWidget(go, 2, 0);

	/*
	 * Appearance group
	 * ----------------
	 */
	QGridLayout *blc = new QGridLayout(ga, 2, 2, 20);

	pbc_1 = new QPushButton(i18n("Background"), ga);
	pbc_1->setPalette(QPalette(backgroundColor()));

	pbc_2 = new QPushButton(i18n("Color 1"), ga);
	pbc_2->setPalette(QPalette(baseColors[0]));

	pbc_3 = new QPushButton(i18n("Color 2"), ga);
	pbc_3->setPalette(QPalette(baseColors[1]));

	blc->addWidget(pbc_2, 0, 0);
	blc->addWidget(pbc_3, 0, 1);
	blc->addMultiCellWidget(pbc_1, 1, 1, 0, 1);

	connect(pbc_1, SIGNAL(clicked()), this, SLOT(selectBackgroundColor()));
	connect(pbc_2, SIGNAL(clicked()), this, SLOT(selectBaseColorOne()));
	connect(pbc_3, SIGNAL(clicked()), this, SLOT(selectBaseColorTwo()));

	/*
	 * Moving style
	 * ------------
	 */
	QBoxLayout *blm = new QVBoxLayout(gm, nb->spacingHint());

	blm->addSpacing(gm->fontMetrics().height());

	for (int i = 0; i < 3; i++)
		rbMove[i] = new QRadioButton(gm);

	rbMove[SHORT_MOVE_NONE]->setText(i18n("&Disable short moves. Only drag and drop will move."));
	rbMove[SHORT_MOVE_SINGLE]->setText(i18n("&Single clicks with the left mouse button will\n"
			  "move a checker the shortest possible distance."));
	rbMove[SHORT_MOVE_DOUBLE]->setText(i18n("D&ouble clicks with the left mouse button will\n"
			  "move a checker the shortest possible distance."));

	for (int i = 0; i < 3; i++) {
		rbMove[i]->setMinimumSize(rbMove[i]->sizeHint());
		blm->addWidget(rbMove[i]);
		rbMove[i]->setChecked(i == getShortMoveMode());
	}

	/*
	 * Other options
	 * -------------
	 */
	QGridLayout *glo = new QGridLayout(go, 1, 1, 20);

	cbp = new QCheckBox(i18n("Show pip count in title bar"), go);
	cbp->setChecked(computePipCount);
	cbp->adjustSize();
	cbp->setMinimumSize(cbp->size());

	glo->addRowSpacing(0, cbp->height());
	glo->addWidget(cbp, 0, 0);

	gl->activate();

	w->adjustSize();
	w->setMinimumSize(w->size());

	tc->addTab(w, i18n("&Board"));

	/*
	 * Save current settings
	 * ---------------------
	 */
	saveBackgroundColor = backgroundColor();
	saveBaseColors[0]   = baseColors[0];
	saveBaseColors[1]   = baseColors[1];

	/*
	 * Font selection page
	 * ===================
	 */
	w = new QWidget(tc);
	kf = new KFontChooser(w);
	kf->setFont(getFont());
	gl = new QGridLayout(w, 1, 1, nb->spacingHint());
	gl->addWidget(kf, 0, 0);
	gl->activate();
	w->adjustSize();
	w->setMinimumSize(w->size());
	tc->addTab(w, i18n("&Font"));
}

/*
 * Empty constructor calls the board constructor
 */
KBgBoardSetup::KBgBoardSetup(QWidget *parent, const char *name, QPopupMenu *menu)
	: KBgBoard(parent, name, menu)
{
	// empty
}

/*
 * User changed first checker color
 */
void KBgBoardSetup::selectBaseColorOne()
{
	KColorDialog *c = new KColorDialog(this, "base-col-1", true);
	c->setColor(baseColors[0]);
	if (c->exec()) {
		baseColors[0] = c->color();
		pbc_2->setPalette(QPalette(baseColors[0]));
		for (int i = 0; i < 30; i++)
			cells[i]->update();
	}
	delete c;
}

/*
 * User changed second checker color
 */
void KBgBoardSetup::selectBaseColorTwo()
{
	KColorDialog *c = new KColorDialog(this, "base-col-2", true);
	c->setColor(baseColors[1]);
	if (c->exec()) {
		baseColors[1] = c->color();
		pbc_3->setPalette(QPalette(baseColors[1]));
		for (int i = 0; i < 30; i++)
			cells[i]->update();
	}
	delete c;
}

/*
 * User changed background color
 */
void KBgBoardSetup::selectBackgroundColor()
{
	KColorDialog *c = new KColorDialog(this, "bg-col", true);
	c->setColor(backgroundColor());
	if (c->exec()) {
		setBackgroundColor(c->color());
		pbc_1->setPalette(QPalette(backgroundColor()));
		for (int i = 0; i < 30; i++)
			cells[i]->update();
	}
	delete c;
}

/*
 * Saves the persistent settings of the board
 */
void KBgBoard::saveConfig()
{
	KConfig* config = kapp->config();
	config->setGroup(name());

	config->writeEntry("bgcolor", backgroundColor());
	config->writeEntry("color-1", baseColors[0]);
	config->writeEntry("color-2", baseColors[1]);
	config->writeEntry("font", getFont());
	config->writeEntry("move", getShortMoveMode());
	config->writeEntry("pip", computePipCount);
}

/*
 * Restore the settings or use reasonable defaults
 */
void KBgBoard::readConfig()
{
	QColor col(200, 200, 166);
	QFont fon("times", 18, QFont::Normal);

	KConfig* config = kapp->config();
	config->setGroup(name());

	setBackgroundColor(config->readColorEntry("bgcolor", &col));
	baseColors[0] = config->readColorEntry("color-1", &black);
	baseColors[1] = config->readColorEntry("color-2", &white);
	setFont(config->readFontEntry("font", &fon));
	setShortMoveMode(config->readNumEntry("move", SHORT_MOVE_DOUBLE));
	computePipCount = config->readBoolEntry("pip", true);
}

/*
 * Get the font the board cells should use for the display of
 * numbers and cube value.
 */
QFont KBgBoard::getFont()
{
	return boardFont;
}

/*
 * Allows the users of the board classe to set the font to be used
 * on the board. Note that the fontsize is dynamically set
 */
void KBgBoard::setFont(const QFont& f)
{
	boardFont = f;
}

/*
 * Ask the user for an updated cube value
 */
void KBgBoard::queryCube()
{
	KBgStatus *st = new KBgStatus();
	getState(st);
	KBgBoardQCube *dlg =
		new KBgBoardQCube(abs(st->cube()), (st->cube(US) > 0), (st->cube(THEM) > 0));
	if (dlg->exec()) {
		bool u = ((dlg->getCubeValue() == 0) || (dlg->getCubeOwner() == US  ));
		bool t = ((dlg->getCubeValue() == 0) || (dlg->getCubeOwner() == THEM));
		st->setCube((int)rint(pow(2.0, dlg->getCubeValue())), u, t);
		setState(*st); // JENS
	}
	delete dlg;
	delete st;
}

/*
 * Constructor, creates the dialog but does not show nor execute it.
 */
KBgBoardQCube::KBgBoardQCube(int val, bool us, bool them)
	: QDialog(0, 0, true)
{
	setCaption(i18n("Set Cube Values"));

	QBoxLayout *vbox = new QVBoxLayout(this, 17);

	QLabel *info = new QLabel(this);

	cb[0]  = new QComboBox(this, "first sb");
	cb[1]  = new QComboBox(this, "second sb");
	ok     = new KPushButton(KStdGuiItem::ok(), this);
	cancel = new KPushButton(KStdGuiItem::cancel(), this);

	info->setText(i18n("Set the face value of the cube and select who should be able to\n"
			   "double. Note that a face value of 1 automatically allows both\n"
			   "players to double."));

	info->setMinimumSize(info->sizeHint());

	vbox->addWidget(info, 0);

	QBoxLayout *hbox_1 = new QHBoxLayout();
	QBoxLayout *hbox_2 = new QHBoxLayout();

	vbox->addLayout(hbox_1);
	vbox->addLayout(hbox_2);

	hbox_1->addWidget(cb[1]);
	hbox_1->addWidget(cb[0]);

	hbox_2->addWidget(ok);
	hbox_2->addWidget(cancel);

	cb[0]->insertItem(" 1", 0);
	cb[0]->insertItem(" 2", 1);
	cb[0]->insertItem(" 4", 2);
	cb[0]->insertItem(" 8", 3);
	cb[0]->insertItem("16", 4);
	cb[0]->insertItem("32", 5);
	cb[0]->insertItem("64", 6);

	switch(val) {
	case  1:
		cb[0]->setCurrentItem(0);
		break;
	case  2:
		cb[0]->setCurrentItem(1);
		break;
	case  4:
		cb[0]->setCurrentItem(2);
		break;
	case  8:
		cb[0]->setCurrentItem(3);
		break;
	case 16:
		cb[0]->setCurrentItem(4);
		break;
	case 32:
		cb[0]->setCurrentItem(5);
		break;
	case 64:
		cb[0]->setCurrentItem(6);
		break;
	}

	cb[1]->insertItem(i18n("Lower Player"), US);
	cb[1]->insertItem(i18n("Upper Player"), THEM);
	cb[1]->insertItem(i18n("Open Cube"), BOTH);

	if (us && them)
		cb[1]->setCurrentItem(BOTH);
	else if (us)
		cb[1]->setCurrentItem(US);
	else if (them)
		cb[1]->setCurrentItem(THEM);

	cb[0]->setMinimumSize(cb[0]->sizeHint());
	cb[1]->setMinimumSize(cb[1]->sizeHint());

	ok->setMinimumSize(ok->sizeHint());
	cancel->setMinimumSize(cancel->sizeHint());

	setMinimumSize(childrenRect().size());

	vbox->activate();

	resize(minimumSize());

	ok->setAutoDefault (true);
	ok->setDefault(true);

	cb[0]->setFocus();

	connect(ok,     SIGNAL(clicked()), SLOT(accept()));
	connect(cancel, SIGNAL(clicked()), SLOT(reject()));

	connect(cb[0],  SIGNAL(activated(int)), SLOT(changePlayer(int)));
	connect(cb[1],  SIGNAL(activated(int)), SLOT(changeValue (int)));
}

/*
 * Deconstructor, empty.
 */
KBgBoardQCube::~KBgBoardQCube()
{
	// nothing
}

/*
 * Get the face value of the cube
 */
int KBgBoardQCube::getCubeValue()
{
	return cb[0]->currentItem();
}

/*
 * Get the owner of the cube
 */
int KBgBoardQCube::getCubeOwner()
{
	return cb[1]->currentItem();
}

/*
 * If the cube is open, the value can only be 1
 */
void KBgBoardQCube::changeValue(int player)
{
	if (player == BOTH)
		cb[0]->setCurrentItem(0);

}

/*
 * If the value is 1, the cube has to be open; and if the value
 * becomes bigger than 1, the player cannot stay open.
 */
void KBgBoardQCube::changePlayer(int val)
{
	if (val == 0)
		cb[1]->setCurrentItem(BOTH);
	else if (cb[1]->currentItem() == BOTH)
		cb[1]->setCurrentItem(US);
}

/*
 * Constructor, creates the dialog but does not show nor execute it.
 */
KBgBoardQDice::KBgBoardQDice(const char *name)
	: QDialog(0, name, true)
{
	setCaption(i18n("Set Dice Values"));

	QBoxLayout *vbox = new QVBoxLayout(this, 17);

	QLabel *info = new QLabel(this);

	sb[0]  = new QSpinBox(this, "first sb");
	sb[1]  = new QSpinBox(this, "second sb");
	ok     = new KPushButton(KStdGuiItem::ok(), this);
	cancel = new KPushButton(KStdGuiItem::cancel(), this);

	info->setText(i18n("Set the face values of the selected dice. The other player's\n"
			   "dice will be cleared and it will be the dice's owner's turn."));

	info->setMinimumSize(info->sizeHint());

	vbox->addWidget(info, 0);

	QBoxLayout *hbox_1 = new QHBoxLayout();
	QBoxLayout *hbox_2 = new QHBoxLayout();

	vbox->addLayout(hbox_1);
	vbox->addLayout(hbox_2);

	hbox_1->addWidget(sb[0]);
	hbox_1->addWidget(sb[1]);

	hbox_2->addWidget(ok);
	hbox_2->addWidget(cancel);

	sb[0]->setMinimumSize(sb[0]->sizeHint());
	sb[1]->setMinimumSize(sb[1]->sizeHint());

	ok->setMinimumSize(ok->sizeHint());
	cancel->setMinimumSize(cancel->sizeHint());

	setMinimumSize(childrenRect().size());

	vbox->activate();

	resize(minimumSize());

	ok->setAutoDefault (true);
	ok->setDefault(true);

	sb[0]->setFocus();

	connect(ok,     SIGNAL(clicked()), SLOT(accept()));
	connect(cancel, SIGNAL(clicked()), SLOT(reject()));

	sb[0]->setValue(1);
	sb[1]->setValue(1);

	sb[0]->setRange(1, 6);
	sb[1]->setRange(1, 6);
}

/*
 * Deconstructor, empty.
 */
KBgBoardQDice::~KBgBoardQDice()
{
	// nothing
}

/*
 * Get the face value of the dice
 */
int KBgBoardQDice::getDice(int n)
{
	return sb[n]->value();
}

/*
 * Allows for overriding the current turn color in edit mode.
 */
void KBgBoard::storeTurn(const int pcs)
{
	storedTurn = ((pcs > 0) ? +1 : -1);
}

/*
 * Switch edit mode on/off
 */
void KBgBoard::setEditMode(const bool m)
{
	editMode = m;
}

/*
 * Retrurns the current edit mode status.
 */
bool KBgBoard::getEditMode() const
{
	return editMode;
}

/*
 * This function takes a KBgStatus object and fills it with the current
 * board status.
 */
KBgStatus* KBgBoard::getState(KBgStatus *st) const
{
	st->setColor(color);
	st->setDirection(direction);

	st->setCube(cube, maydouble[US], maydouble[THEM]);

	st->setBar(US, onbar[US]); st->setBar(THEM, onbar[THEM]);
	st->setHome(US, onhome[US]); st->setHome(THEM, onhome[THEM]);

	st->setDice(US, 0, dice[US][0]);
	st->setDice(US, 1, dice[US][1]);

	st->setDice(THEM, 0, dice[THEM][0]);
	st->setDice(THEM, 1, dice[THEM][1]);

	for (int i = 1; i < 25; ++i)
		st->setBoard(i, ((color*board[i] < 0) ? THEM : US), abs(board[i]));

	return st;
}

/*
 * This function lets external users change the context menu
 */
void KBgBoard::setContextMenu(QPopupMenu *menu)
{
	contextMenu = menu;
}

/*
 * This function prints all moves up to now in the extended FIBS command
 * notation (that is moves that involved kicking have a "+" instead of "-".
 */
void KBgBoard::sendMove()
{
	if (getEditMode())
		return;

	QString s, t;

	s.setNum(moveHistory.count());
	s += " ";

	QPtrListIterator<KBgBoardMove> it(moveHistory);
	for (; it.current(); ++it) {
		KBgBoardMove *move = it.current();
		if (move->source() == BAR_US || move->source() == BAR_THEM ) {
			s += "bar";
		} else {
			t.setNum(move->source());
			s += t;
		}
		if (move->wasKicked())
			s += "+";
		else
		        s += "-";

		if ((move->destination() != HOME_THEM_LEFT) && (move->destination() != HOME_THEM_RIGHT) &&
		    (move->destination() != HOME_US_LEFT  ) && (move->destination() != HOME_US_RIGHT )) {
			t.setNum(move->destination());
			s += t;
		} else {
			s += "off";
		}
		s += " ";
	}
	emit currentMove(&s);
}

/*
 * This is overloaded from QWidget, since it has to pass the new
 * background color to the child widgets (the cells).
 */
void KBgBoard::setBackgroundColor(QColor col)
{
	if (col != backgroundColor()) {
		QWidget::setBackgroundColor(col);
		for( int i = 0; i < 30; ++i)
			cells[i]->setBackgroundColor(col);
	}
}

/*
 * Overloaded from QWidget since we have to resize all cells
 */
void KBgBoard::resizeEvent(QResizeEvent *)
{
	int xo0 = 0;
	int xo1, w;
	int hu = height()/2;
	int hl = height() - hu;

        checkerDiam = (int)((width()/15-2)<(height()/10.0-2) ?
			    (width()/15-2) : (height()/10.0-2));

	if (checkerDiam < MINIMUM_CHECKER_SIZE) 
		checkerDiam = MINIMUM_CHECKER_SIZE;

	for (int i = 0; i < 14; ++i) {
		xo1  = int((i+1)*width()/15.0);
		w = xo1 - xo0;
		cells[i   ]->setGeometry(xo0,  0, w, hu);
		cells[i+15]->setGeometry(xo0, hu, w, hl);
		xo0 = xo1;
	}
	cells[14]->setGeometry(xo0,  0, width() - xo0, hu);
	cells[29]->setGeometry(xo0, hu, width() - xo0, hl);
}

/*
 * This function draws the whole board in black and white on the
 * painter *p. It is very well suited for printing on paper.
 * It scales the output according to the width of the widget.
 * I.e. if the widget is insanely long (y-direction) this will look
 * shitty. The upper 20% of the painter are not used. So the caller
 * can print whatever she/he wants above the 0.2*p->viewport().height()
 * margin (like game status information).
 */
void KBgBoard::print(QPainter *p)
{
	double sf = 0.8*p->viewport().width()/width();
	int xo = int((p->viewport().width() - sf*width())/2);
	int yo = int(0.2*p->viewport().height());
	int hu = height()/2;

	int xo0 = 0;
	for (int i = 0; i < 15; ++i) {
		cells[i   ]->paintCell(p, xo+sf*xo0, yo          , sf);
		cells[i+15]->paintCell(p, xo+sf*xo0, yo+sf*(hu-1), sf);
		xo0 = int((i+1)*width()/15.0);
	}
}

/*
 * This function returns the selected drawing color for a checker
 * of the given sign(!). I.e. we distinguish checkers by whether
 * they are negative or positive.
 */
QColor KBgBoard::getCheckerColor(int p) const
{
	return ((p < 0) ? baseColors[0] : baseColors[1]);
}

/*
 * This small utility function returns the y-coordinate base
 * of a checker. This is the offset in the y-coordinate at
 * which we have toposition the upper corner of the first
 * checker so that it is fully in the cell.
 */
int KBgBoardField::numberBase() const
{
	return (cellID < 13) ? 0 : height()-20;
}

/*
 * This function computes the proper diameter for checkers on this cell.
 * It tries to stay within the horizontal boundaries and adjusts the
 * diameter in such a way that 5 checkers fit on top of each other and
 * there is still some room for stacked checkers.
 */
int KBgBoardCell::getCheckerDiameter() const
{
	return board->checkerDiam;
}

/*
 * Draws the cells content using the painter p.
 * Reimplemented from QLabel.
 */
void KBgBoardCell::drawContents(QPainter *)
{
	QRect cr(0, 0, width(), height());
	cr.moveBottomLeft(rect().bottomLeft());
	QPixmap  pix(cr.size());
	QPainter tmp;
	pix.fill(this, cr.topLeft());
	tmp.begin(&pix);
	paintCell(&tmp);
	tmp.end();
	bitBlt(this, 0, 0, &pix);
	/*
	 * New state is now current.
	 * This avoids unnecessary redrawings.
	 */
	stateChanged = false;
}

/*
 * This does the absolute bare minimum of painting a cell. It draws a small
 * horizontal black line that marks the outer boundary of the cell and all
 * overloaded paintCell() member are supposed to call this one after(!) they
 * have painted themselves.
 */
void KBgBoardCell::paintCell(QPainter *p, int xo, int yo, double sf) const
{
	int x1 = xo; int x2 = xo;
	int y1 = yo; int y2 = yo;

	if ((cellID==HOME_THEM_LEFT || cellID==BAR_THEM) ||
	    (cellID<13 && cellID>0)) {
		x2 += int(sf*width());
	} else if ((cellID==HOME_US_LEFT || cellID==BAR_US) ||
		   (cellID<25 && cellID>12)) {
		x2 += int(sf*width());
		y1 = y2 += int(sf*(height()-1));
	} else if (cellID == HOME_THEM_RIGHT) {
		x2 += int(sf*(width()-1));
	} else if (cellID == HOME_US_RIGHT) {
		x2 += int(sf*(width()-1));
		y1 = y2 += int(sf*(height()-1));
	} else {
		return; // do nothing if the cellID is wrong
	}

	// draw line in black
	p->setBrush( black );
	p->setPen( black );
	p->drawLine(x1, y1, x2, y2);
}

/*
 * This function draws vertical boundaries around a cell. This is used
 * for bars and homes to get them separated from the rest of the board.
 */
void
KBgBoardCell::drawVertBorder(QPainter *p, int xo, int yo, double sf) const
{
	p->setBrush(black);
	p->setPen(black);
	p->drawLine(xo,                yo, xo,                yo+sf*(height()-1));
	p->drawLine(xo+sf*(width()-1), yo, xo+sf*(width()-1), yo+sf*(height()-1));
}

/*
 * This function draws the content of the homes on the painter *p. It
 * starts at the upper left corner (xo, yo) and uses the scaling factor
 * sf.
 */
void KBgBoardHome::paintCell(QPainter *p, int xo, int yo, double sf) const
{
	/*
	 * Only these homes contain checkers. The other ones contains dice and cube.
	 */
	if (((cellID == HOME_THEM_LEFT ) && (direction > 0)) ||
	    ((cellID == HOME_THEM_RIGHT) && (direction < 0)) ||
	    ((cellID == HOME_US_LEFT   ) && (direction > 0)) ||
	    ((cellID == HOME_US_RIGHT  ) && (direction < 0))) {

		drawOverlappingCheckers(p, xo, yo, sf);

	} else {

		drawDiceAndCube(p, ((cellID == HOME_THEM_LEFT ||
				     cellID == HOME_THEM_RIGHT) ?
				    THEM : US), xo, yo, sf);

	}

	/*
	 * Finally draw the boundaries
	 */
	drawVertBorder(p, xo, yo, sf);
	KBgBoardCell::paintCell(p, xo, yo, sf);
}

/*
 * This function draws the content of the bar cells. Bars may contain
 * checkers and the cube. Please read the comments in the code on how
 * and why the checkers and (especially) the cube is printed.
 */
void KBgBoardBar::paintCell(QPainter *p, int xo, int yo, double sf) const
{
	/*
	 * Put in the checkers.
	 */
	drawOverlappingCheckers(p, xo, yo, sf);

	/*
	 * Now comes a slightly tricky part: the cube belongs in the center
	 * of the board if nobody has doubled yet. In the way we do the board
	 * the center belongs to two(!) fields - both bars.
	 *
	 * If we are not printing on paper we use the fact that
	 * Qt will clip the drawing for us. So we print the upper
	 * half of the cube and the lower half on different cells.
	 *
	 * Since there is no such thing as clipping when we print
	 * on paper we can only print one cube. It turns out that
	 * the lower one is sufficiently centered.
	 */
	if (board->canDouble(US) &&
	    board->canDouble(THEM) &&
	    !(abs(xo)+abs(yo) > 0 && cellID == BAR_THEM)) {

		drawCube(p, cellID == BAR_THEM ? CUBE_UPPER :
			 CUBE_LOWER, xo, yo, sf);

	}

	/*
	 * Finally draw the boundaries
	 */
	drawVertBorder(p, xo, yo, sf);
       	KBgBoardCell::paintCell(p, xo, yo, sf);
}


/*
 * This function draws a cube on the painetr p. The cube will be drawn in
 * the coundaries given by cubeRect(...). The other parameters are like
 * in the other functions.
 */
void KBgBoardCell::drawCube(QPainter *p, int who, int xo, int yo,
				    double sf) const
{
	QRect r = cubeRect(who, true, sf);
	r.moveTopLeft(QPoint(xo+r.left(), yo+r.top()));

	p->setBrush(black);
	p->setPen(black);
	p->drawRoundRect(r, 20, 20);

	r = cubeRect(who, false, sf);
	r.moveTopLeft(QPoint(xo+r.left(), yo+r.top()));

	p->setBrush(white);
	p->setPen(white);
	p->drawRoundRect(r, 20, 20);

	p->setBrush(black);
	p->setPen(black);

	QString cubeNum;
	int v = board->getCube();
	/*
	 * Ensure that the cube shows 64 initially
	 */
	if (v == 1) v = 64;
	cubeNum.setNum(v);

	/*
	 * Adjust the font size
	 */
	QFont f = board->getFont();
	f.setPointSizeFloat(0.75*f.pointSizeFloat());
	p->setFont(f);
	p->drawText(r, AlignCenter, cubeNum);
}

/*
 * This function returns a boundary rectangle for the dice. It does so for both
 * dice (i is either 0 or 1). It can return big and small rectangles and everything
 * is scaled with a default value of 1.0. The scale parameter determines the the
 * size of the dice relative to the checker diameter.
 */
QRect KBgBoardCell::diceRect(int i, bool big, double sf, double scale) const
{
	int d = int(scale*getCheckerDiameter());
	int l = (1+width())%2;
	int k = (big ? 0 : 1);
	return(QRect(sf*(width()/2-d+k),
		     sf*(height()/2-2*d-3+2*i*(d+3)-1+k),
		     sf*(2*(d-k)+1-l),
		     sf*(2*(d-k)+1-l)));
}

/*
 * This function returns a bounding rectangle for the cube. This rectangle
 * is moved to the correct place and scaled correctly. The cube is slightly
 * smaller than the dice.
 */
QRect KBgBoardCell::cubeRect(int who, bool big, double sf) const
{
	QRect r = diceRect(0, big, sf, 0.40);

	int d = int(0.40*getCheckerDiameter());
	int h = r.height();
	int k = (big ? 1 : 0);

	switch (who) {
	case US:
		r.setTop(sf*(height() - 3*d) - k);
		break;
	case THEM:
		r.setTop(sf*d - k);
		break;
	case CUBE_UPPER:
		r.setTop(height()-d*sf - k);
		break;
	case CUBE_LOWER:
		r.setTop( -d*sf - k);
		break;
	default:
		return(QRect(0,0,0,0));
	}
	r.setHeight(h);
	return r;
}

/*
 * This function draws the face value on a given dice painter.
 * If the painting of dice should be saved this is the place
 * to modify.
 */
void KBgBoardHome::drawDiceFace(QPainter *p, int col, int num, int who,
					int xo, int yo, double sf) const
{
	p->setBrush(board->getCheckerColor(col));
	p->setPen(board->getCheckerColor(col));

	QRect r = diceRect(num, false, sf);
	r.moveTopLeft(QPoint(xo+r.left(), yo+r.top()));

	int cx  = r.width() /2;
	int cy  = r.height()/2;
	int cx2 = cx/2;
	int cy2 = cy/2;
	int cx7 = int(0.7*cx);
	int cy7 = int(0.7*cy);

	switch (board->getDice(who, num)) {
	case 5:
		p->drawEllipse(r.x()+cx-cx7  , r.y()+cy+cy7-1, 2, 2);
		p->drawEllipse(r.x()+cx+cx7-1, r.y()+cy-cy7  , 2, 2);
	case 3: // fall through
		p->drawEllipse(r.x()+cx-cx7  , r.y()+cy-cy7  , 2, 2);
		p->drawEllipse(r.x()+cx+cx7-1, r.y()+cy+cy7-1, 2, 2);
	case 1: // fall through
		p->drawEllipse(r.x()+cx      , r.y()+cy      , 2, 2);
		break;
	case 4:
		p->drawEllipse(r.x()+cx-cx2,   r.y()+cy+cy2-1, 2, 2);
		p->drawEllipse(r.x()+cx+cx2-1, r.y()+cy-cy2,   2, 2);
	case 2:  // fall through
		p->drawEllipse(r.x()+cx-cx2,   r.y()+cy-cy2,   2, 2);
		p->drawEllipse(r.x()+cx+cx2-1, r.y()+cy+cy2-1, 2, 2);
		break;
	case 6:
		p->drawEllipse(r.x()+cx-cx2,   r.y()+cy-cy7, 2, 2);
		p->drawEllipse(r.x()+cx-cx2,   r.y()+cy,     2, 2);
		p->drawEllipse(r.x()+cx-cx2,   r.y()+cy+cy7, 2, 2);
		p->drawEllipse(r.x()+cx+cx2-1, r.y()+cy-cy7, 2, 2);
		p->drawEllipse(r.x()+cx+cx2-1, r.y()+cy,     2, 2);
		p->drawEllipse(r.x()+cx+cx2-1, r.y()+cy+cy7, 2, 2);
		break;
	default: // nothing
		break;
	}
}

/*
 * This function draws a nice little square on the painter p.
 * The square is suited to contain a a face value as printed
 * by drawDiceFace(...).
 */
void KBgBoardHome::drawDiceFrame(QPainter *p, int col, int num,
					 int xo, int yo, bool big, double sf) const
{
	p->setBrush(board->getCheckerColor(col));
	p->setPen(board->getCheckerColor(col));
	QRect r = diceRect(num, big, sf);
	r.moveTopLeft(QPoint(xo+r.left(), yo+r.top()));
	p->drawRoundRect(r, 20, 20);
}

/*
 * If the event is left button we just store that. If the event is right
 * button we ask the board to possibly display the popup menu.
 */
void KBgBoardCell::mousePressEvent(QMouseEvent *e)
{
	if (e->button() == RightButton)
		board->showContextMenu();
	else
		mouseButton = e->button();
}

/*
 * This function sets the short move mode of the board.
 */
void KBgBoard::setShortMoveMode(int m)
{
	switch (m) {
	case SHORT_MOVE_NONE:
	case SHORT_MOVE_SINGLE:
		shortMoveMode = m;
		break;
	case SHORT_MOVE_DOUBLE:
	default:
		shortMoveMode = SHORT_MOVE_DOUBLE;
	}
}

/*
 * This function returns the currently selected short move mode.
 */
int KBgBoard::getShortMoveMode()
{
	return shortMoveMode;
}

/*
 * This function checks if (a) the mouse event was a left button,
 * (b) the parameter m equals the currently selected short move
 * mode and (c) t a short move from this field is possible. If all
 * tests are ok, the shortest possible move away from here is
 * made.
 */
void KBgBoardCell::checkAndMakeShortMove(QMouseEvent *e, int m)
{
	if ((e->button() == LeftButton) &&
	    (board->getShortMoveMode() == m) &&
	    (dragPossible()) &&
	    (!board->getEditMode()))
		makeShortMove();
}

/*
 * This functions reacts on a double click.
 */
void KBgBoardCell::mouseDoubleClickEvent(QMouseEvent *e)
{
	checkAndMakeShortMove(e, SHORT_MOVE_DOUBLE);
}

/*
 * This function reacts on a double click. Note that the bar knows
 * about two different double clicks: double the cube and make a
 * short move.
 */
void KBgBoardBar::mouseDoubleClickEvent(QMouseEvent *e)
{
	QRect r = cubeRect(cellID == BAR_THEM ? CUBE_UPPER : CUBE_LOWER, true);
	if (board->canDouble(US) &&
	    board->canDouble(THEM) && r.contains(e->pos())) {
		if (board->getEditMode())
			board->queryCube();
		else
			board->getDoubleCube(US);
		return;
	}
	checkAndMakeShortMove(e, SHORT_MOVE_DOUBLE);
}

/*
 * This is the destructor of the backgammon board. It frees
 * all resources previously allocated.
 */
KBgBoard::~KBgBoard()
{
	restoreCursor();
}

/*
 * This function draws dice and cube on the painter for a home cell.
 * who may be either US or THEM.
 */
void KBgBoardHome::drawDiceAndCube(QPainter *p, int who, int xo, int yo,
					   double sf) const
{
 	int col = ((who == THEM) ? -color : color);

	/*
	 * draw the empty squares and then put the face value in there
	 */
	for (int i = 0; i < 2; i++) {
		drawDiceFrame(p,-col, i, xo, yo,  true, sf);
		drawDiceFrame(p, col, i, xo, yo, false, sf);
		drawDiceFace(p,-col, i, who, xo, yo, sf);
	}
	/*
	 * if necessary draw the cube
	 */
	if (board->canDouble(who) &&
	    !(board->canDouble(US) && board->canDouble(THEM)))
		drawCube(p, who, xo, yo, sf);
}

/*
 * This function determines whether a drag off this home is possible.
 * This is only possible if there are checkers and edit mode is on.
 */
bool KBgBoardHome::dragPossible() const
{
	if (board->getEditMode())
		return (pcs != 0);
	return false;
}

/*
 * This function determines whether a drag off this bar is possible.
 * It checks in the follwoing order: (1) owner of this bar is the one
 * whose turn it is now, (2) does the board allow moving right now is
 * it in read-only mode?
 */
bool KBgBoardBar::dragPossible() const
{
	if (board->getEditMode())
		return (pcs != 0);

	switch(board->getTurn()) {
	case US:
		if (pcs*color <= 0) return false;
		break;
	case THEM:
		if (pcs*color >= 0) return false;
		break;
	default:
		return false;
	}
	return board->movingAllowed();
}

/*
 * This function checks whether a checker can be moved away from
 * this field. It first checks whether the owner of this field is
 * the one whose turn to move it it, then it is checked whether
 * the players bar is empty and finally it is checked if the board
 * is in read-only mode.
 */
bool KBgBoardField::dragPossible() const
{
	if (board->getEditMode())
		return (pcs != 0);

	switch(board->getTurn()) {
	case US:
		if (pcs*color <= 0) return false;
		break;
	case THEM:
		if (pcs*color >= 0) return false;
		break;
	default:
		return false;
	}
	if (board->getOnBar(board->getTurn()))
		return false;
	return board->movingAllowed();
}

/*
 * This function returns the current read-write flag of the board.
 * If this returns true the board doesn't accept user input. If
 * allowmoving is true we will accept user events.
 */
bool KBgBoard::movingAllowed() const
{
	return allowmoving;
}

/*
 * This function sets the read-write or read-only flag of the
 * board. See also movingAllowed().
 */
void KBgBoard::allowMoving(const bool fl)
{
	allowmoving = fl;
}

/*
 * This function returns the current pip count of the player w.
 */
int KBgBoard::getPipCount(const int& w) const
{
	if (!computePipCount || (w != US && w != THEM))
		return -1;
	int pip = 25*abs(onbar[w]);
	int d = ((w == US) ? 1 : -1);
	for (int i = 1; i < 25; i++) {
		if (d*board[i]*color > 0)
			pip += ((d*direction < 0) ?
				i*abs(board[i]) :
				(25 - i)*abs(board[i]));
	}
	return pip;
}

/*
 * This function handles double clicks on homes. It will ignore
 * double clicks on the real home and only handle the ones on the
 * "other" home - the one with the dice. It will propagate the event
 * only if the the click happened within the boundaries of a
 * dice or the cube.
 */
void KBgBoardHome::mouseDoubleClickEvent(QMouseEvent * e)
{
	if (e->button() != LeftButton)
		return;
	/*
	 * Check whether this is the bookkeeping home...
	 */
	if ((cellID == HOME_US_LEFT    && direction < 0) ||
	    (cellID == HOME_US_RIGHT   && direction > 0) ||
	    (cellID == HOME_THEM_LEFT  && direction < 0) ||
	    (cellID == HOME_THEM_RIGHT && direction > 0)) {

		int w = ((cellID == HOME_US_LEFT || cellID == HOME_US_RIGHT) ?
			 US : THEM);
		for (int i = 0; i < 2; ++i) {
			QRect r = diceRect(i, true);
			if (r.contains(e->pos())) {
				if (board->getEditMode()) {

					KBgBoardQDice *dlg = new KBgBoardQDice();
					if (dlg->exec()) {
						KBgStatus *st = new KBgStatus();
						board->getState(st);
						st->setDice(w, 0, dlg->getDice(0));
						st->setDice(w, 1, dlg->getDice(1));
						st->setDice(((w == US) ? THEM : US), 0, 0);
						st->setDice(((w == US) ? THEM : US), 1, 0);
						board->setState(*st); // JENS
						delete st;
					}
					delete dlg;

				} else
					board->getRollDice(w);
				return;
			}
		}
		if (board->canDouble(w) &&
		    !(board->canDouble(US) && board->canDouble(THEM))) {
			QRect r = cubeRect(w, true);
			if (r.contains(e->pos()))
				if (board->getEditMode())
					board->queryCube();
				else
					board->getDoubleCube(w);
		}
	}
}

/*
 * This function determines if a checker can be dropped on this field.
 * It checks whether the field is already owned, empty or contains
 * only one opponents piece. Then the dice are checked.
 */
bool KBgBoardField::dropPossible(int fromCellID, int newColor)
{
	if ((newColor*pcs > 0) || (pcs == 0) || (abs(pcs) == 1))
		// editMode is checked in diceAllowMove(...)
		return board->diceAllowMove(fromCellID, cellID);
	return false;
}

/*
 * This function determines if a checker can be dropped on this field.
 * Drops on the bar are never possible.
 */
bool KBgBoardBar::dropPossible(int fromCellID, int newColor)
{
	if (!board->getEditMode())
		return false;

	if (newColor*pcs > 0)
		return true;
	if ((cellID == BAR_US) && (board->getTurn() == US))
		return true;
	if ((cellID == BAR_THEM) && (board->getTurn() == THEM))
		return true;

	return (fromCellID == -12345); // always false
}

/*
 * This function checks if the current player can move a checker off.
 * Check if we can move a piece off. This obviously only works if there
 * are no pieces on the bar and all remaining pieces are in the home
 * board. This does not check the dice and it doesn't work for multiple
 * moves that start outside the home.
 */
bool KBgBoard::moveOffPossible() const
{
	if (getEditMode())
		return true;

	int w = getTurn();
	int d = ((w == THEM) ? -1 : 1);
	if (onbar[w] == 0 && d*direction > 0) {
		for (int i = 1; i < 19; ++i) {
			if (d*color*board[i] > 0) return false;
		}
		return true;

	} else if (onbar[w] == 0 && d*direction < 0) {
		for (int i = 24; i > 6; --i) {
			if (d*color*board[i] > 0) return false;
		}
		return true;
	}
	return false;
}

/*
 * This function tries to determine the field cell under the point p.
 * The point needs to be in board coordinates and the function returns
 * a pointer to the cell or NULL if there is no cell under the point.
 */

KBgBoardCell* KBgBoard::getCellByPos(const QPoint& p) const
{
       	for (int i = 0; i < 30; ++i) {
		if (cells[i]->rect().contains(cells[i]->mapFromParent(p)))
			return cells[i];
	}
	return NULL;
}

/*
 * This function takes a board number (1 to 24 and 0 or 25 depending on the
 * direction) or a cell ID and returns a pointer to the corresponding cell.
 * If the cell cannot be found it returns NULL.
 */
KBgBoardCell* KBgBoard::getCell(int num)
{
	switch (num) {
	case BAR_US:
		return (KBgBoardCell *)cells[22];
	case BAR_THEM:
		return (KBgBoardCell *)cells[ 7];
	case HOME_THEM_LEFT:
		return (KBgBoardCell *)cells[ 0];
	case HOME_THEM_RIGHT:
		return (KBgBoardCell *)cells[14];
	case HOME_US_LEFT:
		return (KBgBoardCell *)cells[15];
	case HOME_US_RIGHT:
		return (KBgBoardCell *)cells[29];
	default:
		int cell;
		if (num < 0 || num > 25)
			return NULL;
		else if (num < 7)
			cell = ((direction > 0) ? num      : 29 - num);
		else if (num < 13)
			cell = ((direction > 0) ? num + 1  : 28 - num);
		else if (num < 19)
			cell = ((direction > 0) ? 41 - num : num - 12);
		else
			cell = ((direction > 0) ? 40 - num : num - 11);
		return (KBgBoardCell *)cells[cell];
	}
}

/*
 * This function translates a field ID to the field number or just
 * returns the ID for bars and homes.
 */
int KBgBoard::IDtoNum(const int ID) const
{
	if (ID > 0 && ID < 25) {
		if (ID < 13)
			return ((direction > 0) ?      ID : 12 + ID);
		else
			return ((direction > 0) ? 37 - ID : 25 - ID);
	}
	return  ID;
}

/*
 * This function takes a checker from the cell if possible. It also
 * updates the bookkeeping of the board and redraws itself.
 */
bool KBgBoardCell::getPiece()
{
	if (pcs != 0) {
		((pcs > 0) ? --pcs : ++pcs);
		stateChanged = true;
		refresh();
		board->updateField(getNumber(), pcs);
		return true;
	}
	return false;
}

/*
 * This function stores the current cursor and replaces it with the
 * supplied one c.
 */
void KBgBoard::replaceCursor(const QCursor& c)
{
	if (savedCursor)
		delete savedCursor;
	savedCursor = new QCursor(cursor());
	setCursor(c);
}

/*
 * This function restores the previously set cursor to the stored one.
 */
void KBgBoard::restoreCursor()
{
	if (savedCursor) {
		setCursor(*savedCursor);
		delete savedCursor;
		savedCursor = NULL;
	}
}

/*
 * This function puts a checker of color newColor on the cell. It handles
 * all necessary updates including the kicking. It will however not properly
 * handle illegal moves!
 */
void KBgBoardCell::putPiece(int newColor)
{
	if (newColor*pcs > 0) {
		pcs > 0 ? ++pcs : --pcs;
	} else if (pcs == 0) {
		newColor > 0 ? pcs = 1 : pcs = -1;
	} else if (newColor*pcs < 0) {
		board->kickedPiece();
		newColor > 0 ? pcs = 1 : pcs = -1;
	}
	stateChanged = true;
	refresh();
	board->updateField(getNumber(), pcs);
	board->sendMove();
}

/*
 * This function handles mouse release events. It is important to know that
 * the cell where the first mousePressEvent occurred receives the release event.
 * The release event marks the end of a drag or a single click short move.
 */
void KBgBoardCell::mouseReleaseEvent(QMouseEvent *e)
{
 	if (dragInProgress) {

		KBgBoardCell *dest = board->getCellByPos
			(mapToParent(e->pos()));
		board->restoreCursor();
		if ((dest != NULL) && (dest->dropPossible(cellID, ((board->getTurn() == US) ?
								   color : -color)))) {
			if (!board->getEditMode())
				board->makeMove(getNumber(), dest->getNumber());
			dest->putPiece(((board->getTurn() == US) ? color : -color));
		} else {
			putPiece(((board->getTurn() == US) ? color : -color));
		}
		dragInProgress = false;

 	} else {

		checkAndMakeShortMove(e, SHORT_MOVE_SINGLE);

 	}
}

/*
 * This is the destructor of the home cells. It doesn't do anything.
 */
KBgBoardHome::~KBgBoardHome()
{
	//  nothing
}

/*
 * This is the destructor of the bar cells. It doesn't do anything.
 */
KBgBoardBar::~KBgBoardBar()
{
	// nothing
}

/*
 * This is the destructor of regular fields. It doesn't do anything.
 */
KBgBoardField::~KBgBoardField()
{
	// nothing
}

/*
 * This is the constructor of the bars. It calls the base class' constructor
 * and defines the QWhatsThis string.
 */
KBgBoardBar::KBgBoardBar(QWidget * parent, int numID)
	: KBgBoardCell(parent, numID)
{
	QWhatsThis::add(this, i18n("This is the bar of the backgammon board.\n\n"
				   "Checkers that have been kicked from the board are put "
				   "on the bar and remain there until they can be put back "
				   "on the board. Checkers can be moved by dragging them to "
				   "their destination or by using the 'short move' feature.\n\n"
				   "If the cube hasn't been doubled yet and if it can be used, "
				   "its face shows 64 and if the cube can be doubled, double "
				   "clicking it will do so."));
}

/*
 * This is the constructor of regular fields. It calls the base class' constructor
 * and defines the QWhatsThis string.
 */
KBgBoardField::KBgBoardField(QWidget * parent, int numID)
	: KBgBoardCell(parent, numID)
{
	QWhatsThis::add(this, i18n("This is a regular field of the backgammon board.\n\n"
				   "Checkers can be placed on this field and if the current state "
				   "of the game and the dice permit this, they can be moved by "
				   "dragging them to their destination or by using the 'short "
				   "move' feature."));
}

/*
 * This is the constructor of the homes. It calls the base class' constructor
 * and defines the QWhatsThis string.
 */
KBgBoardHome::KBgBoardHome(QWidget * parent, int numID)
	: KBgBoardCell(parent, numID)
{
	QWhatsThis::add(this, i18n("This part of the backgammon board is the home.\n\n"
				   "Depending on the direction of the game, one of the homes "
				   "contains the dice and the other one contains checkers that "
				   "have been moved off the board. Checkers can never be moved "
				   "away from the home. If this home contains the dice and the "
				   "current state of the game permits this, double clicking on "
				   "the dice will roll them. Moreover, the cube might be placed "
				   "on the home bar and if it can be doubled, double clicking it "
				   "will do so."));
	savedDice[0] = -1;
	savedDice[1] = -1;
}

/*
 * This function updates the number of checkers on the bar and also updates
 * the cell if the cube has changed (this is more often than necessary...)
 */
void KBgBoardBar::cellUpdate(const int p, const bool cubechanged)
{
	stateChanged = (cubechanged || colorChanged);
	if (pcs != p) {
		stateChanged = true;
		pcs = p;
	}
}

/*
 * This function updates the number of checkers on the field.
 */
void KBgBoardField::cellUpdate(const int p, const bool cubechanged)
{
	if (p != pcs) {
		pcs = p;
		stateChanged = true;
	}
	bool f = stateChanged; // useless, avoids compiler warning
	stateChanged = cubechanged;
	stateChanged = (f || colorChanged);
}

/*
 * This function updates the number of checkers on the home if it
 * actually contains checkers. It will also redraw if the cube or dice
 * have changed.
 */
void KBgBoardHome::cellUpdate(const int p, const bool cubechanged)
{
	if ((cellID == HOME_THEM_LEFT  && direction > 0) ||
	    (cellID == HOME_THEM_RIGHT && direction < 0) ||
	    (cellID == HOME_US_LEFT    && direction > 0) ||
	    (cellID == HOME_US_RIGHT   && direction < 0)) {

		if (pcs != p) {
			pcs = p;
			stateChanged = true;
		}

	} else {

		int who = ((cellID == HOME_THEM_LEFT || cellID == HOME_THEM_RIGHT) ? THEM : US);

		stateChanged = ((savedDice[0] != board->getDice(who, 0)) ||
				(savedDice[1] != board->getDice(who, 1)));

		savedDice[0] = board->getDice(who, 0);
		savedDice[1] = board->getDice(who, 1);

		stateChanged = (stateChanged || cubechanged || colorChanged || directionChanged);
	}
}

/*
 * This function returns whose players turn it is.
 */
int KBgBoard::getTurn() const
{
	if (getEditMode())
		return ((storedTurn*color > 0) ? US : THEM);

	if (getDice(US  , 0) != 0 && getDice(US  , 1) != 0)
		return   US;
	if (getDice(THEM, 0) != 0 && getDice(THEM, 1) != 0)
		return THEM;
	return -1;
}

/*
 * This is the constructor of the basic cells. It initializes the cell
 * to a sane state and connects a signal to the board.
 */
KBgBoardCell::KBgBoardCell(QWidget * parent, int numID)
	: QLabel(parent)
{
	board = (KBgBoard *)parent;

	direction        = +1;
	color            = -1;
	pcs              =  0;
	cellID           = numID;
	stateChanged     = false;
	colorChanged     = false;
	directionChanged = false;
	mouseButton      = NoButton;
	dragInProgress   = false;

	connect(parent, SIGNAL(finishedUpdate()), this, SLOT(refresh()));
}

/*
 * This is the destructor of the cells. It doesn't do anything.
 */
KBgBoardCell::~KBgBoardCell()
{
	// nothing
}

/*
 * This function returns the color of the checkers on this cell.
 */
int KBgBoardCell::getCellColor()
{
	return ((pcs < 0) ? -1 : +1);
}

/*
 * This function updates the basic board settings color and direction
 * and signals a redraw if necessary.
 */
void KBgBoardCell::statusUpdate(int dir, int col)
{
	if (direction != dir || color != col) {
		colorChanged     = (color != col);
		directionChanged = (direction != dir);
		color        = col;
		direction    = dir;
		stateChanged = true;
	}
}

/*
 * This function refreshes the content of the cell if necessary.
 */
void KBgBoardCell::refresh()
{
	if (stateChanged) {
		update();
		stateChanged     = false;
		colorChanged     = false;
		directionChanged = false;
	}
}

/*
 * This function returns the board number of this cell as given by the board.
 */
int KBgBoardCell::getNumber() const
{
	return board->IDtoNum(cellID);
}

/*
 * This function returns the number of checkers of player who on the bar.
 */
int KBgBoard::getOnBar(int who) const
{
	return ((who == US || who == THEM) ? onbar[who] : 0);
}

/*
 * This function returns the face value of the n-th dice of player w
 */
int KBgBoard::getDice( int w, int n ) const
{
	return (((w == US || w == THEM) && (n == 0 || n == 1)) ? dice[w][n] : 0);
}

/*
 * This function returns the current cube value.
 */
int KBgBoard::getCube() const
{
	return cube;
}

/*
 * This function updates the stored number of pieces on field f to v.
 */
void KBgBoard::updateField(int f, int v)
{
	switch (f) {
	case BAR_US:
	case BAR_THEM:
		onbar[((f == BAR_US) ? US : THEM)] = v;
		break;
	case HOME_US_RIGHT:
	case HOME_US_LEFT:
		onhome[US] = v;
		break;
	case HOME_THEM_RIGHT:
	case HOME_THEM_LEFT:
		onhome[THEM] = v;
		break;
	default:
		if (0 < f && f < 25)
			board[f] = v;
		break;
	}
}

/*
 * This function displays the context menu our parent may have given us
 */
void KBgBoard::showContextMenu()
{
	if (contextMenu) contextMenu->popup(QCursor::pos());
}

/*
 * This function determines if the player who can double.
 */
bool KBgBoard::canDouble(int who) const
{
	return ((who == US || who == THEM) ? maydouble[who] : false);
}

/*
 * This function is a simple utility for makeMove. It takes care
 * of all the bookeeeping needed for a move.
 */
int KBgBoard::makeMoveHelper(int si, int sf, int delta)
{
	moveHistory.append(new KBgBoardMove(si, sf, abs(delta)));
	--possMoves[abs(delta)];
	return delta;
}

/*
 * This function makes a move from src to dest for the current player.
 * It can handle illegal moves but the move should have been checked.
 */
void KBgBoard::makeMove(int src, int dest)
{
	int m[4];
	int l;

	int d = direction*((getTurn() == US) ? +1 : -1);

	if (src == BAR_US || src == BAR_THEM ) {

		int start = ((d > 0) ? 0 : 25);
		l = checkMultiMove(start, dest, m);
		moveHistory.append(new KBgBoardMove(src, start+d*m[0], m[0]));
		src = start+d*m[0];
		--possMoves[m[0]];
		for (int i = 1; i < l; i++)
			src += makeMoveHelper(src, src+d*m[i], d*m[i]);

	} else if (0 < src && src < 25 && 0 < dest && dest < 25) {

		l = checkMultiMove(src, dest, m);
		for (int i = 0; i < l; i++)
			src += makeMoveHelper(src, src+d*m[i], d*m[i]);

	} else {

		int s = src;
		int final = ((d > 0) ? 25 : 0);
		while (((l = checkMultiMove(s, final, m)) == 0) && (0 < s && s < 25))
			s -= d;

		for (int i = 0; i < l-1; i++)
			src += makeMoveHelper(src, src+d*m[i], d*m[i]);

		moveHistory.append(new KBgBoardMove(src, dest, ((d > 0) ? 25 - src : src)));
		--possMoves[m[l-1]];

	}
}

/*
 * This function checks if there is any possibility (based on the dice)
 * to move from src to dest. It takes the ownership of the intermediate
 * fields into account. The function returns the number of steps necessary
 * to perform the move (or 0 if the move is not possible) and the actual
 * dice values used for the steps are returned in the array m.
 *
 * The values src and dest are expected to be in board coordinates and the
 * homes and/or bars should already be mapped to the corresponding values
 * 0 and 25 (based onb direction and whose turn it is).
 */
int KBgBoard::checkMultiMove(int src, int dest, int m[4])
{
	m[0] = 0; m[1] = 0; m[2] = 0; m[3] = 0;

	int mcolor = ((getTurn() == US) ? color : -color);
	int d      = ((src > dest) ? -1 : 1);

	/*
	 * These are very easy special cases: move length is 0 or
	 * player cannot move to the destination field.
	 */
	if ((src == dest) || (mcolor*board[dest] < -1)) return 0;

	int diceToUse[4];
	int dice = 0;
	/*
	 * Get the available step sizes for this move
	 */
	for (int i = 1; i < 7; i++) {
		for (int j = 0; j < possMoves[i]; j++) {
			diceToUse[dice++] = i;
			/*
			 * If this happens there is something wrong
			 */
			if (dice > 4) return 0;
		}
	}
	/*
	 * And start all possible combination of dices.
	 */
	switch (dice) {
	case 4:	if (src+4*d*diceToUse[0] == dest) {
		if ((mcolor*board[src+1*d*diceToUse[0]] >=  0) &&
		    (mcolor*board[src+2*d*diceToUse[0]] >=  0) &&
		    (mcolor*board[src+3*d*diceToUse[0]] >=  0)) {
			m[0] = m[1] = m[2] = m[3] = diceToUse[0];
			return 4;
		}
	}
	case 3:	if (src+3*d*diceToUse[0] == dest) {
		if ((mcolor*board[src+1*d*diceToUse[0]] >=  0) &&
		    (mcolor*board[src+2*d*diceToUse[0]] >=  0)) {
			m[0] = m[1] = m[2] = diceToUse[0];
			return 3;
		}
	}
	case 2: if ((src+d*(diceToUse[0]+diceToUse[1])) == dest) {
		if (mcolor*board[src+d*diceToUse[0]] >= 0) {
			m[0] = diceToUse[0];
			m[1] = diceToUse[1];
			return 2;
		}
		if (mcolor*board[src+d*diceToUse[1]] >= 0) {
			m[0] = diceToUse[1];
			m[1] = diceToUse[0];
			return 2;
		}
	}
	case 1:	if (abs(src-dest) < 7 && possMoves[abs(src-dest)] > 0) {
		m[0] = abs(src-dest);
		return 1;
	}
	default: return 0;
	}
}

/*
 * This function determines if a checker can be dropped on this home field.
 * It first checks whether this is the proper of the four home fields (belongs
 * to the player and not the one with dice and cube). Then we check if the move
 * itself is possible.
 */
bool KBgBoardHome::dropPossible(int fromCellID, int newColor)
{
	if ((cellID==HOME_US_LEFT && board->getTurn() == US   && direction > 0) ||
	    (cellID==HOME_THEM_LEFT && board->getTurn() == THEM && direction > 0) ||
	    (cellID==HOME_US_RIGHT && board->getTurn() == US   && direction < 0) ||
	    (cellID==HOME_THEM_RIGHT && board->getTurn() == THEM && direction < 0))
		return (board->moveOffPossible() &&
			board->diceAllowMove(fromCellID, cellID));
	return (newColor == -12345); // always false
}

/*
 * This function is a simple boolean interface to checkMultiMove.
 * It takes car of directions and bar/home mappings. If necessary
 * it also handles the case of bearing off.
 */
bool KBgBoard::diceAllowMove(int src, int dest)
{
	int m[4];
	int w = getTurn();
	int k = ((w == US) ? +1 : -1);
	int t = ((k*direction > 0) ? 25 : 0);
	int d = ((k*direction > 0) ? +1 : -1);

	if (getEditMode())
		return true;

	if ((w == US && src == BAR_US) || (w == THEM && src == BAR_THEM)) {
		/*
		 * Move comes from a bar. Hence it has to end on a field
		 * and not on bars or homes. If there are checkers left
		 * on the bar we don't accept multi moves.
		 */
		if (0 < dest && dest < 25) {
			int r = checkMultiMove((k*direction > 0) ? 0 : 25,
					       IDtoNum(dest), m);
			return((abs(onbar[w]) == 0) ? (r != 0) : (r == 1));
		} else {
			return false;
		}
	} else if (0 < dest && dest < 25 && 0 < src && src < 25) {
		/*
		 * Move from a field to a field
		 */
		if (direction*k*(IDtoNum(dest)-IDtoNum(src)) > 0) {
			return(checkMultiMove(IDtoNum(src), IDtoNum(dest), m));
		} else {
			return false;
		}
	} else {
		/*
		 * Move from a field on the home. First we try exact dice.
		 */
		if (checkMultiMove(IDtoNum(src), t, m) > 0) return true;

 		/*
  		 * Then maybe we could bear the checker off ?
  		 */
		int i = IDtoNum(src);
 		while (0 < i && i < 25) {
			i -= d;
 			if (k*color*board[i] > 0) return false;
 		}

		/*
		 * Indeed we are bearing off. So find the highest dice and use it.
		 * Start from all the way back to catch double 6 from the start.
		 */
		int j = 24;
		while (checkMultiMove(t-d*j, t, m) == 0 && j > 0) {--j;}
		return (j >= t-d*IDtoNum(src));
	}
	return false;
}

/*
 * This is the most important of all members of the board class. It takes
 * a single board status object and initializes the internal status.
 */
void KBgBoard::setState(const KBgStatus &st)
{
	color     = st.color();
	direction = st.direction();

	cubechanged = (cube != abs(st.cube()));
	cube        = abs(st.cube());
	maydouble[US  ] = (st.cube(US  ) > 0);
	maydouble[THEM] = (st.cube(THEM) > 0);

	for (int i = 0; i < 30; i++)
		cells[i]->statusUpdate(direction, color);

	for (int i = 1; i < 25; ++i)
		board[i] = st.board(i);

	onbar[US  ] = st.bar(US  );
	onbar[THEM] = st.bar(THEM);

	onhome[US]   = st.home(US  );
	onhome[THEM] = st.home(THEM);

	dice[US  ][0]    = st.dice(US  , 0);
	dice[US  ][1]    = st.dice(US  , 1);
	dice[THEM][0]    = st.dice(THEM, 0);
	dice[THEM][1]    = st.dice(THEM, 1);

	for (int i = 0; i < 7; ++i)
		possMoves[i] = 0;

	int w = getTurn();
	if (getEditMode())
		w = ((dice[US][0] && dice[US][1]) ? US : THEM);

	if (w == US || w == THEM) {
		++possMoves[dice[w][0]];
		++possMoves[dice[w][1]];
		if (dice[w][0] == dice[w][1])
			possMoves[dice[w][0]] *= 2;
	}

	board[ 0] = 0;
	board[25] = 0;
	for (int i=1; i<25; ++i)
		(getCell(i))->cellUpdate(board[i]);

	(getCell(BAR_US  ))->cellUpdate(st.bar(US  ), cubechanged);
	(getCell(BAR_THEM))->cellUpdate(st.bar(THEM), cubechanged);

	(getCell(HOME_US_LEFT   ))->cellUpdate(st.home(US  ), cubechanged);
	(getCell(HOME_US_RIGHT  ))->cellUpdate(st.home(US  ), cubechanged);
	(getCell(HOME_THEM_LEFT ))->cellUpdate(st.home(THEM), cubechanged);
	(getCell(HOME_THEM_RIGHT))->cellUpdate(st.home(THEM), cubechanged);

	moveHistory.clear();
	redoHistory.clear();

	emit finishedUpdate();
}

/*
 * This function starts a drag from this cell if possible. It asks the board to
 * change the mouse pointer and takes a checker away from this cell.
 */
void KBgBoardCell::mouseMoveEvent(QMouseEvent *)
{
	if ((mouseButton == LeftButton) && dragPossible()) {
		dragInProgress = true;
		QRect cr(0, 0, 1+getCheckerDiameter(), 1+getCheckerDiameter());
		cr.moveBottomLeft(rect().bottomLeft());
		QPixmap pix(cr.size());
		QPainter tmp;
		pix.fill(this, cr.topLeft());
		tmp.begin(&pix);
		board->drawSimpleChecker(&tmp, 0, 0, pcs, getCheckerDiameter());
		tmp.end();
		pix.setMask(pix.createHeuristicMask());
		QBitmap mask = *(pix.mask());
		QBitmap newCursor;
		newCursor = pix;
		board->replaceCursor(QCursor(newCursor, mask));
		if (board->getEditMode())
			board->storeTurn(pcs);
		getPiece();
	}
	mouseButton = NoButton;
}

/*
 * This function draws a checker on the painter p. It is painted
 * in the ractangle with the upper left corner (x,y) and has a
 * maximum diameter of diam. This checker has only two colors and
 * as such it is suited for the mouse cursor and printing.
 */
void KBgBoard::drawSimpleChecker(QPainter *p, int x, int y, int pcs,
					 int diam) const
{
	p->setBrush(getCheckerColor(pcs));
	p->setPen(getCheckerColor(pcs));
	p->drawEllipse(x+1, y+0, diam-0, diam-0);
	p->setBrush(getCheckerColor(-pcs));
	p->setPen(getCheckerColor(-pcs));
	p->drawEllipse(x+2, y+1, diam-2, diam-2);
	p->setBrush(getCheckerColor(pcs));
	p->setPen(getCheckerColor(pcs));
	p->drawEllipse(x+3, y+2, diam-4, diam-4);
}

/*
 * This function draws an anti-aliased checker on the painter p. It
 * is painted in the ractangle with the upper left corner (x,y) and
 * has a diameter of diam. col indicates the color of the cell this
 * checker is painted on. Special values for col are 0 and 100 that
 * indicate that the checker is stacked (bars and homes) or stacked
 * on a field respectively. upper indicates whether the checker is
 * in the upper half of the board or not.
 */
void KBgBoard::drawChecker(QPainter *p, int x, int y, int pcs, int diam,
				   int col, bool upper) const
{
	drawCircle(p, x,   y,   pcs, diam  , col, upper, true );
	drawCircle(p, x+1, y+1,-pcs, diam-2, col, upper, false);
	drawCircle(p, x+2, y+2, pcs, diam-4, col, upper, false);
}

/*
 * This function draws checkers on the painter *p. They overlap so that
 * up to fifteen checkers fit on the cell. This is used by homes and
 * bars.
 */
void KBgBoardCell::drawOverlappingCheckers(QPainter *p, int xo, int yo,
						   double sf) const
{
	int d = getCheckerDiameter();
	bool upper =
	  cellID == HOME_THEM_LEFT  ||
	  cellID == HOME_THEM_RIGHT ||
	  cellID == BAR_THEM;
	double xp = xo + sf*((width()-d-1)/2);
	double ra = sf*d;
	for (int i = 0; i < abs(pcs); ++i) {
		double yp = yo + (upper ? 1+i*sf*height()/25.0 :
				  sf*(height()-d-i*height()/25.0));
		board->drawChecker(p, xp, yp, pcs, ra, 0, upper);
	}
}

/*
 * This function paints the content of a regular cell on the painter p.
 * It does so by first drawing a triangle (depending on whether we draw
 * on the screen or not this will be antialiased). Then on top of that
 * we draw the field number in inverse color. Finally we draw all the
 * checkers in such a way that always five are in one level and the next
 * level is slightly shifted.
 */
void KBgBoardField::paintCell(QPainter *p, int xo, int yo, double sf) const
{
	QColor color, alphaColor, background = backgroundColor();
	bool printing = abs(xo)+abs(yo) > 0;

	if (printing) {
		/*
		 * This is the code for black and white printing on
		 * paper. This justs draws a triangle and surrounds
		 * it by a black triangle. Easy but works.
		 */
		QPointArray pa(3);

		color = (getNumber()%2 ? white : black);

		if (cellID < 13) {
			pa.setPoint( 0, xo               , yo                  );
			pa.setPoint( 1, xo + sf*width()/2, yo + 0.9*sf*height());
			pa.setPoint( 2, xo + sf*width()  , yo                  );
		} else {
			pa.setPoint( 0, xo               , yo + sf*(height()-1));
			pa.setPoint( 1, xo + sf*width()/2, yo + 0.1*sf*height());
			pa.setPoint( 2, xo + sf*width()  , yo + sf*(height()-1));
		}

		p->setBrush(color);
		p->setPen(color);
		p->drawPolygon(pa);

		p->setBrush(black);
		p->setPen(black);
		p->drawPolyline(pa);

	} else {
		/*
		 * This is the code for antialiased triangles. This code has
		 * been written by Bo Thorsen.
		 */
		color = board->getCheckerColor(getNumber()%2-1);

		int topX, topY, bottomX1, bottomX2, bottomY, incrY;
		topX = xo + (int)(sf*width()/2.0);
		bottomX1 = xo;
		bottomX2 = xo + (int)(sf*width());
		if (cellID < 13) {
			topY = yo + (int)(0.9*sf*height());
			bottomY = yo;
			incrY = 1;
		} else {
			topY = yo + (int)(0.1*sf*height());
			bottomY = yo + (int)(sf*height());
			incrY = -1;
		}

		float x1 = bottomX1, x2 = bottomX2;
		float dx1 = (float)(topX-bottomX1) / (topY-bottomY);
		float dx2 = (float)(topX-bottomX2) / (topY-bottomY);
		if (dx1 < 0) dx1 = -dx1;
		if (dx2 < 0) dx2 = -dx2;

		p->setPen( color );
		p->drawLine(bottomX1, bottomY, bottomX2, bottomY);
		x1 += dx1;
		x2 -= dx2;

		/*
		 * The scaling factor (0.99) cuts off the top op the points
		 */
		for (int y=bottomY; x1 < x2*0.99; y+=incrY) {
			int ix1 = (int)x1, ix2 = (int)x2;
			float a1 = x1 - ix1, a2 = x2 - ix2;

			/*
			 * This is a simple linear interpolation between
			 * the two colors
			 */
			int red1 = (int)
				((1-a1)*color.red() + a1*background.red());
			int green1 = (int)
				((1-a1)*color.green() + a1*background.green());
			int blue1 = (int)
				((1-a1)*color.blue() + a1*background.blue());
			int red2 = (int)
				(a2*color.red() + (1-a2)*background.red());
			int green2 = (int)
				(a2*color.green() + (1-a2)*background.green());
			int blue2 = (int)
				(a2*color.blue() + (1-a2)*background.blue());

			/*
			 * Draw the antialiasing pixels
			 */
			alphaColor.setRgb(red1, green1, blue1);
			p->setPen(alphaColor);
			p->drawPoint(ix1, y);
			alphaColor.setRgb(red2, green2, blue2);
			p->setPen(alphaColor);
			p->drawPoint(ix2, y);

			ix1++;
			ix2--;
			x1 += dx1;
			x2 -= dx2;

			if (ix1 <= ix2 && x1 < x2*0.99) {
				/*
				 * Draw the line
				 */
				p->setPen(color);
				p->drawLine(ix1, y, ix2, y);
			}
		}
	}

	/*
	 * Print the field number in inverted color
	 */
	color = board->getCheckerColor((1+getNumber())%2-1);

	p->setBrush(color);
	p->setPen(color);

	QString t;
	t.setNum(getNumber());

	p->setFont(board->getFont());
	int textHeight = QFontMetrics(p->font()).height();
	p->drawText(xo, yo+((cellID < 13) ? 5 : height()-5-textHeight),
		    width()*sf, textHeight, AlignCenter, t);

	/*
	 * Put the checkers on the field.
	 */
	int d = getCheckerDiameter();
	double yp, xp = xo + sf*((width()-d-1)/2);
	double ra = sf*d;
	bool upper = cellID < 13;
	int col = (getNumber()%2) ? 1 : -1;

	for (int i = 0; i < abs(pcs); ++i) {
		/*
		 * There is hard work in these formulas. Unless you have
		 * tried _ALL_ possible windowsizes: don't touch!
		 */
		yp = yo + (upper ? sf*((i%5)+(i/5)/4.0)*(d-1) :
			   sf*(height()-((1+i%5)*d)-int(i/5)*0.25*d)-1);
		if (printing) {
			board->drawSimpleChecker(p, xp, yp, pcs, ra);
		} else {
			board->drawChecker(p, xp, yp, pcs, ra,
					   ((i < 5) ? col : 100), upper);
		}
	}

	/*
	 * Finally draw the horizontal boundaries
	 */
	KBgBoardCell::paintCell(p, xo, yo, sf);
}

/*
 * This function draws an anti-aliased circle on the painter p. It is painted
 * in the ractangle with the upper left corner (x,y) and has a maximum diameter
 * of diam. col and upper are as in drawChecker(). outer indicates if this
 * circle blends with the background. Note that this function needs knowledge
 * about the triangles on the cells. This is long but it is just a big if
 * construct.
 */
void KBgBoard::drawCircle(QPainter *p, int x, int y, int pcs, int diam,
				  int col, bool upper, bool outer) const
{
	QColor fColor = getCheckerColor(pcs);
	QColor alphaColor;
	QColor bColor;

	int red, green, blue;
	int rad = diam/2;
	int xoff = 0;

	float sn = 4;
	float rs = 0.25*diam*diam;
	float cf, a;

	for (int ys = rad; ys >= 0; ys--) {
		for (int xs = xoff; cf = 0, xs < rad; xs++) {

			/*
			 * perform super-sample this pixel
			 */
			for (int s1 = 0; s1 < sn; s1++)
				for (int s2 = 0; s2 < sn; s2++)
					if ((rad-xs+s1/sn)*(rad-xs+s1/sn)+
					    (rad-ys+s2/sn)*(rad-ys+s2/sn) < rs)
						cf += 1;
			a = cf/sn/sn;

			if (outer && (col == 0 || col == 100)) {

				if (col == 0)
					bColor = backgroundColor();
				else
					bColor = fColor;

				red   = (int)
					((1-a)*bColor.red()+a*fColor.red());
				green = (int)
					((1-a)*bColor.green()+a*fColor.green());
				blue  = (int)
					((1-a)*bColor.blue()+a*fColor.blue());

				alphaColor.setRgb(red, green, blue);

				p->setBrush(alphaColor);
				p->setPen(alphaColor);

				if (upper) {

					p->drawPoint(x+xs, y+diam-ys);
					p->drawPoint(x+diam-xs, y+diam-ys);

					p->setBrush(fColor);
					p->setPen(fColor);

					p->drawPoint(x+xs, y+ys);
					p->drawPoint(x+diam-xs, y+ys);

				} else {

					p->drawPoint(x+xs, y+ys);
					p->drawPoint(x+diam-xs, y+ys);

					p->setBrush(fColor);
					p->setPen(fColor);

					p->drawPoint(x+xs, y+diam-ys);
					p->drawPoint(x+diam-xs, y+diam-ys);

				}

			} else if (outer) {

				if (upper) {

					bColor = getCheckerColor(col);

					red   = (int)((1-a)*bColor.red()+
						      a*fColor.red());
					green = (int)((1-a)*bColor.green()+
						      a*fColor.green());
					blue  = (int)((1-a)*bColor.blue()+
						      a*fColor.blue());

					alphaColor.setRgb(red, green, blue);

					p->setBrush(alphaColor);
					p->setPen(alphaColor);

					p->drawPoint(x+xs, y+ys);
					p->drawPoint(x+diam-xs, y+ys);
					p->drawPoint(x+xs, y+diam-ys);
					p->drawPoint(x+diam-xs, y+diam-ys);

					bColor = backgroundColor();

					red   = (int)((1-a)*bColor.red()+
						      a*fColor.red());
					green = (int)((1-a)*bColor.green()+
						      a*fColor.green());
					blue  = (int)((1-a)*bColor.blue()+
						      a*fColor.blue());

					alphaColor.setRgb(red, green, blue);
					p->setBrush(alphaColor);
					p->setPen(alphaColor);

					if (x+xs < rad*(y+ys)/(0.45*height())) {
						p->drawPoint(x+xs, y+ys);
						p->drawPoint(x+diam-xs, y+ys);
					}
					if (x+xs<rad*(y+diam-ys)/(0.45*height())) {
						p->drawPoint(x+xs, y+diam-ys);
						p->drawPoint(x+diam-xs, y+diam-ys);
					}

				} else {

					bColor = getCheckerColor(col);

					red   = (int)((1-a)*bColor.red()+
						      a*fColor.red());
					green = (int)((1-a)*bColor.green()+
						      a*fColor.green());
					blue  = (int)((1-a)*bColor.blue()+
						      a*fColor.blue());

					alphaColor.setRgb(red, green, blue);
					p->setBrush(alphaColor);
					p->setPen(alphaColor);

					p->drawPoint(x+xs, y+ys);
					p->drawPoint(x+diam-xs, y+ys);
					p->drawPoint(x+xs, y+diam-ys);
					p->drawPoint(x+diam-xs, y+diam-ys);

					bColor = backgroundColor();

					red   = (int)((1-a)*bColor.red()+
						      a*fColor.red());
					green = (int)((1-a)*bColor.green()+
						      a*fColor.green());
					blue  = (int)((1-a)*bColor.blue()+
						      a*fColor.blue());

					alphaColor.setRgb(red, green, blue);
					p->setBrush(alphaColor);
					p->setPen(alphaColor);

					if (x+xs<rad*(0.5-(y+ys)/
						      (1.0*height()))/0.45) {
						p->drawPoint(x+xs, y+ys);
						p->drawPoint(x+diam-xs, y+ys);
					}
					if (x+xs < rad*(0.5-(y+diam-ys)/
							(1.0*height()))/0.45) {
						p->drawPoint(x+xs, y+diam-ys);
						p->drawPoint(x+diam-xs, y+diam-ys);
					}
				}

			} else {

				bColor = getCheckerColor(-pcs);

				red   = (int)((1-a)*bColor.red()+
					      a*fColor.red());
				green = (int)((1-a)*bColor.green()+
					      a*fColor.green());
				blue  = (int)((1-a)*bColor.blue()+
					      a*fColor.blue());

				alphaColor.setRgb(red, green, blue);
				p->setBrush(alphaColor);
				p->setPen(alphaColor);

				p->drawPoint(x+xs, y+ys);
				p->drawPoint(x+diam-xs, y+ys);
				p->drawPoint(x+xs, y+diam-ys);
				p->drawPoint(x+diam-xs, y+diam-ys);

			}

			if (fabs(cf-sn*sn) < 0.0001) {

				p->moveTo(x+xs, y+ys);
				p->lineTo(x+diam-xs, y+ys);
				p->moveTo(x+xs, y+diam-ys);
				p->lineTo(x+diam-xs, y+diam-ys);

				xoff = xs;
				break;

			}
		}
	}
}

/*
 * This function redoes a previously undone move
 */
void KBgBoard::redoMove()
{
	if (getEditMode())
		return;

	int w      = getTurn();
	int mcolor = ((w == US) ? color : -color);
	KBgBoardMove *move = redoHistory.last();
	if (move && (w == US || w == THEM)) {
		/*
		 * Make changes at source
		 */
		if (move->source() == BAR_US || move->source() == BAR_THEM) {
			onbar[w] -= mcolor;
			(getCell(move->source()))->cellUpdate(onbar[w], false);
		} else {
			board[move->source()] -= mcolor;
			(getCell(move->source()))->cellUpdate(board[move->source()]);
		}
		/*
		 * Make changes at the destination
		 */
		if ((move->destination() == HOME_THEM_LEFT ) || (move->destination() == HOME_THEM_RIGHT) ||
		    (move->destination() == HOME_US_LEFT   ) || (move->destination() == HOME_US_RIGHT  )) {
			onhome[w] += mcolor;
			(getCell(move->destination()))->cellUpdate(onhome[w], false);
		} else {
			board[move->destination()] += mcolor;
			if (move->wasKicked()) {
				board[move->destination()] = mcolor;
				onbar[((w == US) ? THEM : US)] -= mcolor;
				(getCell(((w == US) ? BAR_THEM : BAR_US)))->cellUpdate
					(onbar[((w == US) ? THEM : US)], false);
			}
			(getCell(move->destination()))->cellUpdate(board[move->destination()]);
		}
		makeMove(move->source(), move->destination());
		redoHistory.remove();
		emit finishedUpdate();
	}
	sendMove();
}

/*
 * This function performs and undo for the last move and updates the parent
 * of the board by calling sendMove() after the undo.
 */
void KBgBoard::undoMove()
{
	if (getEditMode())
		return;

	int w      = getTurn();
	int mcolor = ((w == US) ? color : -color);
	KBgBoardMove *move = moveHistory.last();
	if (move && (w == US || w == THEM)) {
		/*
		 * Undo changes at source
		 */
		if (move->source() == BAR_US || move->source() == BAR_THEM) {
			onbar[w] += mcolor;
			(getCell(move->source()))->cellUpdate(onbar[w], false);
		} else {
			board[move->source()] += mcolor;
			(getCell(move->source()))->cellUpdate
				(board[move->source()]);
		}
		/*
		 * Undo changes at the destination
		 */
		if ( (move->destination() == HOME_THEM_LEFT ) ||
		     (move->destination() == HOME_THEM_RIGHT) ||
		     (move->destination() == HOME_US_LEFT   ) ||
		     (move->destination() == HOME_US_RIGHT  )) {
			onhome[w] -= mcolor;
			(getCell(move->destination()))->cellUpdate
				(onhome[w], false);
		} else {
			board[move->destination()] -= mcolor;
			if (move->wasKicked()) {
				board[move->destination()] = -mcolor;
				onbar[((w == US) ? THEM : US)] += mcolor;
				(getCell(((w == US) ?
					  BAR_THEM : BAR_US)))->cellUpdate
					(onbar[((w == US) ? THEM : US)], false);
			}
			(getCell(move->destination()))->cellUpdate
				(board[move->destination()]);
		}
		++possMoves[move->length()];
		redoHistory.append(new KBgBoardMove(*move));
		moveHistory.remove();
		emit finishedUpdate();
	}
	sendMove();
}

/*
 * While putting a piece on a cell the cell has noticed that it changed
 * ownership and hence needs a piece to be kicked. Since cells don't
 * know where the opponents bar is we handle this here.
 */
void KBgBoard::kickedPiece()
{
	int w = ((getTurn()) == US ? THEM : US);

	if (w == US) {
		onbar[w] += color;
		(getCell(BAR_US  ))->cellUpdate(onbar[w], false);
	} else {
		onbar[w] -= color;
		(getCell(BAR_THEM))->cellUpdate(onbar[w], false);
	}
	if (!getEditMode()) {
		KBgBoardMove *move = moveHistory.last();
		move->setKicked(true);
	}
	emit finishedUpdate();
}

/*
 * This is a very short utility function for makeShortMove().
 */
void KBgBoardCell::makeShortMoveHelper(int s, int d)
{
	if (getPiece()) {
		board->makeMove(s, d);
		KBgBoardCell *dest = board->getCell(d);
		dest->putPiece(((board->getTurn() == US) ? color : -color));
	}
}

/*
 * This function makes the shortes possible move from this cell. It
 * uses only one dice and and it will kick opponent checkers.
 */
void KBgBoardCell::makeShortMove()
{
	int m[4];

	int dir = ((board->getTurn() == US) ? direction : -direction);
	int src = board->IDtoNum(cellID);

	if (src == BAR_US || src == BAR_THEM) {

		int s = (dir > 0) ? 0 : 25;
		for (int i = 1; i < 7; i++) {
			int d = (dir > 0) ? i : 25 - i;
			if (board->checkMultiMove(s, d, m) == 1) {
				makeShortMoveHelper(src, d);
				break;
			}
		}

	} else {

		for (int i = 1; i < 7; i++) {
			int d = src + dir*i;
			if (d > 25) d = 25;
			if (d <  0) d =  0;
			if (0 < d && d < 25) {

				if (board->checkMultiMove(src, d, m) == 1) {
					makeShortMoveHelper(src, d);
					break;
				}

			} else {

				if (board->moveOffPossible()) {
					int whichHome;
					if (board->getTurn() == US)
						whichHome = ((direction > 0) ?
							     HOME_US_LEFT :
							     HOME_US_RIGHT);
					else
						whichHome = ((direction > 0) ?
							     HOME_THEM_LEFT :
							     HOME_THEM_RIGHT);

					if (board->diceAllowMove
					    (cellID, whichHome)) {
						makeShortMoveHelper(src, whichHome);
						break;
					}
				}
			}
		}
	}
}

/*
 * Ask the current backgammon engine for a doubled cube.
 */
void KBgBoard::getDoubleCube(const int w)
{
	emit doubleCube(w);
}

/*
 * Ask the current backgammon engine rolling the dice.
 */
void KBgBoard::getRollDice(const int w)
{
	emit rollDice(w);
}

/*
 * This is the constructor of the KBgBoard class. It creates
 * a backgammon board with an initial distribution of checkers, empty
 * dice and a cube with face value 1. The initial board is not usable!
 * You have to change the status by passing a KBgStatus
 * object to setState(...) before you can play!
 */
KBgBoard::KBgBoard(QWidget *parent, const char *name, QPopupMenu *menu)
	: QWidget(parent, name)
{
	/*
	 * The following lines set up internal bookkeeping data.
	 */
	moveHistory.setAutoDelete(true);
	redoHistory.setAutoDelete(true);
	cube = 1;
	allowMoving(true);
	setEditMode(false);
	savedCursor = NULL;
	checkerDiam = MINIMUM_CHECKER_SIZE;

	/*
	 * We may be initialized with a popup menu by our parent.
	 */
	contextMenu = menu;

	baseColors[0] = black;
	baseColors[1] = white;

	/*
	 * Get the 30 cells that constitute the board and initialize
	 * them properly.
	 */
	cells[ 0] = new KBgBoardHome(this, HOME_THEM_LEFT);
	cells[14] = new KBgBoardHome(this, HOME_THEM_RIGHT);
	cells[15] = new KBgBoardHome(this, HOME_US_LEFT);
	cells[29] = new KBgBoardHome(this, HOME_US_RIGHT);

	cells[ 7] = new KBgBoardBar(this, BAR_THEM);
	cells[22] = new KBgBoardBar(this, BAR_US);

	for (int i=1; i<7; ++i) {
		cells[   i] = new KBgBoardField(this,    i);
		cells[ 7+i] = new KBgBoardField(this,  6+i);
		cells[15+i] = new KBgBoardField(this, 12+i);
		cells[22+i] = new KBgBoardField(this, 18+i);
	}

	/*
	 * Get the default seeting of the board and initialize the
	 * state of it.
	 */
	KBgStatus *st = new KBgStatus();

	st->setCube(1, true, true);
	st->setDirection(+1);
	st->setColor(+1);

	st->setBoard( 1, US,   2); st->setBoard( 6, THEM, 5);
	st->setBoard( 8, THEM, 3); st->setBoard(12, US,   5);
	st->setBoard(13, THEM, 5); st->setBoard(17, US,   3);
	st->setBoard(19, US,   5); st->setBoard(24, THEM, 2);

	st->setHome(US, 0);

	st->setDice(US  , 0, 0); st->setDice(US  , 1, 0);
	st->setDice(THEM, 0, 0); st->setDice(THEM, 1, 0);

	setState(*st);

	delete st;

	/*
	 * This line simplifies the checkMultiMove(...) function a lot.
	 */
	board[0] = board[25] = 0;

	/*
	 * User interface design settings come here. These may be
	 * overwritten by the user.
	 */
	shortMoveMode = SHORT_MOVE_DOUBLE;
	setBackgroundColor(QColor(200, 200, 166));
	computePipCount = true;

	/*
	 * Set initial font
	 */
	setFont(QApplication::font());
}

QSize KBgBoard::minimumSizeHint() const
{
	return QSize(MINIMUM_CHECKER_SIZE * 15, MINIMUM_CHECKER_SIZE * 11);
}

QSize KBgBoard::sizeHint() const {
	return QSize(MINIMUM_CHECKER_SIZE *15*4,MINIMUM_CHECKER_SIZE*11*2);
}

