#ifndef WF_TYPES_H
#define WF_TYPES_H

#include <qdatastream.h>

class EnumClass
{
 public:
	EnumClass(int _f) : f(_f) {}
	int f;
};
QDataStream &operator <<(QDataStream &s, const EnumClass &f);
QDataStream &operator >>(QDataStream &s, EnumClass &f);

class IO_Flag : public EnumClass
{
 public:
	enum IOF { Init = 0, Play, Pause, Stop, GameOver };
	IO_Flag(IOF f = Init) : EnumClass(f) {}
	IOF value() const { return (IOF)f; }
};
	
enum ServerState { SS_Init, SS_Play, SS_Pause, SS_Stop, SS_Standby,
		           SS_PauseAsked, SS_UnpauseAsked };

class SC_Flag : public EnumClass
{
 public:
	enum SC { Stop = 0, GameOver };
	SC_Flag(SC f = Stop) : EnumClass(f) {}
	SC value() const { return (SC)f; }
};

enum BoardState { BS_Init, BS_Play, BS_Pause, BS_Stop, BS_Standby };

#endif
