#ifndef COMMON_INTER_H
#define COMMON_INTER_H

#include <kexthighscore.h>

#include "lib/mp_simple_interface.h"
#include "base/inter.h"
#include "types.h"
#include "board.h"


class Field;

class Interface : public MPSimpleInterface, public BaseInterface
{
    Q_OBJECT
public:
    Interface(const MPGameInfo &, QWidget *parent);

signals:
    void settingsChanged();

public slots:
    void normalGame();
    void arcadeGame();
    void settingsChangedSlot() { emit settingsChanged(); }

protected:
    void _showHighscores(QWidget *parent);

private:
    QMemArray<ClientPlayData> _data;
    KExtHighscore::Score _score;
    KExtHighscore::MultiplayerScores _scores;
    Field *_firstField;

    enum Action { Nb_Actions = 8 };
    static const ActionData ACTION_DATA[Nb_Actions];
    static const int KEYCODE_ONE[Nb_Actions];
    static const int KEYCODE_TWO[Nb_Actions];

    MPBoard *newBoard(uint);
    void setInitData(uint player, ServerInitData &);
    uint prev(uint i) const;
    uint next(uint i) const;

    void _readGameOverData(QDataStream &s);
    void _sendGameOverData(QDataStream &s);
    void _firstInit() {}
    void _treatInit();
    void _init();
    void _showGameOverData();
    bool _readPlayData();
    void _sendPlayData();

    void _start() { MPSimpleInterface::start(); }
    void _pause() { MPSimpleInterface::pause(); }
    bool _isPaused() const { return MPSimpleInterface::isPaused(); }
};

#endif
