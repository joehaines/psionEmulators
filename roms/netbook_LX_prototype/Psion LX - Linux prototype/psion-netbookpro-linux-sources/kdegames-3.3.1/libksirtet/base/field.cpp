#include "field.h"

#include <qwhatsthis.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qpushbutton.h>

#include <klocale.h>
#include <kcanvasrootpixmap.h>
#include <knotifyclient.h>
#include <kgamelcd.h>

#include "factory.h"
#include "board.h"
#include "baseprefs.h"


const char *BaseField::BUTTON_TEXTS[NB_BUTTON_TYPE] = {
    I18N_NOOP("Start"), I18N_NOOP("Resume"), I18N_NOOP("Proceed")
};

BaseField::BaseField(QWidget *w)
    : _widget(w), _boardLayout(0), _label(0), _button(0)
{
    top = new QGridLayout(w, 3, 5, 10);

    lcds = new QGridLayout(7, 1, 5);
    top->addLayout(lcds, 1, 0);
    lcds->setRowStretch(1, 0);

    board = bfactory->createBoard(true, w);
    _boardRootPixmap = new KCanvasRootPixmap(board);
    _boardRootPixmap->start();
    top->addWidget(board, 1, 2);
}

void BaseField::init(bool AI, bool multiplayer, bool server, bool first,
                     const QString &name)
{
    _flags.AI = AI;
    _flags.multiplayer = multiplayer;
    _flags.server = server;
    _flags.first = first;
	QString text = (AI ? i18n("%1\n(AI player)").arg(name)
                    : (multiplayer ? i18n("%1\n(Human player)").arg(name)
                       : QString::null));
	if ( first && !server ) text += i18n("\nWaiting for server");
	setMessage(text, (first && server ? StartButton : NoButton));
    showScore->resetColor();
    board->init(false);
}

void BaseField::setArcade()
{
    board->init(true);
    setMessage(i18n("Stage #1"), StartButton);
}

bool BaseField::isArcade() const
{
    return board->isArcade();
}

void BaseField::setMessage(const QString &label, ButtonType type)
{
    delete _label;
    _label = 0;
    delete _button;
    _button = 0;
    delete _boardLayout;
    _boardLayout = 0;

    if ( label.isEmpty() && type==NoButton ) {
        _widget->setFocus();
        return;
    }

    _boardLayout = new QVBoxLayout(board);
    _boardLayout->addStretch(3);
    if ( !label.isEmpty() ) {
        QString str = (isArcade() ? i18n("Arcade game") + '\n'
                       : QString::null) + label;
        _label = new QLabel(str, board);
        _label->setAlignment(Qt::AlignCenter);
        _label->setFrameStyle( QFrame::Panel | QFrame::Sunken );
        _boardLayout->addWidget(_label, 0, Qt::AlignCenter);
        _label->show();
    }
    _boardLayout->addStretch(1);
    if ( type!=NoButton ) {
        _button = new QPushButton(i18n(BUTTON_TEXTS[type]), board);
        _button->setFocus();
        const char *slot = (type==ResumeButton ? SLOT(pause())
                            : SLOT(start()));
        _button->connect(_button, SIGNAL(clicked()),
                              _widget->parent(), slot);
        _boardLayout->addWidget(_button, 0, Qt::AlignCenter);
        _button->show();
    }
    _boardLayout->addStretch(3);
}

void BaseField::start(const GTInitData &data)
{
    _firstScore = KExtHighscore::firstScore();
    _lastScore = KExtHighscore::lastScore();
    hideMessage();
	board->start(data);
}

void BaseField::pause(bool pause)
{
	if (pause) {
		board->pause();
        setMessage(i18n("Game paused"), ResumeButton);
	} else {
		board->unpause();
        hideMessage();
	}
}

void BaseField::stop(bool gameover)
{
	board->stop();
    ButtonType button = StartButton;
    QString msg = (gameover ? i18n("Game over") : QString::null);
    if ( board->isArcade() && board->arcadeStageDone() ) {
        if ( board->arcadeStage()==bfactory->bbi.nbArcadeStages )
            msg = i18n("The End");
        else {
            msg = i18n("Stage #%1 done").arg(board->arcadeStage());
            button = ProceedButton;
        }
    }
    setMessage(msg, button);
}

void BaseField::gameOver(const KExtHighscore::Score &score, QWidget *parent)
{
    KNotifyClient::event(parent->winId(), "game over", i18n("Game Over"));
    KExtHighscore::submitScore(score, parent);
}

void BaseField::scoreUpdated()
{
    showScore->display( (int)board->score() );
    if (_flags.multiplayer) return;

    QColor color;
    if ( _firstScore<currentScore() ) color = Qt::red;
    else if ( _lastScore<currentScore() ) color = Qt::blue;
    showScore->setColor(color);
}

void BaseField::settingsChanged()
{
    QColor color = BasePrefs::fadeColor();
    double s = BasePrefs::fadeIntensity();
    _boardRootPixmap->setFadeEffect(s, color);
    board->canvas()->setBackgroundColor(color);
    board->settingsChanged();
}
