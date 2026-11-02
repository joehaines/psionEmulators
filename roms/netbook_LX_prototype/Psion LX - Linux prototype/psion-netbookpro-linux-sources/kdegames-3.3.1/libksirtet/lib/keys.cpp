#include "keys.h"
#include "keys.moc"

#include <qsignal.h>
#include <kkeydialog.h>
#include <klocale.h>


KeyData::KeyData(uint maxNb, uint nbActions, const ActionData *data,
                 QObject *parent)
    : QObject(parent), _maxNb(maxNb)
{
    _data.duplicate(data, nbActions);

    for (uint n=0; n<maxNb; n++)
        for (uint i=0; i<=n; i++)
            _keycodes[n][i].fill(0, nbActions);
}

void KeyData::setKeycodes(uint nb, uint index, const int *keycodes)
{
    Q_ASSERT( nb!=0 && nb-1<_maxNb && index<nb );
    for (uint n=nb-1; n<_maxNb; n++)
        for (uint k=0; k<_data.size(); k++)
            if ( n==nb-1 || _keycodes[n][index][k]==0 )
                _keycodes[n][index][k] = keycodes[k];
}

void KeyData::setCurrentNb(uint nb)
{
    Q_ASSERT( nb<_maxNb );
    clear();
    _cols.fill(0, nb);
}

void KeyData::clear()
{
    for (uint i=0; i<_cols.size(); i++)
        delete _cols[i];
    _cols.resize(0);
}

void KeyData::createActionCollection(uint index, QWidget *receiver)
{
    Q_ASSERT( index<_cols.size() );
    _cols[index] = new KActionCollection(receiver, this);
    for (uint k=0; k<_data.size(); k++) {
        QString label = i18n(_data[k].label);
        QString name = QString("%2 %3").arg(index+1).arg(_data[k].name);
        const char *slot = (_data[k].slotRelease ? 0 : _data[k].slot);
        KAction *a = new KAction(label, _keycodes[_cols.size()-1][index][k],
                                 receiver, slot, _cols[index], name.utf8());
        a->setEnabled(false);
        if ( slot==0 ) {
            SpecialData data;
            data.enabled = false;
            data.pressed = new QSignal(this);
            data.pressed->connect(receiver, _data[k].slot);
            data.released = new QSignal(this);
            data.released->connect(receiver, _data[k].slotRelease);
            _specActions[a] = data;
        }
    }
    _cols[index]->readShortcutSettings(group());
}

void KeyData::setEnabled(uint index, bool enabled)
{
    for (uint k=0; k<_cols[index]->count(); k++) {
        QMap<KAction *, SpecialData>::Iterator it =
            _specActions.find(_cols[index]->action(k));
        if ( it==_specActions.end() )
            _cols[index]->action(k)->setEnabled(enabled);
        else (*it).enabled = enabled;
    }
}

void KeyData::addKeys(KKeyDialog &d)
{
    for (uint i=0; i<_cols.size(); i++)
        d.insert(_cols[i], i18n("Shortcuts for player #%1/%2").arg(i+1)
                 .arg(_cols.size()));
}

void KeyData::save()
{
    for (uint i=0; i<_cols.size(); i++)
        _cols[i]->writeShortcutSettings(group());
}

void KeyData::keyEvent(QKeyEvent *e, bool pressed)
{
    if ( e->isAutoRepeat() ) return;

    KKey key(e);
    QMap<KAction *, SpecialData>::Iterator it = _specActions.begin();
    for(; it!=_specActions.end(); ++it) {
        if ( !it.data().enabled ) continue;
        if ( !it.key()->shortcut().contains(key) ) continue;
        if (pressed) it.data().pressed->activate();
        else it.data().released->activate();
    }
    e->ignore();
}
