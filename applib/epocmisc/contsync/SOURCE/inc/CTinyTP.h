


#ifndef CTINYTP
#define CTINYTP

/*
#if defined (__WINS__)
	#define PDD_NAME _L("ECDRV")
#else 
	#define PDD_NAME _L("EUART1")
#endif

#define LDD_NAME _L("ECOMM")
*/

#include <e32base.h>
#include <es_sock.h>
#include <ir_sock.h>

#include <eikenv.h>

#include "contsyncdef.h"
#include "NWInterface.h"

//#define KBeamPortNumber 0x01



class CTinyTP: public CNWInterface
	{
public:
	~CTinyTP();
	TInt ConnectSockServ();
	
	virtual TBool Connect() = 0;

	void Read();
	void Write(TDesC& aPacket);
	
	RSocketServ& SockServ();
	RSocket& CommunicationSock();
	
private:
	
	RSocketServ iSs;
	TBuf8<KMaxContactDataSize> iPacket;
	RSocket iCommunicationSocket;
	};



class CTinyTPClient: public CTinyTP
	{
public:
	static CTinyTPClient* NewL(void);
	~CTinyTPClient();
	TBool Connect();


private:
	void ConstructL();	
	TBool FindProtocol();
	TBool OpenListener();
	TBool Bind();
	TBool StartListener();
	TBool OpenCommunicationSocket();
	TBool ListenerAccept();
private:
	
	TProtocolDesc iPInfo;
	RSocket iListener;
	TSockAddr iSockAddress;


	};




class CTinyTPServer: public CTinyTP
	{
public:
	static CTinyTPServer* NewL(void);
	~CTinyTPServer();
	TBool Connect();
	
	
	
private:
	void ConstructL();
	TRequestStatus iStat;
	TBool FindProtocol();
	TBool OpenHostResolver();
	TBool GetByName();
	TBool OpenCommunicationSocket();
	TBool CommunicationSocketConnect();

private:
	RSocket iSock;
	TProtocolDesc iPInfo;
	RHostResolver ihr1;
	TNameEntry iLog;
	

	};


















#endif;