#include "mp_simple_types.h"

QDataStream &operator <<(QDataStream &s, const EnumClass &ec)
  { s << (Q_UINT8)ec.f; return s; }
QDataStream &operator >>(QDataStream &s, EnumClass &ec) 
  { Q_UINT8 t; s >> t; ec.f = (int)t; return s; }
