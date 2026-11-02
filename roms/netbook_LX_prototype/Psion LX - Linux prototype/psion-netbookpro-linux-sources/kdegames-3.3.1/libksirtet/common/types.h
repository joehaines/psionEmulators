#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include <qdatastream.h>

struct ClientPlayData { Q_UINT8 height, gift, end; };
QDataStream &operator <<(QDataStream &s, const ClientPlayData &d);
QDataStream &operator >>(QDataStream &s, ClientPlayData &d);

struct ServerPlayData { Q_UINT8 prevHeight, nextHeight, gift; };
QDataStream &operator <<(QDataStream &s, const ServerPlayData &d);
QDataStream &operator >>(QDataStream &s, ServerPlayData &d);

class ServerInitData
{
 public:
	QString  prevName, nextName, name;
	Q_UINT32 initLevel, seed;
};
QDataStream &operator <<(QDataStream &s, const ServerInitData &d);
QDataStream &operator >>(QDataStream &s, ServerInitData &d);

#endif
