#ifndef KS_FIELD_H
#define KS_FIELD_H

#include "common/field.h"
#include "common/misc_ui.h"


class KSField : public Field
{
 Q_OBJECT
 public:
    KSField(QWidget *parent);

 private slots:
    void removedLinesUpdated(uint i, uint removed);
    virtual void settingsChanged();
};

#endif
