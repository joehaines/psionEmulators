#include "highscores.h"

#include <kurl.h>

#include "factory.h"


using namespace KExtHighscore;

BaseHighscores::BaseHighscores()
{
    setWWHighscores(KURL( bfactory->mainData.homepage ), bfactory->mainData.version);
    const BaseBoardInfo &bi = bfactory->bbi;
    if ( bi.histogramSize!=0 ) {
        QMemArray<uint> a;
        a.duplicate(bi.histogram, bi.histogramSize);
        setScoreHistogram(a, bi.scoreBound ? ScoreBound : ScoreNotBound);
    }
}
