#ifndef MTYPES_H
#define MTYPES_H

#include <qstring.h>
#include <qbuffer.h>
#include <qvaluelist.h>

#include "miscui.h"


/** Internal class : used for client identification. */
class cId
{
 public:
	cId() {}
	cId(const QString &gameName, const QString &gameId);

	enum State { Accepted, LibIdClash, GameNameClash, GameIdClash };
	void check(const cId &id);
	bool accepted() const { return state==Accepted; }
	QString errorMessage(const cId &id) const;

	friend QDataStream &operator << (QDataStream &s, const cId &id);
	friend QDataStream &operator >> (QDataStream &s, cId &id);

 private:
	QString libId, gameName, gameId;
	State   state;
};
QDataStream &operator << (QDataStream &s, const cId &id);
QDataStream &operator >> (QDataStream &s, cId &id);

/** Flags used for the netmeeting. */
enum MeetingMsgFlag
    { IdFlag = 0, EndFlag, NewFlag, DelFlag, Mod_TextFlag, Mod_TypeFlag, Mod_OptFlag, PlayFlag };
QDataStream &operator << (QDataStream &s, const MeetingMsgFlag &f);
QDataStream &operator >> (QDataStream &s, MeetingMsgFlag &f);

/** Internal class : used in netmeeting to transport text line. */
class TextInfo
{
 public:
	TextInfo() {}

	uint i;
	QString text;
};
QDataStream &operator << (QDataStream &s, const TextInfo &ti);
QDataStream &operator >> (QDataStream &s, TextInfo &ti);

/** Internal class : used in netmeeting to transport readiness status. */
typedef struct {
	uint i;
	MeetingCheckBox::Type type;
} TypeInfo;
QDataStream &operator << (QDataStream &s, const MeetingCheckBox::Type &t);
QDataStream &operator >> (QDataStream &s, MeetingCheckBox::Type &t);
QDataStream &operator << (QDataStream &s, const TypeInfo &ti);
QDataStream &operator >> (QDataStream &s, TypeInfo &ti);

/* Internal class : store game data. */
class BoardData
{
 public:
	BoardData() {}

	QString              name;
	PlayerComboBox::Type type;
};
QDataStream &operator <<(QDataStream &, const BoardData &);
QDataStream &operator >>(QDataStream &, BoardData &);

/* Internal class : store extended game data (used in netmeeting). */
class ExtData
{
 public:
	ExtData() {}
	ExtData(const QValueList<BoardData> &_bds, const QString &_text,
			MeetingCheckBox::Type _type)
	: bds(_bds), text(_text), type(_type) {}

	QValueList<BoardData> bds;
	QString               text;
	MeetingCheckBox::Type type;
};
QDataStream &operator << (QDataStream &s, const ExtData &ed);
QDataStream &operator >> (QDataStream &s, ExtData &ed);

/* Internal class : store meeting line data (in netmeeting). */
class MeetingLineData
{
 public:
	MeetingLineData() {}

	ExtData ed;
	bool own;
};
QDataStream &operator << (QDataStream &s, const MeetingLineData &pld);
QDataStream &operator >> (QDataStream &s, MeetingLineData &pld);

/* Internal class : store remote host data. */
class Socket;

class RemoteHostData
{
 public:
	RemoteHostData() : socket(0) {}

	Socket                *socket;
	QValueList<BoardData>  bds;
};

/* Internal class : store connection data (used by config. wizard). */
class ConnectionData
{
 public:
	ConnectionData() {}

	bool           network, server;
	RemoteHostData rhd;
};

/** Flags used for network communication. */
enum MetaFlag { MF_Ask = 0, MF_Data };
QDataStream &operator << (QDataStream &s, const MetaFlag &f);
QDataStream &operator >> (QDataStream &s, MetaFlag &f);

/** Internal class : encapsulate read/write QBuffer. */
class Stream : public QDataStream
{
 public:
	Stream(int mode);

	void clear();
	void setArray(QByteArray a);

	QByteArray buffer() const { return buf.buffer(); }
	uint size() const { return buf.buffer().size(); }

 protected:
	QBuffer buf;

 private:
	int mode;
};

/** Internal class : encapsulate write QBuffer. */
class WritingStream : public Stream
{
 public:
	WritingStream() : Stream(IO_WriteOnly) {}
};

/** Internal class : encapsulate read QBuffer. */
class ReadingStream : public Stream
{
 public:
	ReadingStream() : Stream(IO_ReadOnly) {}

	bool readOk();
	void clearRead();
};

/** Internal class : include a @ref ReadingStream and a @ref WritingStream. */
class IOBuffer
{
 public:
	IOBuffer() {}

	void writingToReading();

	ReadingStream reading;
	WritingStream writing;
};

/** Internal class : array of @ref IOBuffer. */
class BufferArray
{
 public:
	BufferArray() {}
	BufferArray(uint nb) { resize(nb); }
	~BufferArray();

	void resize(uint nb);

	uint size() const { return a.size(); }
	IOBuffer *operator [](uint i) const { return a[i]; }

 private:
	QMemArray<IOBuffer *> a;

	void clear(uint nb);
};
QDataStream &operator <<(QDataStream &s, const BufferArray &b);
QDataStream &operator >>(QDataStream &s, BufferArray &b);

#endif // MTYPES_H
