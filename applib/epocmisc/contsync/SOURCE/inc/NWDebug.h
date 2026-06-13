#ifndef NWDEBUG
#define NWDEBUG


#include "NWInterface.h"






class CNWDebug: public CNWInterface
	{
public:
	
	static CNWDebug* NewL(void);
	~CNWDebug();
	TBool Connect();
	void Write(TDesC& aPacket);
	void Read();


private:
	void ContsructL();


private:

	RFs iFileSystem;
	RFile iFile;
	TInt iFilePos;
	TBuf<KMaxContactDataSize> iBuf16;



	};



#endif