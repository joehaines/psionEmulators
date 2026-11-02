#ifndef KS_PIECE_H
#define KS_PIECE_H

#include <klocale.h>
#include "base/piece.h"


class KSPieceInfo : public GPieceInfo
{
 public:
    KSPieceInfo() : _oldRotationStyle(false) {}

    void setOldRotationStyle(bool oldStyle) { _oldRotationStyle = oldStyle; }

    virtual uint nbBlocks() const { return NB_BLOCKS; }
    virtual uint nbForms()  const { return NB_FORMS; }
    virtual uint nbTypes()  const { return NB_FORMS; }

    virtual const int *i(uint form, uint rot) const {
        return (_oldRotationStyle ? FORMS[form].oi[rot] : FORMS[form].i[rot]);
    }
    virtual const int *j(uint form, uint rot) const {
        return (_oldRotationStyle ? FORMS[form].oj[rot] : FORMS[form].j[rot]);
    }
    virtual uint value(uint type, uint) const       { return type; }
    virtual uint form(uint type) const              { return type; }
    virtual uint nbConfigurations(uint type) const  { return FORMS[type].nbConfigs; }

    virtual uint nbNormalBlockTypes() const         { return NB_FORMS; }
    virtual uint nbGarbageBlockTypes() const        { return 1; }
    virtual uint nbBlockModes() const               { return 1; }

    virtual uint nbColors() const { return NB_FORMS + 1; }
    virtual QString colorLabel(uint i) const { return i18n(COLOR_LABELS[i]); }
    virtual QColor defaultColor(uint i) const;

 private:
    virtual void draw(QPixmap *, uint blockType, uint blockMode,
                      bool lighted) const;

 private:
    bool _oldRotationStyle;

    enum { NB_BLOCKS = 4,
           NB_FORMS  = 7 };

    struct Form {
        int i[4][NB_BLOCKS];  // new rotation style
        int j[4][NB_BLOCKS];
        int oi[4][NB_BLOCKS]; // old rotation style
        int oj[4][NB_BLOCKS];
        uint nbConfigs;      // number of unique configs
    };
    static const Form FORMS[NB_FORMS];

    static const char *COLOR_LABELS[NB_FORMS+1];
    static const char *DEFAULT_COLORS[NB_FORMS+1];
};

#endif



