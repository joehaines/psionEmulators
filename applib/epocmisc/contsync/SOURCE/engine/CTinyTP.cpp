// CTinyTPClient  - Client Implementation




#include "CTinyTP.h"



CTinyTP::~CTinyTP()
	{
	/* CTinyTPClient destructor 
	 */

	// close the communication socket
	iCommunicationSocket.Close();
	
	// close the socket server
	iSs.Close();

	}

TInt CTinyTP::ConnectSockServ()
	{
	return iSs.Connect();
	}


RSocketServ &CTinyTP::SockServ()
	{
	return iSs;
	}

RSocket& CTinyTP::CommunicationSock()
	{
	return iCommunicationSocket;
	}


void CTinyTP::Write(TDesC& aPacket)
	{
	/* function  Write, writes the supplied descriptor to the communication socket
	 *			 effectively transmitting the data to the remote device
				 Note that use of Wait for request ensures a Synchronous send.
	 * parameter aPacket is the descriptor to be transmitted
	 */

	TRequestStatus stat;

	iPacket.Copy(aPacket);

	iCommunicationSocket.Write(iPacket,stat);
    
	User::WaitForRequest(stat);

	}// end function Write


void CTinyTP::Read()
	{
	/* function  Read, reads from the Communication Socket into the member iB
	 *			 and then converts this to the default machine descriptor type
	 *			 before returning the new descriptor
	 * return	 the machine defaulrt descrioptor type containg the received buffer
	 */
	TRequestStatus stat;

	// read from the socket into iPacket
	iCommunicationSocket.Read(iPacket,stat);
    // ensure that read is done synchronously
	User::WaitForRequest(stat);
	
	iBuf.Copy(iPacket);
	}// end function Read


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CTinyTPClient* CTinyTPClient::NewL(void){
	/* Two phase constructor - first phase
	 * return pointer to self
	 */
	CTinyTPClient* self = new (ELeave) CTinyTPClient();
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop();
	return self;
	}

void CTinyTPClient::ConstructL()
	{
	/* two phase constructor - second phase
	 * not used at present
	 */


	}


CTinyTPClient::~CTinyTPClient()
	{
	/* CTinyTPClient destructor 
	 */
	
	// close the listener socket
	iListener.Close();

	}







TBool CTinyTPClient::Connect()
	{
	/* function  Connect connects this host device to the server device
	 * return	 ETrue if successful, EFalse otherwise
	 */
	//aEikonEnv->InfoWinL(_L("SockServ"), _L(" about to connect "));
	// Connect to the socket server
	if (ConnectSockServ() != KErrNone)
		{
		return EFalse;
		}
	//aEikonEnv->InfoWinL(_L("SockServ"), _L("Connected"));
	// Find the tinyTP protocol
	if (!FindProtocol())
		{
		return EFalse;
		}
	//aEikonEnv->InfoWinL(_L("Protocol"), _L("Found"));
	// open a listener socket, used to listen for connections
	if (!OpenListener())
		{
		return EFalse;
		}
	//aEikonEnv->InfoWinL(_L("Listener"), _L("Opened"));
	// bind the listener to the IrDA port
	if (!Bind())
		{
		return EFalse;
		}
	
	// start the listener - this will allow this device to be connected to 
	// the device currently in host resolver mode - see server code
	if (!StartListener())
		{
		return EFalse;
		}
	//aEikonEnv->InfoWinL(_L("Listener"), _L("Started"));
	// open a communications socket 
	if (!OpenCommunicationSocket())
		{
		return EFalse;
		}
	//aEikonEnv->InfoWinL(_L("CommsSock"), _L("Opened"));
	// accept an incoming connection
	if (!ListenerAccept())
		{
		return EFalse;
		}
	
	return ETrue;
	}



TBool CTinyTPClient::FindProtocol()
	{
	/* function  FindProtocol instructs the socket server to find the named protocol
	 *			 storing its details in iPInfo
	 * return	 ETrue if successful, EFalse otherwise
	 */

	TBuf<32> tinyTP = _L("IrTinyTP");
	
	TInt ret;

	ret = SockServ().FindProtocol(tinyTP, iPInfo);

	if (ret == KErrNone)
		{
		return ETrue;
		}
	
	return EFalse;
	
	}
	

TBool CTinyTPClient::OpenListener()
	{
	/* function  OpenListener instruct the socket server to open a Listener socket 
	 *			 using the protocol stored in iPInfo, TinyTP
	 * return	 ETrue if successful, EFalse otherwise
	 */
	int ret = iListener.Open(SockServ(), iPInfo.iAddrFamily, iPInfo.iSockType, iPInfo.iProtocol);
	
	if (ret != KErrNone)
		{
		return EFalse;
		}

	return ETrue;
	}

TBool CTinyTPClient::Bind()
	{
	/* function  Bind, binds the listener to a local socket address, in this case 2
	 *			 indicating port 2. Port 0 & 1 are serial ports and 2 is the irda port
	 * return    Etrue if successful, EFalse otherwise
	 */
	TSockAddr a;

	TInt ret;
	
	a.SetPort(0x02); // 0x02 = IrDA port, a is now the socket address of the IrDA port
	
	ret = iListener.Bind(a);
	
	if (ret != KErrNone)
		return EFalse;

	return ETrue;
	}

TBool CTinyTPClient::StartListener()
	{
	/* function  StartListener, starts the listener listening for connections
	 * return	 ETrue if Listener successfully started, EFalse otherwise
	 */
	
	// instruct the listener to listen for connections and with a connection queue 
	// size of 1, thus only accept 1 connection at a time
	int ret = iListener.Listen(1);

	if (ret != KErrNone)
		return EFalse;

	return ETrue;
	}

TBool CTinyTPClient::OpenCommunicationSocket()
	{
	/* function  OpenCommunicationSocket, opens on socket on which communication will 
	 *			 take place once a connection has be made.
	 * return	 ETrue if successfully opened, EFalse otherwise
	 */

	int ret = CommunicationSock().Open(SockServ());

	if (ret != KErrNone)
		return EFalse;

	return ETrue;
	}//end function OpenCommunicationSocket


TBool CTinyTPClient::ListenerAccept()
	{
	/* function  ListnerAccept waits for and accepts connections from the IrDA port and pasess them
	 *			 to the communication socket so that communication may take place, freeing
	 *			 the listener for further communication (if required)
	 * return	 ETrue if successfully connected, EFalse if accept fails
	 */
	 
	TRequestStatus stat;

	iListener.Accept(CommunicationSock(), stat);

	User::WaitForRequest(stat);
	
	if (stat !=KErrNone)
		{
		return EFalse;
		}


	
	return ETrue;
	}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTinyTPServer* CTinyTPServer::NewL(void){
	/* Two phase constructor - first phase
	 * return pointer to self
	 */
	CTinyTPServer* self = new (ELeave) CTinyTPServer();
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop();
	return self;
	}

void CTinyTPServer::ConstructL()
	{
	/* two phase constructor - second phase
	 * not used at present
	 */


	}



CTinyTPServer::~CTinyTPServer()
	{
	/* Destructor
	 * closes all resources required for CTinTP
	 */
	iSock.Close();
	CommunicationSock().Close();
	}// end CTinyTPServer destructor



TBool CTinyTPServer::Connect()
	{
	/* funtcion  ConnectL, connects this deice to the remote device
	 * return	 ETrue if all connection stages succeed EFalse otherwise
	 */
	 
	// connect to the socket server
	if (ConnectSockServ() != KErrNone)
		{
		return EFalse;
		}

	
	// find the CTinyTPServer protocol
	if (!FindProtocol())
		{
		return EFalse;
		}

	
	// open a CTinyTPServer host resolver using the socket server
	// looks up the remote device
	if (!OpenHostResolver())
		{
		return EFalse;
		}

	
	// get the remote hosts name
	if (!GetByName())
		{
		return EFalse;
		}
	
	// open a Communication Socket on which to communicate
	if(!OpenCommunicationSocket())
		{
		return EFalse;
		}
	

	// connect the Communication Socket to the remote devive
	if(!CommunicationSocketConnect())
		{
		return EFalse;
		}

	
	return ETrue;
	}// end function Connect



TBool CTinyTPServer::FindProtocol()
	{
	/* function  FindProtocol calls the connected socket server to find the 
	 *			 underlying CTinyTPServer Protocol.
	 * return	 ETrue if protocol found successfully, EFalse otherwise
	 */

	// buffer containing the name of the protocol to be found
	TBuf<32> tinyTP = _L("IrTinyTP");
	
	TInt ret;

	// find the protocol named tinyTP and store its info in iPInfo
	// for later use
	ret = SockServ().FindProtocol(tinyTP, iPInfo);

	if (ret == KErrNone)
		{
		return ETrue;
		}
	else
		{
		return EFalse;
		}


	}// end function FindProtocol

TBool CTinyTPServer::OpenHostResolver()
	{
	/* function  OpenHostResolver opens a host resolver, used to find the remote device
	 * return	 ETrue if successfull, EFalse otherwise
	 */

	TInt ret;
    
	// open a host resolver using the IPInfo address family and protocol
	// i.e. TinyTp
	ret=ihr1.Open(SockServ(),iPInfo.iAddrFamily,iPInfo.iProtocol);
    
	if (ret !=KErrNone)
		{
		return EFalse;
		}
	
	return ETrue;
    }// end function OpenHostResolver


TBool CTinyTPServer::GetByName()
	{
	/* function   GetByName, uses the now open host resolver to look up any device
	 *			  current trying to communicate over the chosen protocol, i.e. TinyTP
	 *			  Connection details stored in iLog, name stored locally and discarded
	 *			  as not required.
	 * return	  ETrue if remote device found, EFalse otherwise.
	 */

	THostName name;
	
	int ret=ihr1.GetByName(name,iLog);

    if (ret!=KErrNone)
          {
          // No devices discovered - may be none present
		  return EFalse;
          }
	
	return ETrue;
	}// end function GetByName


TBool CTinyTPServer::OpenCommunicationSocket()
	{
	/* function   OpenCommunicationSocket opens a socket on which this device can talk to the remote device
	 *			  using the socket server and iPinfo
	 * return	  ETrue if remote device found, EFalse otherwise.
	 */
	int ret = CommunicationSock().Open(SockServ(),iPInfo.iAddrFamily,iPInfo.iSockType,iPInfo.iProtocol);
	if (ret!=KErrNone)
          {
          // No devices discovered - may be none present
		  return EFalse;
          }

    return ETrue;
	}// end function OpenCommunicationSocket


TBool CTinyTPServer::CommunicationSocketConnect()
	{
	/* function   CCommunicationSocketConnect, connects the communication socket to the remote device
	 *			  ready for communication, using the remote devices information the host 
	 *			  resolver stored in iLog
	 * return	  ETrue if remote device found, EFalse otherwise.
	 */

	iLog().iAddr.SetPort(0x02);
    
    CommunicationSock().Connect(iLog().iAddr,iStat);
    
	User::WaitForRequest(iStat);
	
	if (iStat != KErrNone)
		{
		return EFalse;
		}

	return ETrue;
    }//end function CommunicationSocketConnect


