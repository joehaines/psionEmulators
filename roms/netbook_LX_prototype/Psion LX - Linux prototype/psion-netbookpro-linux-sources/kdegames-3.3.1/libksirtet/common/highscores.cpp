#include "highscores.h"

#include <klocale.h>
#include <kapplication.h>
#include <kconfig.h>

#include "base/factory.h"


using namespace KExtHighscore;

CommonHighscores::CommonHighscores()
{
    Item *item = new Item((uint)1, i18n("Level"), Qt::AlignRight);
    addScoreItem("level", item);
    item = new Item((uint)0, i18n(bfactory->mainData.removedLabel),
                    Qt::AlignRight);
    addScoreItem("removed", item);
}

void CommonHighscores::convertLegacy(uint)
{
    KConfigGroupSaver cg(kapp->config(), "High Scores");
    for (uint i=0; i<10; i++) {
        QString name
            = cg.config()->readEntry(QString("name%1").arg(i), QString::null);
        if ( name.isNull() ) break;
        if ( name.isEmpty() ) name = i18n("anonymous");
        uint score
            = cg.config()->readUnsignedNumEntry(QString("score%1").arg(i), 0);
        uint level
            = cg.config()->readUnsignedNumEntry(QString("level%1").arg(i), 1);
        Score s(Won);
        s.setScore(score);
        s.setData("name", name);
        s.setData("level", level);
        submitLegacyScore(s);
    }
}

bool CommonHighscores::isStrictlyLess(const Score &s1, const Score &s2) const
{
    uint l1 = s1.data("level").toUInt();
    uint r1 = s1.data("removed").toUInt();
    uint l2 = s2.data("level").toUInt();
    uint r2 = s2.data("removed").toUInt();

    if ( s1.score()==s2.score() ) {
        if ( l1==l2 ) return r1<r2;
        else return l1<l2;
    } else return BaseHighscores::isStrictlyLess(s1, s2);
}

void CommonHighscores::additionalQueryItems(KURL &url, const Score &s) const
{
    uint l = s.data("level").toUInt();
    addToQueryURL(url, "scoreLevel", QString::number(l));
    uint r = s.data("removed").toUInt();
    addToQueryURL(url, "scoreRemoved", QString::number(r));
}
