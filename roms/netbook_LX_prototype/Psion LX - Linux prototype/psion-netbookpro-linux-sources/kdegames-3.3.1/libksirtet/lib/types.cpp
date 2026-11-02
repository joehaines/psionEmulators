#include "types.h"

#include <klocale.h>
#include "version.h"

cId::cId(const QString &_gameName, const QString &_gameId)
: libId(MULTI_ID), gameName(_gameName), gameId(_gameId)
{}

void cId::check(const cId &id)
{
	if ( libId!=id.libId )            state = LibIdClash;
	else if ( gameName!=id.gameName ) state = GameNameClash;
	else if ( gameId!=id.gameId )     state = GameIdClash;
	else                              state = Accepted;
}

QString cId::errorMessage(const cId &id) const
{
	QString str = i18n("\nServer: \"%1\"\nClient: \"%2\"");

	switch (state) {
	 case Accepted: return QString::null;
	 case LibIdClash:
		return i18n("The MultiPlayer library of the server is incompatible")
			+ str.arg(libId).arg(id.libId);
	 case GameNameClash:
		return i18n("Trying to connect a server for another game type")
			+ str.arg(gameName).arg(id.gameName);
	 case GameIdClash:
		return i18n("The server game version is incompatible")
			+ str.arg(gameId).arg(id.gameId);
	}
	Q_ASSERT(0);
	return QString::null;
}

QDataStream &operator << (QDataStream &s, const cId &id)
{
	s << id.libId << id.gameName << id.gameId << (Q_UINT8)id.state;
	return s;
}

QDataStream &operator >> (QDataStream &s, cId &id)
{
	Q_UINT8 state;
	s >> id.libId >> id.gameName >> id.gameId >> state;
	id.state = (cId::State)state;
	return s;
}

//-----------------------------------------------------------------------------
QDataStream &operator << (QDataStream &s, const MeetingMsgFlag &f)
{
	s << (Q_UINT8)f;
	return s;
}

QDataStream &operator >> (QDataStream &s, MeetingMsgFlag &f)
{
	Q_UINT8 i;
	s >> i; f = (MeetingMsgFlag)i;
	return s;
}

//-----------------------------------------------------------------------------
QDataStream &operator << (QDataStream &s, const TextInfo &ti)
{
	s << (Q_UINT32)ti.i << ti.text;
	return s;
}

QDataStream &operator >> (QDataStream &s, TextInfo &ti)
{
	Q_UINT32 i;
	s >> i >> ti.text; ti.i = i;
	return s;
}

//-----------------------------------------------------------------------------
QDataStream &operator << (QDataStream &s, const MeetingCheckBox::Type &t)
{
	s << (Q_UINT8)t;
	return s;
}

QDataStream &operator >> (QDataStream &s, MeetingCheckBox::Type &t)
{
	Q_UINT8 i;
	s >> i; t = (MeetingCheckBox::Type)i;
	return s;
}

//-----------------------------------------------------------------------------
QDataStream &operator << (QDataStream &s, const TypeInfo &ti)
{
	s << (Q_UINT32)ti.i << ti.type;
	return s;
}

QDataStream &operator >> (QDataStream &s, TypeInfo &ti)
{
	Q_UINT32 i;
	s >> i >> ti.type; ti.i = i;
	return s;
}

//-----------------------------------------------------------------------------
QDataStream &operator << (QDataStream &s, const BoardData &bd)
{
	s << (Q_UINT8)bd.type << bd.name;
	return s;
}

QDataStream &operator >> (QDataStream &s, BoardData &bd)
{
    Q_UINT8 i;
	s >> i >> bd.name;
    bd.type = (PlayerComboBox::Type)i;
	return s;
}

//-----------------------------------------------------------------------------
QDataStream &operator << (QDataStream &s, const ExtData &ed)
{
	s << ed.bds << ed.text << ed.type;
	return s;
}

QDataStream &operator >> (QDataStream &s, ExtData &ed)
{
	s >> ed.bds >> ed.text >> ed.type;
	return s;
}

//-----------------------------------------------------------------------------
QDataStream &operator << (QDataStream &s, const MeetingLineData &pld)
{
	s << pld.ed << (Q_UINT8)pld.own;
	return s;
}

QDataStream &operator >> (QDataStream &s, MeetingLineData &pld)
{
	Q_UINT8 b;
	s >> pld.ed >> b; pld.own = b;
	return s;
}

//-----------------------------------------------------------------------------
QDataStream &operator << (QDataStream &s, const MetaFlag &f)
{
	s << (Q_UINT8)f;
	return s;
}

QDataStream &operator >> (QDataStream &s, MetaFlag &f)
{
	Q_UINT8 i;
	s >> i; f = (MetaFlag)i;
	return s;
}


//-----------------------------------------------------------------------------
Stream::Stream(int _mode)
: mode(_mode)
{
	setDevice(&buf);
	Q_ASSERT( _mode==IO_ReadOnly || _mode==IO_WriteOnly );
	buf.open(_mode);
}

void Stream::clear()
{
	buf.close();
	buf.open(mode | IO_Truncate);
}

void Stream::setArray(QByteArray a)
{
	buf.close();
	buf.setBuffer(a);
	buf.open(mode);
}

bool ReadingStream::readOk()
{
	return ( buf.status()==IO_Ok );
}

void ReadingStream::clearRead()
{
	int i = buf.at();
	if ( i==0 ) return;
	buf.close();
	QByteArray a;
	a.duplicate(buffer().data() + i, size() - i);
	buf.setBuffer(a);
	buf.open(IO_ReadOnly);
}

//-----------------------------------------------------------------------------
void IOBuffer::writingToReading()
{
	// this should do the trick :)
	reading.setArray(writing.buffer());
	QByteArray a;
	writing.setArray(a);
}

//-----------------------------------------------------------------------------
void BufferArray::clear(uint k)
{
	for (uint i=k; i<a.size(); i++) delete a[i];
}

BufferArray::~BufferArray()
{
	clear(0);
}

void BufferArray::resize(uint nb)
{
	uint s = a.size();
	if ( nb<s ) clear(nb);
	a.resize(nb);
	for (uint i=s; i<nb; i++) a[i] = new IOBuffer;
}

QDataStream &operator <<(QDataStream &s, const BufferArray &b)
{
	for (uint i=0; i<b.size(); i++) {
		s.writeBytes(b[i]->writing.buffer().data(), b[i]->writing.size());
//		debug("BUFFERARRAY : << (i=%i size=%i)", i, b[i]->writing.size());
		b[i]->writing.clear();
	}
	return s;
}

QDataStream &operator >>(QDataStream &s, BufferArray &b)
{
	uint size;
	char *c;
	for (uint i=0; i<b.size(); i++) {
		s.readBytes(c, size);
		QByteArray a;
		a.assign(c, size);
		b[i]->reading.setArray(a);
//		debug("BUFFERARRAY : >> (i=%i c=%i size=%i s=%i)",
//			  i, (int)c, size, b[i]->reading.size());
	}
	return s;
}
