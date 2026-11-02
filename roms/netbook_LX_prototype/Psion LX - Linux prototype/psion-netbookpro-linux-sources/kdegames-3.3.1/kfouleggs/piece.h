#ifndef PIECE_H
#define PIECE_H

#include "base/piece.h"


class FEPieceInfo : public GPieceInfo
{
 public:
    FEPieceInfo() {}

    virtual uint nbBlocks() const { return NB_BLOCKS; }
    virtual uint nbForms()  const { return 1; }
    virtual uint nbTypes()  const
        { return NB_NORM_BLOCK_TYPES * NB_NORM_BLOCK_TYPES; }

    virtual const int *i(uint, uint rot) const { return FORM.i[rot]; }
    virtual const int *j(uint, uint rot) const { return FORM.j[rot]; }
    virtual uint value(uint type, uint n) const
        { return (n%2 ? type/4 : type%4); }
    virtual uint form(uint) const               { return 0; }
    virtual uint nbConfigurations(uint type) const
        { return ((type%4)==(type/4) ? 2 : 4);}

    virtual uint nbNormalBlockTypes() const  { return NB_NORM_BLOCK_TYPES; }
    virtual uint nbGarbageBlockTypes() const { return 1; }
    virtual uint nbBlockModes() const        { return NB_BLOCK_MODES; }

    virtual uint nbColors() const { return NB_NORM_BLOCK_TYPES + 1; }
    virtual QString colorLabel(uint i) const;
    virtual QColor defaultColor(uint i) const;

 private:
    void draw(QPixmap *, uint blockType, uint blockMode, bool lighted) const;
    void setMask(QPixmap *, uint blockMode) const;

    enum { NB_BLOCKS = 2,
           NB_NORM_BLOCK_TYPES = 4,
           NB_BLOCK_MODES = 1+4+6+4+1 };  // all possible connections

    struct Form {
        int i[4][NB_BLOCKS];
        int j[4][NB_BLOCKS];
    };
    static const Form FORM;

    static const char *DEFAULT_COLORS[NB_NORM_BLOCK_TYPES + 1];
};

#endif
