#include "field.h"
#include "field.moc"

#include <qwhatsthis.h>

#include <klocale.h>
#include <kgamelcd.h>

#include "common/commonprefs.h"
#include "board.h"


FEField::FEField(QWidget *parent)
    : Field(parent)
{
    connect(board, SIGNAL(removedUpdated(uint, uint)),
            SLOT(removedUpdated(uint, uint)));
    Board *b = static_cast<Board *>(board);
    QWhatsThis::add(b->giftPool(), i18n("Display the amount of foul eggs sent by your opponent."));
}

void FEField::removedUpdated(uint index, uint removed)
{
    KGameLCD *lcd = static_cast<KGameLCD *>(removedList->lcd(0));
    lcd->displayInt(static_cast<const FEBoard *>(board)->nbPuyos());
    if ( static_cast<const FEBoard *>(board)->nbPuyos() ) lcd->highlight();
    if ( removedList->size()!=1 ) {
      lcd = static_cast<KGameLCD *>(removedList->lcd(index+1));
      lcd->displayInt(removed);
      if (removed) lcd->highlight();
    }
}

void FEField::settingsChanged()
{
    Field::settingsChanged();

    removedList->clear();
    KGameLCD *lcd = new KGameLCD(6, removedList);
    removedList->append(i18n("Total:"), lcd);
    uint nb = static_cast<const FEBoard *>(board)->nbPuyos();
    lcd->displayInt(nb);
    lcd->show();
        
    if ( CommonPrefs::showDetailedRemoved() ) {
      QWhatsThis::add(removedList,
                      i18n("Display the number of removed groups (\"puyos\") classified by the number of chained removal."));
      for (uint i=0; i<4; i++) {
          KGameLCD *lcd = new KGameLCD(6, removedList);
          QString s = (i==3 ? ">3" : QString::number(i));
          removedList->append(s, lcd);
          uint nb = static_cast<const FEBoard *>(board)->nbChainedPuyos(i);
          lcd->displayInt(nb);
          lcd->show();
      }
    } else 
      QWhatsThis::add(removedList,
                    i18n("Display the number of removed groups (\"puyos\")."));
}
