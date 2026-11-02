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

  $Id: kbg.cpp,v 1.38 2004/07/13 00:20:01 bmeyer Exp $

*/

#include "kbg.h"
#include "kbg.moc"

#include <qpainter.h>
#include <qlayout.h>
#include <qgroupbox.h>
#include <qpixmap.h>
#include <qstring.h>
#include <qwhatsthis.h>
#include <qstringlist.h>
#include <qvaluelist.h>
#include <qiconset.h>
#include <qvbox.h>

#include <kmenubar.h>
#include <ktoolbar.h>
#include <kapplication.h>
#include <kiconloader.h>
#include <kedittoolbar.h>
#include <klocale.h>
#include <kaction.h>
#include <kstdaction.h>
#include <kstdgameaction.h>
#include <kaboutdata.h>
#include <kmessagebox.h>
#include <kconfig.h>
#include <kcompletion.h>
#include <kcompletionbox.h>
#include <kpopupmenu.h>
#include <kurllabel.h>
#include <krun.h>
#include <kstatusbar.h>
#include <klineedit.h>
#include <knuminput.h>
#include <kprinter.h>

#include "kbgtextview.h"
#include "offline/kbgoffline.h"
#include "fibs/kbgfibs.h"
#include "gnubg/kbggnubg.h"
#include "nextgen/kbgng.h"
#include "version.h"


// == setup ====================================================================

/*
 * Constructor creates user interface, actions and first engine.
 */
KBg::KBg()
{
	/*
	 * Initialize menu strings
	 */
	engineString[Offline] = i18n("Open Board");
	engineString[FIBS   ] = i18n("FIBS");
	engineString[GNUbg  ] = i18n("GNU Backgammon (Experimental)");
	engineString[NextGen] = i18n("Next Generation (Experimental)");

	helpTopic[FIBSHome][0] = i18n("FIBS Home");
	helpTopic[FIBSHome][1] = "http://www.fibs.com/";

	helpTopic[RuleHome][0] = i18n("Backgammon Rules");
	helpTopic[RuleHome][1] = "http://www.bkgm.com/rules.html";

	helpTopic[SelfHome][0] = i18n("KBackgammon");
	helpTopic[SelfHome][1] = PROG_HOME;

	/*
	 * The main view is shared between the board and a small text window
	 */
	panner = new QSplitter(Vertical, this, "panner");
	board  = new KBgBoardSetup(panner, "board");
	status = new KBgTextView(panner, "status");
	setCentralWidget(panner);

	/*
	 * Create all actions needed by the application
	 */
	newAction = KStdGameAction::gameNew(this, SLOT(openNew()), actionCollection());
    newAction->setEnabled(false);
	KStdGameAction::print(this, SLOT(print()), actionCollection());
	KStdGameAction::quit(this, SLOT(close()),  actionCollection());

	QStringList list;
	for (int i = 0; i < MaxEngine; i++)
		list.append(engineString[i]);
	engineSet = new KSelectAction(i18n("&Engine"), 0, this, SLOT(setupEngine()), actionCollection(),
                                  "move_engine");
    engineSet->setItems(list);

    // AB: what the heck has this to do with redisplay? perhaps use reload instead?
	loadAction = KStdAction::redisplay(this, SLOT(load()), actionCollection(), "move_load");
    loadAction->setEnabled(false);
	undoAction = KStdGameAction::undo(this, SLOT(undo()), actionCollection());
    undoAction->setEnabled(false);
	redoAction = KStdGameAction::redo(this, SLOT(redo()), actionCollection());
    redoAction->setEnabled(false);

	rollAction = KStdGameAction::roll(this, SLOT(roll()), actionCollection());
    rollAction->setEnabled(false);
	endAction = KStdGameAction::endTurn(this, SLOT(done()), actionCollection());
    endAction->setEnabled(false);
    cubeAction = new KAction(i18n("Double Cube"), QIconSet(kapp->iconLoader()->loadIcon
                                                          (PROG_NAME "-double.xpm", KIcon::Toolbar)),
                              0, this, SLOT(cube()), actionCollection(), "move_cube");
    cubeAction->setEnabled(false);

	KStdAction::showMenubar(this, SLOT(toggleMenubar()),     actionCollection());
	KStdAction::preferences(this, SLOT(setupDlg()), actionCollection());
	KStdAction::saveOptions(this, SLOT(saveConfig()), actionCollection());

	KPopupMenu *p = (new KActionMenu(i18n("&Backgammon on the Web"),
					 actionCollection(), "help_www"))->popupMenu();

	(new KAction(helpTopic[FIBSHome][0], 0, this, SLOT(wwwFIBS()),
		     actionCollection(), "help_www_fibs"))->plug(p);
	(new KAction(helpTopic[RuleHome][0], 0, this, SLOT(wwwRule()),
		     actionCollection(), "help_www_rule"))->plug(p);
	(new KAction(helpTopic[SelfHome][0], 0, this, SLOT(wwwHome()),
		     actionCollection(), "help_www_home"))->plug(p);

	/*
	 * Set up the command line - using actions, otherwise recreating the GUI will delete them
	 * (e.g. using KEditToolbar)
	 */
	cmdLabel = new QLabel(i18n("Command: "), this);
	new KWidgetAction( cmdLabel, cmdLabel->text(), 0, 0, 0, actionCollection(), "command_label");
	cmdLine  = new KLineEdit(this, "commandline");
	KWidgetAction* actionCmdLine = new KWidgetAction( cmdLine, QString::null, 0, 0, 0, actionCollection(), "command_lineedit");
	actionCmdLine->setAutoSized(true);

	cmdLine->completionObject()->setOrder(KCompletion::Weighted);
	connect(cmdLine, SIGNAL(returnPressed(const QString &)), this, SLOT(handleCmd(const QString &)));
	/*
	 * Done with the actions, create the XML-defined parts of the
	 * user interface
	 */
	statusBar();
	setupGUI();

	cmdLine->setFocus();

	/*
	 * Initialize the engine to the default (offline). If the user
	 * prefers a different engine, it will be started later
	 */
	for (int i = 0; i < MaxEngine; i++)
		engine[i] = 0;
	currEngine = None;
	engineSet->setCurrentItem(Offline);
	setupEngine();

	/*
	 * Set up configuration handling.
	 * FIXME: support session management
	 */
	connect(this, SIGNAL(readSettings()), board, SLOT(readConfig()));
	connect(this, SIGNAL(saveSettings()), board, SLOT(saveConfig()));

	/*
	 * Set up some whatis messages for the online help
	 */
	QWhatsThis::add(status, i18n("This area contains the status messages for the game. "
			     "Most of these messages are sent to you from the current "
			     "engine."));
	QWhatsThis::add(toolBar("cmdToolBar"),
			i18n("This is the command line. You can type special "
			     "commands related to the current engine in here. "
			     "Most relevant commands are also available "
			     "through the menus."));
	QWhatsThis::add(toolBar("mainToolBar"),
			i18n("This is the button bar tool bar. It gives "
			     "you easy access to game related commands. "
			     "You can drag the bar to a different location "
			     "within the window."));
	QWhatsThis::add(statusBar(),
			i18n("This is the status bar. It shows you the currently "
			     "selected engine in the left corner."));

	/*
	 * Create and initialize the context menu
	 */
	QPopupMenu* menu = (QPopupMenu*)factory()->container("popup", this);
	board->setContextMenu(menu);
}

/*
 * Destructor is empty
 */
KBg::~KBg() {}


// == engine handling ==========================================================

/*
 * Set the engine according to the currently selected item in the
 * engineSet action. Additional engines have to be added to the switch
 * statement (and only there).
 */
void KBg::setupEngine()
{
	/*
	 * Get new engine type
	 */
	int type = engineSet->currentItem();

	/*
	 * Engine doesn't need to be changed?
	 */
	if (engine[type]) return;

	/*
	 * Check with the engine if it can be terminated
	 */
	if (currEngine != None && engine[currEngine] && !engine[currEngine]->queryClose()) {
		engineSet->setCurrentItem(currEngine);
		return;
	}

	/*
	 * Remove the old engine, create a new one, and hook up menu and slots/signals
	 */
	QPopupMenu *commandMenu = (QPopupMenu *)factory()->container("command_menu", this);
	QString s = PROG_NAME;
	commandMenu->clear();

	if (currEngine != None) {
		delete engine[currEngine];
		engine[currEngine] = 0;
	}

	switch (currEngine = type) {
	case Offline:
	    engine[currEngine] = new KBgEngineOffline(this, &s, commandMenu);
	    break;
	case FIBS:
	    engine[currEngine] = new KBgEngineFIBS(this, &s, commandMenu);
	    break;
	case GNUbg:
	    engine[currEngine] = new KBgEngineGNU(this, &s, commandMenu);
	    break;
	case NextGen:
	    engine[currEngine] = new KBgEngineNg(this, &s, commandMenu);
	    break;
	default: // FIXME: we need some kind of catch here...
	    currEngine = Offline;
	    engine[currEngine] = new KBgEngineOffline(this, &s, commandMenu);
	    break;
	}

	statusBar()->message(engineString[currEngine]);
	KConfig* config = kapp->config();
	config->setGroup("global settings");
	if (config->readBoolEntry("enable timeout", true))
		engine[currEngine]->setCommit(config->readDoubleNumEntry("timeout", 2.5));
	newAction->setEnabled(engine[currEngine]->haveNewGame());

	// engine -> this
	connect(engine[currEngine], SIGNAL(statText(const QString &)), this, SLOT(updateCaption(const QString &)));
	connect(engine[currEngine], SIGNAL(infoText(const QString &)), status, SLOT(write(const QString &)));
	connect(engine[currEngine], SIGNAL(allowCommand(int, bool)),   this, SLOT(allowCommand(int, bool)));

	// this -> engine
	connect(this, SIGNAL(readSettings()), engine[currEngine], SLOT(readConfig()));
	connect(this, SIGNAL(saveSettings()), engine[currEngine], SLOT(saveConfig()));

	// board -> engine
	connect(board, SIGNAL(rollDice(const int)),    engine[currEngine], SLOT(rollDice(const int)));
	connect(board, SIGNAL(doubleCube(const int)),  engine[currEngine], SLOT(doubleCube(const int)));
	connect(board, SIGNAL(currentMove(QString *)), engine[currEngine], SLOT(handleMove(QString *)));

	// engine -> board
	connect(engine[currEngine], SIGNAL(undoMove()), board, SLOT(undoMove()));
	connect(engine[currEngine], SIGNAL(redoMove()), board, SLOT(redoMove()));
	connect(engine[currEngine], SIGNAL(setEditMode(const bool)), board, SLOT(setEditMode(const bool)));
	connect(engine[currEngine], SIGNAL(allowMoving(const bool)), board, SLOT(allowMoving(const bool)));
	connect(engine[currEngine], SIGNAL(getState(KBgStatus *)), board, SLOT(getState(KBgStatus *)));
	connect(engine[currEngine], SIGNAL(newState(const KBgStatus &)), board, SLOT(setState(const KBgStatus &)));

	// now that all signals are connected, start the engine
	engine[currEngine]->start();
}


// == configuration handing ====================================================

/*
 * Save all settings that should be saved for the next start.
 */
void KBg::saveConfig()
{
	KConfig* config = kapp->config();
	config->setGroup("global settings");

	/*
	 * Save the main window options unless the user has asked not
	 * to do so.
	 */
	if (config->readBoolEntry("autosave on exit", true)) {

		config->setGroup("main window");

		config->writeEntry("origin", pos());

		config->writeEntry("font",   status->font());
		config->writeEntry("panner", (double)board->height()/(double)panner->height());

		saveMainWindowSettings(config, "main window");
	}

	/*
	 * Save the history
	 */
	config->setGroup("command line");
	config->writeEntry("history", cmdLine->completionObject()->items());

	/*
	 * Save current engine
	 */
	config->setGroup("engine settings");
	config->writeEntry("last engine", currEngine);

	/*
	 * Tell other objects to save their settings, too.
	 */
	emit saveSettings();

	config->sync();
}

/*
 * Read the stored configuration and apply it
 */
void KBg::readConfig()
{
	KConfig* config = kapp->config();
	config->setGroup("global settings");

	/*
	 * Restore the main window settings unless the user has asked
	 * not to do so.
	 */
	if (config->readBoolEntry("autosave on exit", true)) {

		config->setGroup("main window");

		QPoint pos, defpos(10, 10);
		QFont kappFont = kapp->font();

		pos = config->readPointEntry("origin", &defpos);

		status->setFont(config->readFontEntry("font", &kappFont));

		QValueList<int> l;
		l.append(qRound(   config->readDoubleNumEntry("panner", 0.75) *panner->height()));
		l.append(qRound((1-config->readDoubleNumEntry("panner", 0.75))*panner->height()));
		panner->setSizes(l);

		applyMainWindowSettings(config, "main window");
	}

	/*
	 * Restore the history
	 */
	config->setGroup("command line");
	cmdLine->completionObject()->setItems(config->readListEntry("history"));

	/*
	 * Tell other objects to read their configurations
	 */
	emit readSettings();

	/*
	 * Restore last engine
	 */
	config->setGroup("engine settings");
	engineSet->setCurrentItem((Engines)config->readNumEntry("last engine", Offline));
	setupEngine();
}


// == configuration ============================================================

/*
 * Connected to the setup dialog applyButtonPressed signal. Make sure
 * that all changes are saved.
 */
void KBg::setupOk()
{
	// global settings
	KConfig* config = kapp->config();
	config->setGroup("global settings");

	config->writeEntry("enable timeout",   cbt->isChecked());
	config->writeEntry("timeout",          sbt->value());
	config->writeEntry("autosave on exit", cbs->isChecked());

	// tell engine about commit timer
	engine[currEngine]->setCommit(cbt->isChecked() ? sbt->value() : -1);

	// one time requests
	if (cbm->isChecked())
		KMessageBox::enableAllMessages();

	// tell children to read their changes
	board->setupOk();

	// engines
	for (int i = 0; i < MaxEngine; i++)
		engine[i]->setupOk();

	// save it all
	saveConfig();
}

/*
 * Load default values for the user settings
 */
void KBg::setupDefault()
{
	// timeout
	cbt->setChecked(true);
	sbt->setValue(2.5);

	// messages
	cbm->setChecked(false);

	// auto save
	cbs->setChecked(true);

	// board
	board->setupDefault();

	// engines
	for (int i = 0; i < MaxEngine; i++)
		engine[i]->setupDefault();
}

/*
 * Connected to the setup dialog cancelButtonPressed signal. There
 * isn't much to do. We tell the board to undo the changes.
 */
void KBg::setupCancel()
{
	// board
	board->setupCancel();

	// engines
	for (int i = 0; i < MaxEngine; i++)
		engine[i]->setupCancel();
}

/*
 * Setup dialog is ready to be deleted. Do it later...
 */
void KBg::setupDone()
{
	nb->delayedDestruct();
	for (int i = 0; i < MaxEngine; i++)
		if (i != currEngine) engine[i] = 0;
}

// FIXME make more general...

void KBg::startKCM(const QString &url)
{
	KRun::runCommand(url);
}

/*
 * Initialize and display the setup dialog
 */
void KBg::setupDlg()
{
	/*
	 * Get a new notebook in which all other members can put their
	 * config pages
	 */
	nb = new KDialogBase(KDialogBase::IconList, i18n("Configuration"),
			     KDialogBase::Ok|KDialogBase::Cancel|KDialogBase::Default|
			     KDialogBase::Apply|KDialogBase::Help,
			     KDialogBase::Ok, this, "setup", true, true);

	KConfig* config = kapp->config();
	config->setGroup("global settings");

	/*
	 * Main Widget
	 */
	QVBox *w = nb->addVBoxPage(i18n("General"), i18n("Here you can configure general settings of %1").
				   arg(kapp->aboutData()->programName()),
				   kapp->iconLoader()->loadIcon("go", KIcon::Desktop));

	/*
	 * Group boxes
	 */
	QGroupBox *gbm = new QGroupBox(i18n("Messages"), w);
	QGroupBox *gbt = new QGroupBox(i18n("Timer"), w);
	QGroupBox *gbs = new QGroupBox(i18n("Autosave"), w);
	QGroupBox *gbe = new QGroupBox(i18n("Events"), w);

	/*
	 * Timer box
	 */
	QWhatsThis::add(gbt, i18n("After you finished your moves, they have to be sent to the engine. "
				  "You can either do that manually (in which case you should not enable "
				  "this feature), or you can specify an amount of time that has to pass "
				  "before the move is committed. If you undo a move during the timeout, the "
				  "timeout will be reset and restarted once you finish the move. This is "
				  "very useful if you would like to review the result of your move."));

	cbt = new QCheckBox(i18n("Enable timeout"), gbt);
	cbt->setChecked(config->readBoolEntry("enable timeout", true));

	sbt = new KDoubleNumInput(gbt);
	sbt->setRange(0.0, 60.0, 0.5);
	sbt->setLabel(i18n("Move timeout in seconds:"));
	sbt->setValue(config->readDoubleNumEntry("timeout", 2.5));

	connect(cbt, SIGNAL(toggled(bool)), sbt, SLOT(setEnabled(bool)));
	sbt->setEnabled(cbt->isChecked());

	QGridLayout *gl = new QGridLayout(gbt, 2, 1, 20);
	gl->addWidget(cbt, 0, 0);
	gl->addWidget(sbt, 1, 0);

	/*
	 * Enable messages
	 */
	QWhatsThis::add(gbm, i18n("Check the box to enable all the messages that you have previously "
				  "disabled by choosing the \"Don't show this message again\" option."));

	QGridLayout *glm = new QGridLayout(gbm, 1, 1, nb->spacingHint());
	cbm = new QCheckBox(i18n("Reenable all messages"), gbm);
	glm->addWidget(cbm, 0, 0);

	/*
	 * Save options on exit ?
	 */
	QWhatsThis::add(gbm, i18n("Check the box to automatically save all window positions on program "
				  "exit. They will be restored at next start."));

	QGridLayout *gls = new QGridLayout(gbs, 1, 1, nb->spacingHint());
	cbs = new QCheckBox(i18n("Save settings on exit"), gbs);
	cbs->setChecked(config->readBoolEntry("autosave on exit", true));
	gls->addWidget(cbs, 0, 0);

	/*
	 * Event vonfiguration
	 */
	QWhatsThis::add(gbe, i18n("Event notification of %1 is configured as part of the "
				  "system-wide notification process. Click here, and you "
				  "will be able to configure system sounds, etc.").
			arg(kapp->aboutData()->programName()));

	QGridLayout *gle = new QGridLayout(gbe, 1, 1, nb->spacingHint());
	KURLLabel *lab = new KURLLabel("kcmshell kcmnotify",
				       i18n("Klick here to configure the event notification"), gbe);
	lab->setMaximumSize(lab->sizeHint());

	gle->addWidget(lab, 0, 0);
	connect(lab, SIGNAL(leftClickedURL(const QString &)), SLOT(startKCM(const QString &)));

	/*
	 * Board settings
	 */
	board->getSetupPages(nb);

	/*
	 * Hack alert: this little trick makes sure that ALL engines
	 * have their settings available in the dialog.
	 */
	QPopupMenu *dummyPopup = new QPopupMenu(nb);
	QString s = PROG_NAME;
	for (int i = 0; i < MaxEngine; i++) {
		if (currEngine != i) {
			switch (i) {
			case Offline:
		 		engine[i] = new KBgEngineOffline(nb, &s, dummyPopup);
				break;
			case FIBS:
				engine[i] = new KBgEngineFIBS(nb, &s, dummyPopup);
				break;
			case GNUbg:
				engine[i] = new KBgEngineGNU(nb, &s, dummyPopup);
				break;
			case NextGen:
				engine[i] = new KBgEngineNg(nb, &s, dummyPopup);
				break;
			}
			connect(this, SIGNAL(saveSettings()), engine[i], SLOT(saveConfig()));
		}
		engine[i]->getSetupPages(nb);
	}

	/*
	 * Connect the signals of nb
	 */
	connect(nb, SIGNAL(okClicked()),     this, SLOT(setupOk()));
	connect(nb, SIGNAL(applyClicked()),  this, SLOT(setupOk()));
	connect(nb, SIGNAL(cancelClicked()), this, SLOT(setupCancel()));
	connect(nb, SIGNAL(defaultClicked()),this, SLOT(setupDefault()));

	connect(nb, SIGNAL(finished()), this, SLOT(setupDone()));

	nb->resize(nb->minimumSize());
	nb->show();
}


// == action slots =============================================================

/*
 * Tell the board to print itself - restore and save user settings for
 * the print dialog.
 */
void KBg::print()
{
	KPrinter *prt = new KPrinter();

	KConfig* config = kapp->config();
	config->setGroup("printing");

	prt->setNumCopies(config->readNumEntry("numcopies", 1));
	prt->setOutputFileName(config->readPathEntry("outputfile"));
	prt->setOutputToFile(config->readBoolEntry("tofile", false));
	prt->setPageSize((KPrinter::PageSize) config->readNumEntry("pagesize", KPrinter::A4));
	prt->setOrientation((KPrinter::Orientation)config->readNumEntry("orientation", KPrinter::Landscape));

	if (prt->setup(this, i18n("Print %1").arg(baseCaption))) {
		QPainter p;
		p.begin(prt);
		board->print(&p);
		p.end();
		config->writeEntry("tofile",      prt->outputToFile());
		config->writePathEntry("outputfile",  prt->outputFileName());
		config->writeEntry("pagesize",    (int)prt->pageSize());
		config->writeEntry("orientation", (int)prt->orientation());
		config->writeEntry("numcopies",   prt->numCopies());
	}
	delete prt;
}

/*
 * Toggle visibility of the menubar - be careful that the menu doesn't
 * get lost
 */
void KBg::toggleMenubar()
{
	if (menuBar()->isVisible()) {

		KMessageBox::information(this, i18n("You can enable the menubar again with the "
						    "right mouse button menu of the board."),
					 i18n("Information"), "conf_menubar_information");
		menuBar()->hide();

	} else {

		menuBar()->show();
	}
}

/*
 * Display a standard dialog for the toolbar content
 */
void KBg::configureToolbars()
{
	saveMainWindowSettings(KGlobal::config(), "kedittoolbar settings"); // temp group
	KEditToolbar dlg(actionCollection(), xmlFile(), true, this);
	connect(&dlg,SIGNAL(newToolbarConfig()),this,SLOT(newToolbarConfig()));
	dlg.exec();
	KGlobal::config()->deleteGroup( "kedittoolbar settings" ); // delete temp group
}

/*
 * Called when clicking OK or Apply in the toolbar editor
 */
void KBg::newToolbarConfig()
{
	createGUI();
	applyMainWindowSettings(KGlobal::config(), "kedittoolbar settings");
}

/*
 * Help slots
 */
void KBg::wwwFIBS() {showWWW(FIBSHome);}
void KBg::wwwHome() {showWWW(SelfHome);}
void KBg::wwwRule() {showWWW(RuleHome);}

void KBg::showWWW(int t)
{
	kapp->invokeBrowser(helpTopic[t][1]);
}

/*
 * Edit slots
 */
void KBg::undo() {engine[currEngine]->undo();}
void KBg::redo() {engine[currEngine]->redo();}
void KBg::roll() {engine[currEngine]->roll();}
void KBg::cube() {engine[currEngine]->cube();}
void KBg::done() {engine[currEngine]->done();}
void KBg::load() {engine[currEngine]->load();}

/*
 * Start a new game with the current engine
 */
void KBg::openNew()
{
	engine[currEngine]->newGame();
}


// == various slots - not for actions ==========================================

/*
 * Check with the engine if it is okay to close the window.
 * If so, save settings.
 */
bool KBg::queryClose()
{
	bool ret = engine[currEngine]->queryClose();
	if ( ret )
	    saveConfig();
	return ret;
}

/*
 * Set the caption of the main window. If the user has requested pip
 * counts, they are appended, too.
 */
void KBg::updateCaption(const QString &s)
{
	baseCaption = s;
	QString msg;
	if (!s.isEmpty()) {
		msg = s;
		if (board->getPipCount(US) >= 0) {
			QString tmp;
			tmp.setNum(board->getPipCount(US  ));
			msg += " - " + tmp;
			tmp.setNum(board->getPipCount(THEM));
			msg += "-"  + tmp;
		}
	}
	setCaption(msg, false);
}

/*
 * Take the string from the commandline, give it to the engine, append
 * to the history and clear the buffer.
 */
void KBg::handleCmd(const QString &s)
{
	if (!s.stripWhiteSpace().isEmpty()) {
		engine[currEngine]->handleCommand(s);
		cmdLine->completionObject()->addItem(s);
	}
	cmdLine->clear();
	cmdLine->completionBox()->close();
}

/*
 * Reflect the availability of commands in the button bar.
 */
void KBg::allowCommand(int cmd, bool f)
{
	switch (cmd) {
	case KBgEngine::Undo:
        undoAction->setEnabled(f);
		break;
	case KBgEngine::Redo:
		redoAction->setEnabled(f);
		break;
	case KBgEngine::Roll:
		rollAction->setEnabled(f);
		break;
	case KBgEngine::Cube:
		cubeAction->setEnabled(f);
		break;
	case KBgEngine::Done:
		endAction->setEnabled(f);
		break;
	case KBgEngine::Load:
		loadAction->setEnabled(f);
		break;
	}
}

/*
 * Catch the hide envents. That way, the current engine can close its
 * child windows.
 */
void KBg::hideEvent(QHideEvent *e)
{
	KMainWindow::hideEvent(e);
	engine[currEngine]->hideEvent();
}

/*
 * Catch the show envents. That way, the current engine can open any
 * previously hidden windows.
 */
void KBg::showEvent(QShowEvent *e)
{
	KMainWindow::showEvent(e);
	engine[currEngine]->showEvent();
}

// EOF

