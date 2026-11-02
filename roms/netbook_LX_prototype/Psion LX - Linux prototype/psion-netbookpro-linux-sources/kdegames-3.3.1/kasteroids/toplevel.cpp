/*
 * KAsteroids - Copyright (c) Martin R. Jones 1997
 *
 * Part of the KDE project
 */

#include <qlabel.h>
#include <qlayout.h>
#include <qlcdnumber.h>
#include <qspinbox.h>
#include <qcheckbox.h>

#include <kdebug.h>
#include <kapplication.h>
#include <kstandarddirs.h>
#include <kglobalsettings.h>
#include <kmessagebox.h>
#include <klocale.h>
#include <kkeydialog.h>
#include <kaction.h>
#include <kstdgameaction.h>
#include <kaudioplayer.h>
#include <kscoredialog.h>
#include <kconfigdialog.h>

#include "settings.h"
#include "toplevel.h"
#include "ledmeter.h"

#include "toplevel.moc"

#if defined Q_WS_X11 && ! defined K_WS_QTONLY
#include <X11/Xlib.h>
#endif
#include <fixx11h.h>

#define SB_SCORE	1
#define SB_LEVEL	2
#define SB_SHIPS	3

struct SLevel
{
    int    nrocks;
    double rockSpeed;
};

#define MAX_LEVELS	16

SLevel levels[MAX_LEVELS] =
{
    { 1, 0.4 },
    { 1, 0.6 },
    { 2, 0.5 },
    { 2, 0.7 },
    { 2, 0.8 },
    { 3, 0.6 },
    { 3, 0.7 },
    { 3, 0.8 },
    { 4, 0.6 },
    { 4, 0.7 },
    { 4, 0.8 },
    { 5, 0.7 },
    { 5, 0.8 },
    { 5, 0.9 },
    { 5, 1.0 }
};

#ifdef KA_ENABLE_SOUND
#include <arts/dispatcher.h>
#include <arts/soundserver.h>

Arts::SimpleSoundServer *soundServer = 0;
#endif


KAstTopLevel::KAstTopLevel()
{
    QWidget *mainWin = new QWidget( this );
    mainWin->setFixedSize(640, 480);

    view = new KAsteroidsView( mainWin );
    connect( view, SIGNAL( shipKilled() ), SLOT( slotShipKilled() ) );
    connect( view, SIGNAL( rockHit(int) ), SLOT( slotRockHit(int) ) );
    connect( view, SIGNAL( rocksRemoved() ), SLOT( slotRocksRemoved() ) );
    connect( view, SIGNAL( updateVitals() ), SLOT( slotUpdateVitals() ) );

    QVBoxLayout *vb = new QVBoxLayout( mainWin );
    QHBoxLayout *hb = new QHBoxLayout;
    QHBoxLayout *hbd = new QHBoxLayout;
    vb->addLayout( hb );

    QFont labelFont( KGlobalSettings::generalFont().family(), 24 );
    QColorGroup grp( darkGreen, black, QColor( 128, 128, 128 ),
	    QColor( 64, 64, 64 ), black, darkGreen, black );
    QPalette pal( grp, grp, grp );

    mainWin->setPalette( pal );

    hb->addSpacing( 10 );

    QLabel *label;
    label = new QLabel( i18n("Score"), mainWin );
    label->setFont( labelFont );
    label->setPalette( pal );
    label->setFixedWidth( label->sizeHint().width() );
    hb->addWidget( label );

    scoreLCD = new QLCDNumber( 6, mainWin );
    scoreLCD->setFrameStyle( QFrame::NoFrame );
    scoreLCD->setSegmentStyle( QLCDNumber::Flat );
    scoreLCD->setFixedWidth( 150 );
    scoreLCD->setPalette( pal );
    hb->addWidget( scoreLCD );
    hb->addStretch( 10 );

    label = new QLabel( i18n("Level"), mainWin );
    label->setFont( labelFont );
    label->setPalette( pal );
    label->setFixedWidth( label->sizeHint().width() );
    hb->addWidget( label );

    levelLCD = new QLCDNumber( 2, mainWin );
    levelLCD->setFrameStyle( QFrame::NoFrame );
    levelLCD->setSegmentStyle( QLCDNumber::Flat );
    levelLCD->setFixedWidth( 70 );
    levelLCD->setPalette( pal );
    hb->addWidget( levelLCD );
    hb->addStretch( 10 );

    label = new QLabel( i18n("Ships"), mainWin );
    label->setFont( labelFont );
    label->setFixedWidth( label->sizeHint().width() );
    label->setPalette( pal );
    hb->addWidget( label );

    shipsLCD = new QLCDNumber( 1, mainWin );
    shipsLCD->setFrameStyle( QFrame::NoFrame );
    shipsLCD->setSegmentStyle( QLCDNumber::Flat );
    shipsLCD->setFixedWidth( 40 );
    shipsLCD->setPalette( pal );
    hb->addWidget( shipsLCD );

    hb->addStrut( 30 );

    QFrame *sep = new QFrame( mainWin );
    sep->setMaximumHeight( 5 );
    sep->setFrameStyle( QFrame::HLine | QFrame::Raised );
    sep->setPalette( pal );

    vb->addWidget( sep );

    vb->addWidget( view, 10 );

// -- bottom layout:
    QFrame *sep2 = new QFrame( mainWin );
    sep2->setMaximumHeight( 1 );
    sep2->setFrameStyle( QFrame::HLine | QFrame::Raised );
    sep2->setPalette( pal );
    vb->addWidget( sep2, 1 );

    vb->addLayout( hbd );

    QFont smallFont( KGlobalSettings::generalFont().family(), 14 );
    hbd->addSpacing( 10 );

    QString sprites_prefix =
        KGlobal::dirs()->findResourceDir("sprite", "rock1/rock10000.png");
/*
    label = new QLabel( i18n( "T" ), mainWin );
    label->setFont( smallFont );
    label->setFixedWidth( label->sizeHint().width() );
    label->setPalette( pal );
    hbd->addWidget( label );

    teleportsLCD = new QLCDNumber( 1, mainWin );
    teleportsLCD->setFrameStyle( QFrame::NoFrame );
    teleportsLCD->setSegmentStyle( QLCDNumber::Flat );
    teleportsLCD->setPalette( pal );
    teleportsLCD->setFixedHeight( 20 );
    hbd->addWidget( teleportsLCD );

    hbd->addSpacing( 10 );
*/
    QPixmap pm( sprites_prefix + "powerups/brake.png" );
    label = new QLabel( mainWin );
    label->setPixmap( pm );
    label->setFixedWidth( label->sizeHint().width() );
    label->setPalette( pal );
    hbd->addWidget( label );

    brakesLCD = new QLCDNumber( 1, mainWin );
    brakesLCD->setFrameStyle( QFrame::NoFrame );
    brakesLCD->setSegmentStyle( QLCDNumber::Flat );
    brakesLCD->setPalette( pal );
    brakesLCD->setFixedHeight( 20 );
    hbd->addWidget( brakesLCD );

    hbd->addSpacing( 10 );

    pm.load( sprites_prefix + "powerups/shield.png" );
    label = new QLabel( mainWin );
    label->setPixmap( pm );
    label->setFixedWidth( label->sizeHint().width() );
    label->setPalette( pal );
    hbd->addWidget( label );

    shieldLCD = new QLCDNumber( 1, mainWin );
    shieldLCD->setFrameStyle( QFrame::NoFrame );
    shieldLCD->setSegmentStyle( QLCDNumber::Flat );
    shieldLCD->setPalette( pal );
    shieldLCD->setFixedHeight( 20 );
    hbd->addWidget( shieldLCD );

    hbd->addSpacing( 10 );

    pm.load( sprites_prefix + "powerups/shoot.png" );
    label = new QLabel( mainWin );
    label->setPixmap( pm );
    label->setFixedWidth( label->sizeHint().width() );
    label->setPalette( pal );
    hbd->addWidget( label );

    shootLCD = new QLCDNumber( 1, mainWin );
    shootLCD->setFrameStyle( QFrame::NoFrame );
    shootLCD->setSegmentStyle( QLCDNumber::Flat );
    shootLCD->setPalette( pal );
    shootLCD->setFixedHeight( 20 );
    hbd->addWidget( shootLCD );

    hbd->addStretch( 1 );

    label = new QLabel( i18n( "Fuel" ), mainWin );
    label->setFont( smallFont );
    label->setFixedWidth( label->sizeHint().width() + 10 );
    label->setPalette( pal );
    hbd->addWidget( label );

    powerMeter = new KALedMeter( mainWin );
    powerMeter->setFrameStyle( QFrame::Box | QFrame::Plain );
    powerMeter->setRange( MAX_POWER_LEVEL );
    powerMeter->addColorRange( 10, darkRed );
    powerMeter->addColorRange( 20, QColor(160, 96, 0) );
    powerMeter->addColorRange( 70, darkGreen );
    powerMeter->setCount( 40 );
    powerMeter->setPalette( pal );
    powerMeter->setFixedSize( 200, 12 );
    hbd->addWidget( powerMeter );

    initKAction();

    setCentralWidget( mainWin );
		
    setupGUI( KMainWindow::Save | Create );
    
		setFocusPolicy( StrongFocus );
    setFocus();

#ifdef KA_ENABLE_SOUND
    soundServer = new Arts::SimpleSoundServer(
                         Arts::Reference("global:Arts_SimpleSoundServer") );
#endif

    loadSettings();
    slotNewGame();
    installEventFilter(this);
}

KAstTopLevel::~KAstTopLevel()
{
#if defined Q_WS_X11 && ! defined K_WS_QTONLY
    XAutoRepeatOn( qt_xdisplay() );
#endif
    soundDict.setAutoDelete(true);
    soundDict.clear();
#ifdef KA_ENABLE_SOUND
    delete soundServer;
#endif
}

void KAstTopLevel::initKAction()
{
// game
    KStdGameAction::gameNew( this, SLOT( slotNewGame() ), actionCollection() );
    KStdGameAction::highscores( this, SLOT( slotShowHighscores() ), actionCollection() );
    KStdGameAction::pause( this, SLOT( slotPause() ), actionCollection() );
    KStdGameAction::quit(this, SLOT( close() ), actionCollection());

// settings
    KStdAction::keyBindings(this, SLOT( slotKeyConfig() ), actionCollection());
    KStdAction::preferences(this, SLOT( slotPref() ), actionCollection());

// keyboard-only actions
    keycodes.insert(Thrust, new KAction(i18n("Thrust"), Qt::Key_Up, 0, 0, actionCollection(), "Thrust"));
    keycodes.insert(RotateLeft, new KAction(i18n("Rotate Left"), Qt::Key_Left, 0, 0, actionCollection(), "RotateLeft"));
    keycodes.insert(RotateRight, new KAction(i18n("Rotate Right"), Qt::Key_Right, 0, 0, actionCollection(), "RotateRight"));
    keycodes.insert(Shoot, new KAction(i18n("Shoot"), Qt::Key_Space, 0, 0, actionCollection(), "Shoot"));
//     keycodes.insert(Teleport, new KAction(i18n("Teleport"), Qt::Key_Z, 0, 0, actionCollection(), "Teleport"));
    keycodes.insert(Brake, new KAction(i18n("Brake"), Qt::Key_X, 0, 0, actionCollection(), "Brake"));
    keycodes.insert(Shield, new KAction(i18n("Shield"), Qt::Key_S, 0, 0, actionCollection(), "Shield"));
    launchAction = new KAction(i18n("Launch"), Qt::Key_L, this, SLOT(slotLaunch()), actionCollection(), "Launch");
}


void KAstTopLevel::loadSettings()
{
    soundDict.insert("ShipDestroyed", 
                 new QString( locate("sounds", Settings::soundShipDestroyed())) );
    soundDict.insert("RockDestroyed", 
                 new QString( locate("sounds", Settings::soundRockDestroyed())) );

    shipsRemain = Settings::numShips();
}

void KAstTopLevel::playSound( const char *snd )
{
// KAudioPlayer sometimes seem to be a little bit...slow
// it takes a moment until the sound is played - maybe an arts problem
// but it's a good temporary solution
    if (!Settings::playSounds())
        return;

    QString *filename = soundDict[ snd ];
    if (filename) {
        KAudioPlayer::play(*filename);
    }

return; // remove this and the above when the sound below is working correctly
#ifdef KA_ENABLE_SOUND
    QString *filename = soundDict[ snd ];
    if (filename) {
        kdDebug(12012)<< "playing " << *filename << endl;
        if(!soundServer->isNull()) soundServer->play(filename->latin1());
    }
#endif
}

bool KAstTopLevel::eventFilter( QObject* /* object */, QEvent *event )
{
	QKeyEvent *e = dynamic_cast<QKeyEvent*>(event);
	if (event->type() == QEvent::AccelOverride)
	{
		if (processKeyPress(e)) return true;
		else return false;
	}
	else if (event->type() == QEvent::KeyRelease)
	{
		if (processKeyRelease(e)) return true;
		else return false;
	}
	return false;
}

bool KAstTopLevel::processKeyPress( QKeyEvent *event )
{
    KKey key(event);
    Action a = Invalid;
    QMap<Action, KAction*>::Iterator it = keycodes.begin();
    for (; it != keycodes.end(); ++it)
    {
       if ( (*it)->shortcut().contains(key) )
       {
           a = it.key();
           break;
       }
    }
    switch ( a )
    {
        case RotateLeft:
            view->rotateLeft( true );
            break;

        case RotateRight:
            view->rotateRight( true );
            break;

        case Thrust:
            view->thrust( true );
            break;

        case Shoot:
            if (gameOver)
               slotNewGame();
            else
               view->shoot( true );
            break;

        case Shield:
            view->setShield( true );
            break;

/*        case Teleport:
            view->teleport( true );
            break;*/

        case Brake:
            view->brake( true );
            break;

        case Invalid:
        default:
            return false;
    }
    return true;
}

bool KAstTopLevel::processKeyRelease( QKeyEvent *event )
{
    KKey key(event);
    Action a = Invalid;
    QMap<Action, KAction*>::Iterator it = keycodes.begin();
    for (; it != keycodes.end(); ++it)
    {
       if ( (*it)->shortcut().contains(key) )
       {
           a = it.key();
           break;
       }
    }
    switch ( a )
    {
        case RotateLeft:
            view->rotateLeft( false );
            break;

        case RotateRight:
            view->rotateRight( false );
            break;

        case Thrust:
            view->thrust( false );
            break;

        case Shoot:
            view->shoot( false );
            break;

        case Brake:
            view->brake( false );
            break;

        case Shield:
            view->setShield( false );
            break;

/*        case Teleport:
            view->teleport( false );
            break;*/

        case Invalid:
        default:
            return false;
    }

    return true;
}

void KAstTopLevel::focusInEvent( QFocusEvent *e )
{
    view->pause( false );
#if defined Q_WS_X11 && ! defined K_WS_QTONLY
    XAutoRepeatOff( qt_xdisplay() );
#endif
    KMainWindow::focusInEvent(e);
}

void KAstTopLevel::focusOutEvent( QFocusEvent *e )
{
    view->pause( true );
#if defined Q_WS_X11 && ! defined K_WS_QTONLY
    XAutoRepeatOn( qt_xdisplay() );
#endif
    KMainWindow::focusOutEvent(e);
}

void KAstTopLevel::slotNewGame()
{
    loadSettings();
    score = 0;
    scoreLCD->display( 0 );
    level = 0;
    levelLCD->display( level );
    shipsLCD->display( shipsRemain-1 );
    view->newGame();
    view->setRockSpeed( levels[0].rockSpeed );
    view->addRocks( levels[0].nrocks );
    view->showText( i18n( "Press %1 to launch." )
                    .arg(launchAction->shortcut().seq(0).toString()),
                    yellow );
    waitShip = true;
    gameOver = false;
}

bool KAstTopLevel::queryExit()
{
#if defined Q_WS_X11 && ! defined K_WS_QTONLY
    XAutoRepeatOn( qt_xdisplay() );
#endif
    return true;
}

void KAstTopLevel::slotShipKilled()
{
    shipsRemain--;
    shipsLCD->display( shipsRemain-1 );

    playSound( "ShipDestroyed" );

    if ( shipsRemain )
    {
        waitShip = true;
        view->showText( i18n( "Ship Destroyed. Press %1 to launch.")
                        .arg(launchAction->shortcut().seq(0).toString()),
                        yellow );
    }
    else
    {
        view->showText( i18n("Game Over!"), red );
        view->endGame();
	doStats();
	KScoreDialog d(KScoreDialog::Name | KScoreDialog::Level | KScoreDialog::Score, this);
        KScoreDialog::FieldInfo scoreInfo;
        scoreInfo[KScoreDialog::Level].setNum(level);

        if (d.addScore(score, scoreInfo) || Settings::showHiscores())
        {
           // Show highscore and ask for name.
           d.exec();
           slotGameOver();
        }
        else
        {
           QTimer::singleShot(1000, this, SLOT(slotGameOver()));
        }
    }
}

void KAstTopLevel::slotGameOver()
{
   gameOver = true;
}

void KAstTopLevel::slotRockHit( int size )
{
    switch ( size )
    {
	case 0:
	    score += 10;
	     break;

	case 1:
	    score += 20;
	    break;

	default:
	    score += 40;
      }

    playSound( "RockDestroyed" );

    scoreLCD->display( score );
}

void KAstTopLevel::slotRocksRemoved()
{
    level++;

    if ( level >= MAX_LEVELS )
	level = MAX_LEVELS - 1;

    view->setRockSpeed( levels[level-1].rockSpeed );
    view->addRocks( levels[level-1].nrocks );

    levelLCD->display( level );
}

void KAstTopLevel::slotKeyConfig()
{
    KKeyDialog::configure( actionCollection(), this );
    if ( waitShip ) view->showText( i18n( "Press %1 to launch." )
                    .arg(launchAction->shortcut().seq(0).toString()),
                    yellow, false );
}

void KAstTopLevel::slotPref()
{
  if(KConfigDialog::showDialog("settings"))
    return;

  KConfigDialog *dialog = new KConfigDialog(this, "settings", Settings::self(), KDialogBase::Swallow);

  /* Make widget */
  QWidget *w = new QWidget(0, "Settings");
  QVBoxLayout *page = new QVBoxLayout( w, 11 );

  QHBoxLayout *hb = new QHBoxLayout( page, 11 );
  QLabel *label = new QLabel( i18n("Start new game with"), w );
  QSpinBox* sb = new QSpinBox( 1, 5, 1, w, "kcfg_numShips" );
  sb->setValue(3);
  QLabel *lb = new QLabel( i18n(" ships."), w );
  QSpacerItem* hspacer = new QSpacerItem( 16, 20, QSizePolicy::Minimum, QSizePolicy::Expanding );
  hb->addWidget(label);
  hb->addWidget(sb);
  hb->addWidget(lb);
  hb->addItem(hspacer);

  QCheckBox *f1 = new QCheckBox(i18n("Show highscores on Game Over"), w, "kcfg_showHiscores");
  QCheckBox *f2 = new QCheckBox(i18n("Player can destroy Powerups"), w, "kcfg_canDestroyPowerups");
  f2->setChecked(true);
  page->addWidget(f1);
  page->addWidget(f2);
  QSpacerItem* spacer = new QSpacerItem( 20, 16, QSizePolicy::Minimum, QSizePolicy::Expanding );
  page->addItem( spacer );
  /* Done */

  dialog->addPage(w, i18n("General"), "package_settings");
  connect(dialog, SIGNAL(settingsChanged()), this, SLOT(loadSettings()));
  dialog->show();
}

void KAstTopLevel::slotShowHighscores()
{
    KScoreDialog d(KScoreDialog::Name | KScoreDialog::Level | KScoreDialog::Score, this);
    d.exec();
}

void KAstTopLevel::doStats()
{
    QString r;
    if ( view->shots() )
	 r = KGlobal::locale()->formatNumber(( (float)view->hits() /
					(float)view->shots() ) * 100, 2 );
    else
	r = KGlobal::locale()->formatNumber( 0.0, 2 );

    QString s = i18n( "Game Over\n\n"
		      "Shots fired:\t%1\n"
		      "  Hit:\t%2\n"
		      "  Missed:\t%3\n"
		      "Hit ratio:\t%4 %\t\t")
      .arg(view->shots()).arg(view->hits())
      .arg(view->shots() - view->hits())
      .arg(r);

    view->showText( s, green, FALSE );
}

void KAstTopLevel::slotUpdateVitals()
{
    brakesLCD->display( view->brakeCount() );
    shieldLCD->display( view->shieldCount() );
    shootLCD->display( view->shootCount() );
//    teleportsLCD->display( view->teleportCount() );
    powerMeter->setValue( view->power() );
}

void KAstTopLevel::slotPause()
{
    KMessageBox::information( this,
                              i18n("KAsteroids is paused."),
                              i18n("Paused") );
    //Hack (montel@kde.org)
    //To create "pause" action we display a messagebox
    //and we "play" when we close this messagebox
    //so before this action was 'checked/uncheched'
    //so I force to unchecked it when we close messagebox.
    KToggleAction * act = (KToggleAction *)(sender());
    act->setChecked(false);
}

void KAstTopLevel::slotLaunch()
{
    if ( waitShip )
    {
        view->newShip();
        waitShip = false;
        view->hideText();
    }
}
