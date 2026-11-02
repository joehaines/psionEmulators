#ifndef BASE_PIECE_H
#define BASE_PIECE_H

#include <qcanvas.h>
#include <qptrvector.h>

#include <kgrid2d.h>


class KRandomSequence;

//-----------------------------------------------------------------------------
class GPieceInfo
{
 public:
    GPieceInfo();
    virtual ~GPieceInfo() {}

    virtual uint nbBlocks() const = 0; // nb of blocks in a piece
    virtual uint nbTypes() const  = 0; // nb of combin. of types in piece
    virtual uint nbForms() const  = 0; // nb of geometrical form of piece

    virtual const int *i(uint form, uint rotation) const = 0;
    virtual const int *j(uint form, uint rotation) const = 0;
    virtual uint value(uint type, uint n) const          = 0;
    virtual uint form(uint type) const                   = 0;
    virtual uint nbConfigurations(uint type) const       = 0;
    uint generateType(KRandomSequence *) const;

    KGrid2D::Coord maxSize() const;

    QPixmap *pixmap(uint blockSize, uint blockType, uint blockMode,
                    bool lighted) const;

    virtual uint nbNormalBlockTypes() const  = 0;
    virtual uint nbGarbageBlockTypes() const = 0;
    virtual uint nbBlockModes() const        = 0; // nb of modes per block
    bool isGarbage(uint type) const { return type>=nbNormalBlockTypes(); }
    uint generateGarbageBlockType(KRandomSequence *) const;

    virtual uint nbColors() const = 0;
    virtual QString colorLabel(uint i) const = 0;
    QCString colorKey(uint i) const;
    virtual QColor defaultColor(uint i) const = 0;
    void loadColors();

 protected:
    QColor color(uint i) const { return _colors[i]; }

    virtual void draw(QPixmap *, uint blockType, uint blockMode,
                      bool lighted) const = 0;
    virtual void setMask(QPixmap *, uint /*blockMode*/) const {}

 private:
    QValueVector<QColor> _colors;
};

class SequenceArray : public QMemArray<QCanvasPixmapArray *>
{
 public:
	SequenceArray();
	~SequenceArray();

	void setBlockSize(uint size);
    uint blockSize() const { return _size; }

 private:
	uint _size;
};

//-----------------------------------------------------------------------------
class BlockInfo : public QCanvas
{
 public:
    BlockInfo(const SequenceArray &);
    const SequenceArray &sequences() const { return _sequences; }

    QPoint toPoint(const KGrid2D::Coord &) const;

 private:
    const SequenceArray &_sequences;
};

//-----------------------------------------------------------------------------
class Block
{
 public:
    Block(uint value = 0);
    ~Block();

    void setValue(uint, BlockInfo *);
    uint value() const { return _value; }
    bool isGarbage() const;
    void toggleLight();
    QCanvasSprite *sprite() const { return _sprite; }

 private:
    uint           _value;
    QCanvasSprite *_sprite;

    Block(const Block &);             // disabled
    Block &operator =(const Block &); // disabled
};

//-----------------------------------------------------------------------------
class Piece
{
 public:
    Piece();

    void copy(const Piece *);
    void setBlockInfo(BlockInfo *bi)         { _binfo = bi; }
    static void setPieceInfo(GPieceInfo *pi) { _info = pi; }
    static GPieceInfo &info()                { return *_info; }

    uint type() const             { return _type; }
    uint nbBlocks() const         { return _blocks.size(); }
    uint nbConfigurations() const { return _info->nbConfigurations(_type); }

    int value(uint k) const       { return _blocks[k]->value(); }
    KGrid2D::Coord pos(uint k, const KGrid2D::Coord &) const;

    KGrid2D::Coord min() const;
    KGrid2D::Coord max() const;
    KGrid2D::Coord size() const { return max() - min() + KGrid2D::Coord(1, 1); }

    void generateNext(int type = -1);
    void rotate(bool left, const QPoint &);
    void move(const QPoint &);
    void moveCenter();
    void show(bool show);

    void setRandomSequence(KRandomSequence *random) { _random = random; }

    Block *garbageBlock() const;
    Block *takeBlock(uint k);

 private:
    QPtrVector<Block>  _blocks;
    uint               _type;
    KRandomSequence   *_random;
    static GPieceInfo *_info;
    BlockInfo         *_binfo;
    uint               _rotation;
    int const         *_i;
    int const         *_j;

    Piece(const Piece &);             // disabled
    Piece &operator =(const Piece &); // disabled

    KGrid2D::Coord coord(uint k) const { return KGrid2D::Coord(_i[k], _j[k]); }
    void moveBlock(uint k, const QPoint &);
};

#endif
