#include "NWDebug.h"




CNWDebug::~CNWDebug()
	{
	iFile.Close();
	iFileSystem.Close();
	}



CNWDebug* CNWDebug::NewL(void)
	{
	CNWDebug* self = new(ELeave) CNWDebug;
	CleanupStack::PushL(self);
	self->ContsructL();
	CleanupStack::Pop();
	return self;
	}



void CNWDebug::ContsructL()
	{
	
	}



TBool CNWDebug::Connect()
	{
	/* function  Connect calls the network implementation to connect
	 *			 to the remote device, or in debug mode opens the file 
	 *			 to read from and write to
	 */

	iFileSystem.Connect();

#ifdef __WINS__

	_LIT(fileName, "c:\\input.txt");
#else
#ifdef COMMUNICATOR
	_LIT(fileName, "d:\\input.txt");
#else 
	_LIT(fileName, "c:\\input.txt");
#endif
#endif

	TInt err=iFile.Open(iFileSystem, fileName, EFileWrite); 
	
	if (err==KErrNotFound) // file does not exist - create it 
		{ 
		err=iFile.Create(iFileSystem, fileName, EFileWrite); 
		}

	iFilePos = 0;
	
	return err == KErrNone;


	}// end function Connect 



void CNWDebug::Write(TDesC& aPacket)
	{
	/* function	 Send sends the descriptor aPacket over the network implementation
	 *			 or in debug mode writes it to the file
	 */

	// ensure that the file is written in 8 bit mode
	TBuf8<1000> buf;
	buf.Copy(aPacket);
	
	iFile.Write(buf);
	iFile.Write(_L8("\r\n"));

	}//end function send(TDesC&)
	


void CNWDebug::Read()
	{
	/* function  Receive, receives a packet from the network
	 *			 or in debug mode reads it from the file
	 */
	int a;
	if (iFilePos == iFile.Size(a))
		{
		iFilePos = 0;
		iFile.Seek(ESeekStart, iFilePos);
		
		}

	TBuf8<KMaxContactDataSize> buf;

	// fill buffer
	iFile.Read(buf, buf.MaxLength());
	
	TInt contactDataSize = buf.Locate('\n');
	
	buf.SetLength(contactDataSize - 1);
	iBuf.Copy(buf);
	
	// get onto next line
	iFilePos += (contactDataSize + 1);
	
	iFile.Seek(ESeekStart, iFilePos);

	}// end function Receive


