#include "piece.h"

#include <kglobal.h>
#include <krandomsequence.h>

#include "baseprefs.h"

using namespace KGrid2D;

QPoint operator *(const Coord &c, int i)
{
    return QPoint(c.first * i, c.second * i);
}

//-----------------------------------------------------------------------------
GPieceInfo::GPieceInfo()
{
	Piece::setPieceInfo(this);
}

QPixmap *GPieceInfo::pixmap(uint blockSize, uint blockType, uint blockMode,
                            bool lighted) const
{
    QPixmap *pixmap = new QPixmap(blockSize, blockSize);
    draw(pixmap, blockType, blockMode, lighted);
    setMask(pixmap, blockMode);
    return pixmap;
}

Coord GPieceInfo::maxSize() const
{
    Coord min, max;
    Coord size(0, 0);
	for (uint n=0; n<nbForms(); n++) {
        min = max = Coord(i(n, 0)[0], j(n, 0)[0]);
        for (uint k=0; k<nbBlocks(); k++) {
            Coord tmp = Coord(i(n, 0)[k], j(n, 0)[k]);
            max = maximum(max, tmp);
            min = minimum(min, tmp);
        }
        size = maximum(size, max - min);
    }
    return size;
}

uint GPieceInfo::generateType(KRandomSequence *random) const
{
    return random->getLong( nbTypes() );
}

uint GPieceInfo::generateGarbageBlockType(KRandomSequence *random) const
{
    return nbNormalBlockTypes() + random->getLong( nbGarbageBlockTypes() );
}

void GPieceInfo::loadColors()
{
    _colors.resize(nbColors());
    for (uint i=0; i<_colors.size(); i++)
        _colors[i] = BasePrefs::color(i);
}


//-----------------------------------------------------------------------------
SequenceArray::SequenceArray()
: _size(0)
{
    const GPieceInfo &pinfo = Piece::info();
    fill(0, pinfo.nbNormalBlockTypes() + pinfo.nbGarbageBlockTypes());
}

void SequenceArray::setBlockSize(uint bsize)
{
    _size = bsize;
    const GPieceInfo &pinfo = Piece::info();
    QPtrList<QPixmap> pixmaps;
    pixmaps.setAutoDelete(TRUE);
    QPtrList<QPoint> points;
    points.setAutoDelete(TRUE);
    uint nm = pinfo.nbBlockModes();
    for (uint i=0; i<size(); i++) {
        for (uint k=0; k<2; k++)
            for (uint j=0; j<nm; j++) {
                QPoint *po = new QPoint(0, 0);
                QPixmap *pi = pinfo.pixmap(bsize, i, j, k==1);
                if ( at(i) ) {
                    at(i)->setImage(k*nm + j, new QCanvasPixmap(*pi, *po));
                    delete po;
                    delete pi;
                } else {
                    points.append(po);
                    pixmaps.append(pi);
                }
            }
        if ( at(i)==0 ) {
            at(i) = new QCanvasPixmapArray(pixmaps, points);
            pixmaps.clear();
            points.clear();
        }
    }
}

SequenceArray::~SequenceArray()
{
	for (uint i=0; i<size(); i++) delete at(i);
}

//-----------------------------------------------------------------------------
BlockInfo::BlockInfo(const SequenceArray &s)
: _sequences(s)
{}

QPoint BlockInfo::toPoint(const Coord &pos) const
{
	return pos * _sequences.blockSize();
}

//-----------------------------------------------------------------------------
Block::Block(uint value)
: _value(value), _sprite(0)
{}

Block::~Block()
{
	delete _sprite;
}

void Block::setValue(uint value, BlockInfo *binfo)
{
	_value = value;
	if (binfo) {
		QCanvasPixmapArray *seq = binfo->sequences()[value];
		if (_sprite) _sprite->setSequence(seq);
		else {
			_sprite = new QCanvasSprite(seq, binfo);
			_sprite->setZ(0);
		}
	}
}

void Block::toggleLight()
{
	const GPieceInfo &pinfo = Piece::info();
	uint f = _sprite->frame() + pinfo.nbBlockModes()
		* (_sprite->frame()>=(int)pinfo.nbBlockModes() ? -1 : 1);
	_sprite->setFrame(f);
}

bool Block::isGarbage() const
{
	return Piece::info().isGarbage(_value);
}


//-----------------------------------------------------------------------------
GPieceInfo *Piece::_info = 0;

Piece::Piece()
    : _binfo(0), _i(0), _j(0)
{
    _blocks.setAutoDelete(true);
}

void Piece::rotate(bool left, const QPoint &p)
{
    if (left) {
        if ( _rotation==0 ) _rotation = 3;
        else _rotation--;
    } else {
        if ( _rotation==3 ) _rotation = 0;
        else _rotation++;
    }

    uint form = _info->form(_type);
    _i = _info->i(form, _rotation);
    _j = _info->j(form, _rotation);
    if (_binfo) move(p);
}

Coord Piece::min() const
{
    if ( _i==0 || _j==0 ) return Coord(0, 0);
    Coord min = coord(0);
    for(uint k=1; k<_info->nbBlocks(); k++)
        min = minimum(min, coord(k));
    return min;
}

Coord Piece::max() const
{
    if ( _i==0 || _j==0 ) return Coord(0, 0);
    Coord max = coord(0);
    for(uint k=1; k<_info->nbBlocks(); k++)
        max = maximum(max, coord(k));
    return max;
}

void Piece::copy(const Piece *p)
{
    if ( p->_blocks.size()!=0 ) {
        _blocks.resize(p->_blocks.size());
        for (uint k=0; k<_blocks.size(); k++) {
            if ( _blocks[k]==0 ) _blocks.insert(k, new Block);
            _blocks[k]->setValue(p->_blocks[k]->value(), _binfo);
        }
    }
    _type = p->_type;
    _random = p->_random;
    _rotation = p->_rotation;
    _i = p->_i;
    _j = p->_j;
}

void Piece::generateNext(int type)
{
    if ( _blocks.size()==0 ) {
        _blocks.resize(_info->nbBlocks());
        for (uint k=0; k<_blocks.size(); k++) _blocks.insert(k, new Block);
    }
    _type = (type==-1 ? _info->generateType(_random) : (uint)type );
    _rotation = 0;

    uint form = _info->form(_type);
    _i = _info->i(form, _rotation);
    _j = _info->j(form, _rotation);

    for (uint k=0; k<_blocks.size(); k++)
        _blocks[k]->setValue(_info->value(_type, k), _binfo);
}

void Piece::moveCenter()
{
    uint s = _binfo->sequences().blockSize();
    QPoint p = QPoint(_binfo->width(), _binfo->height()) - size() * s;
    move(p/2 - min() * s);
}

Coord Piece::pos(uint k, const Coord &pos) const
{
    return Coord(pos.first + coord(k).first, pos.second - coord(k).second);
}

void Piece::move(const QPoint &p)
{
    for (uint k=0; k<_blocks.size(); k++) moveBlock(k, p);
}

void Piece::moveBlock(uint k, const QPoint &p)
{
    QPoint po = p + _binfo->toPoint(coord(k));
    _blocks[k]->sprite()->move(po.x(), po.y());
}

Block *Piece::garbageBlock() const
{
    Block *b = new Block;
    b->setValue(_info->generateGarbageBlockType(_random), _binfo);
    return b;
}

Block *Piece::takeBlock(uint k)
{
    Block *b = _blocks.take(k);
    _blocks.insert(k, new Block);
    return b;
}

void Piece::show(bool show)
{
    for (uint k=0; k<_blocks.size(); k++) {
        if (show) _blocks[k]->sprite()->show();
        else _blocks[k]->sprite()->hide();
    }
}
