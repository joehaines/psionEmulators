#include "inter.h"

#include <kexthighscore.h>


void BaseInterface::showHighscores(QWidget *parent)
{
    if ( !_isPaused() ) _pause();
    _showHighscores(parent);
}

void BaseInterface::_showHighscores(QWidget *parent)
{
    KExtHighscore::show(parent);
}
