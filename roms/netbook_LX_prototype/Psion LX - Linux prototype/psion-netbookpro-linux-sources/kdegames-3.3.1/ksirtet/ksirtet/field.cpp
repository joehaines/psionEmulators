#include "field.h"
#include "field.moc"

#include <qwhatsthis.h>

#include <klocale.h>
#include <kgamelcd.h>

#include "board.h"
#include "prefs.h"
#include "piece.h"

//-----------------------------------------------------------------------------
KSField::KSField(QWidget *parent)
    : Field(parent)
{
    connect(board, SIGNAL(removedLinesUpdated(uint, uint)),
            SLOT(removedLinesUpdated(uint, uint)));
            
    Board *b = static_cast<Board *>(board);
    QWhatsThis::add(b->giftPool(), i18n("Indicate the number of garbage lines you received from your opponent."));
}

void KSField::removedLinesUpdated(uint index, uint removed)
{
    KGameLCD *lcd = static_cast<KGameLCD *>(removedList->lcd(0));
    lcd->displayInt(board->nbRemoved());
    if ( board->nbRemoved() ) lcd->highlight();
    if ( removedList->size()!=1 ) {
      lcd = static_cast<KGameLCD *>(removedList->lcd(index+1));
      lcd->displayInt(removed);
      if (removed) lcd->highlight();
    }
}

void KSField::settingsChanged()
{
    Field::settingsChanged();
    bool b = Prefs::oldRotationStyle();
    static_cast<KSPieceInfo &>(Piece::info()).setOldRotationStyle(b);

    b = Prefs::showDetailedRemoved();
    
    removedList->clear();
    QWhatsThis::remove(removedList);
    KGameLCD *lcd = new KGameLCD(5, removedList);
    QString s = (b ? i18n("Total:") : QString::null);
    removedList->append(s, lcd);
    lcd->displayInt( board->nbRemoved() );
    lcd->show();
        
    if (b) {
        for (uint i=0; i<4; i++) {
            KGameLCD *lcd = new KGameLCD(5, removedList);
            QString s = i18n("1 Line:", "%n Lines:", i+1);
            removedList->append(s, lcd);
            uint nb = static_cast<const KSBoard *>(board)->nbRemovedLines(i);
            lcd->displayInt(nb);
            lcd->show();
        }
    }
}
