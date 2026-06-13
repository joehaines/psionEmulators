
#ifndef CNWINTERFACE
#define CNWINTERFACE


#include<e32base.h>

#include "ContSyncDef.h"

#include <eikenv.h>





class CNWInterface : public CBase
	{
public:
	virtual TBool Connect() = 0;
	virtual void Read() = 0;
	virtual void Write(TDesC&) = 0;
	void Send(TDesC& aPacket);
	void Send();
	void Receive();
	TDesC& GetBuffer();
	void SetBuffer(const TDesC& aStream);
	void AppendBuffer(const TDesC& aStream);
protected:
	TBuf<KMaxContactDataSize> iBuf;
	};



#endif