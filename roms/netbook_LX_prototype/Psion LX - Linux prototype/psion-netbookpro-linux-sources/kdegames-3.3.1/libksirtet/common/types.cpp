#include "types.h"

QDataStream &operator <<(QDataStream &s, const ClientPlayData &d)
  { s << d.height << d.gift << d.end; return s; }
QDataStream &operator >>(QDataStream &s, ClientPlayData &d)
  { s >> d.height >> d.gift >> d.end; return s; }

QDataStream &operator <<(QDataStream &s, const ServerPlayData &d)
  { s << d.prevHeight << d.nextHeight << d.gift; return s; }
QDataStream &operator >>(QDataStream &s, ServerPlayData &d)
  { s >> d.prevHeight >> d.nextHeight >> d.gift; return s; }

QDataStream &operator <<(QDataStream &s, const ServerInitData &d)
{ s << d.initLevel << d.seed << d.nextName << d.prevName << d.name; return s; }
QDataStream &operator >>(QDataStream &s, ServerInitData &d)
{ s >> d.initLevel >> d.seed >> d.nextName >> d.prevName >> d.name; return s; }
