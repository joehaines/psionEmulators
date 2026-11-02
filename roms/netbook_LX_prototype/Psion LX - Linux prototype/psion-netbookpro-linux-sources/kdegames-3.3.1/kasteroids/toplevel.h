/*
 * KAsteroids - Copyright (c) Martin R. Jones 1997
 *
 * Part of the KDE project
 */

#ifndef __KAST_TOPLEVEL_H__
#define __KAST_TOPLEVEL_H__

#include <kmainwindow.h>
#include <kaccel.h>
#include <qdict.h>
#include <qmap.h>

#include "view.h"

//#define KA_ENABLE_SOUND

class KALedMeter;
class QLCDNumber;
class KDialogBase;

class KAstTopLevel : public KMainWindow
{
    Q_OBJECT
public:
    KAstTopLevel();
    virtual ~KAstTopLevel();

private:
    void initKAction();
    void playSound( const char *snd );
    void readSoundMapping();
    void doStats();
    bool queryExit();
    bool processKeyPress( QKeyEvent *event );
    bool processKeyRelease( QKeyEvent *event );

protected:
    virtual bool eventFilter( QObject *object, QEvent *event );
    virtual void focusInEvent( QFocusEvent *event );
    virtual void focusOutEvent( QFocusEvent *event );

private slots:
    void loadSettings();
    void slotNewGame();
    void slotGameOver();

    void slotShipKilled();
    void slotRockHit( int size );
    void slotRocksRemoved();

    void slotUpdateVitals();

    void slotKeyConfig();
    void slotPref();
    void slotShowHighscores();

    void slotPause();
    void slotLaunch();

private:
    KAsteroidsView *view;
    QLCDNumber *scoreLCD;
    QLCDNumber *levelLCD;
    QLCDNumber *shipsLCD;

    QLCDNumber *teleportsLCD;
//    QLCDNumber *bombsLCD;
    QLCDNumber *brakesLCD;
    QLCDNumber *shieldLCD;
    QLCDNumber *shootLCD;
    KALedMeter *powerMeter;

    QDict<QString> soundDict;

    // waiting for user to press Enter to launch a ship
    bool waitShip;
    bool gameOver;

    int shipsRemain;
    int score;
    int level;

    enum Action { Invalid, Launch, Thrust, RotateLeft, RotateRight, Shoot,
                    Teleport, Brake, Shield };

    QMap<Action, KAction*> keycodes;

    KAction *launchAction;
};

#endif

