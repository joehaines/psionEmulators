#ifndef BASE_FIELD_H
#define BASE_FIELD_H

#include <kexthighscore.h>


class QVBoxLayout;
class QGridLayout;
class KGameLCD;
class KGameLCDList;
class BaseBoard;
class QLabel;
class QButton;
class GTInitData;
class KCanvasRootPixmap;

class BaseField
{
 public:
    BaseField(QWidget *widget);
    virtual ~BaseField() {}
        
    virtual KExtHighscore::Score currentScore() const = 0;
    static void gameOver(const KExtHighscore::Score &, QWidget *parent);

    virtual void setArcade();
    bool isArcade() const;

 protected:
    QGridLayout  *top, *lcds;
    KGameLCD     *showScore;
    KGameLCDList *removedList, *scoreList;
    BaseBoard    *board;

    virtual void scoreUpdated();
    virtual void init(bool AI, bool multiplayer, bool server, bool first,
                      const QString &name);
    virtual void start(const GTInitData &);
    virtual void pause(bool pause);
    virtual void stop(bool gameover);
    virtual void settingsChanged();

 private:
    QWidget   *_widget;
    struct Flags {
        bool AI, multiplayer, server, first;
    };
    Flags        _flags;
    uint         _arcadeStage;
    QVBoxLayout *_boardLayout;
    QLabel      *_label;
    QButton     *_button;
    KCanvasRootPixmap *_boardRootPixmap;
    KExtHighscore::Score _firstScore, _lastScore;

    enum ButtonType { StartButton = 0, ResumeButton, ProceedButton,
                      NB_BUTTON_TYPE, NoButton = NB_BUTTON_TYPE };
    static const char *BUTTON_TEXTS[NB_BUTTON_TYPE];

    bool hasButton() const { return _flags.server && _flags.first; }
	void setMessage(const QString &label, ButtonType);
    void hideMessage() { setMessage(QString::null, NB_BUTTON_TYPE); }
};

#endif
