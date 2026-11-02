#ifndef KEYS_H
#define KEYS_H

#include <qmap.h>
#include <kaction.h>

#include "mp_interface.h"


class KeyData : public QObject
{
 Q_OBJECT
 public:
    KeyData(uint maxNb, uint nbActions, const ActionData *,
            QObject *parent);
    void setKeycodes(uint nb, uint i, const int *keycodes);

    void setCurrentNb(uint nb);
    void clear();
    void createActionCollection(uint index, QWidget *receiver);
    void setEnabled(uint index, bool enabled);
    void addKeys(KKeyDialog &);
    void save();

    void keyEvent(QKeyEvent *e, bool pressed);

 private:
    uint                                   _maxNb;
    QMemArray<ActionData>                  _data;
    QMap<int, QMap<int, QMemArray<int> > > _keycodes;
    QMemArray<KActionCollection *>         _cols;
    struct SpecialData {
        bool enabled;
        QSignal *pressed, *released;
    };
    QMap<KAction *, SpecialData>           _specActions;

    QString group() const
        { return QString("Keys (%1 humans)").arg(_cols.size()); }
};

#endif // KEYS_H
