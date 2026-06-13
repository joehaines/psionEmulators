



#include "options.h"






COptions* COptions::NewL()
	{
	/* function  CPptions 2 phase contructor - first phase
	 * return	 pointer the self
	 */
	COptions* self = new (ELeave) COptions();
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop();
	return self;
	}


void COptions::ConstructL()
	{
	/* function  CPptions 2 phase contructor - second phase
	 *			 sets locxally stored values to those stored 
	 *			 in the .ini file
	 */

	ReadIniFile();
	
	}


EXPORT_C void COptions::SetContactsDbRole(TInt aContactDbRole)
	{
	/* function  SetContactsDbRole sets the stores the suplied
	 *			 aConatctDbRole locally
	 */
	iContactDbRole = ContactDbRole(aContactDbRole);
	}


EXPORT_C void COptions::SetNetworkType(TInt aNetworkType)
	{
	/* function  SetNetworkType sets the stores the suplied
	 *			 aNetworkType locally
	 */
	iNetworkType = NetworkType(aNetworkType);
	}


EXPORT_C void COptions::SetNetworkRole(TInt aNetworkRole)
	{
	/* function  SetNetworkRole sets the stores the suplied
	 *			 aNetworkRole locally
	 */
	iNetworkRole = NetworkRole(aNetworkRole);
	}


EXPORT_C COptions::ContactDbRole COptions::GetContactDbRole()
	{
	/* function  GetContactDbRole
	 * return	 the locally stored contact database role
	 */
	return iContactDbRole;
	}


EXPORT_C COptions::NetworkType COptions::GetNetworkType()
	{
	/* function  GetNetworkType
	 * return	 the locally stored network interface type
	 */
	return iNetworkType;
	}


EXPORT_C COptions::NetworkRole COptions::GetNetworkRole()
	{
	/* function  GetNetworkRole
	 * return	 the locally stored network interface Role
	 */
	return iNetworkRole;
	}

void COptions::ReadIniFile(void)
	{
	/* function  ReadIniFile opens the \system\data\contsuync.ini file 
	 *			 and uses the content to set the database role and 
	 *			 the network role and type
	 */
	RFs fileServerSession;
	RFile file;
	
	fileServerSession.Connect();


	TInt err = file.Open(fileServerSession, KIniFileName, EFileRead);
	if (err < 0) 
	{
		return;
	}

	TBuf8<256> buf;
	TInt filePos = 0;
	TInt fileSize;
	file.Size(fileSize);

	while (filePos < fileSize)
		{
		
		file.Read(buf, buf.MaxLength()); 

		TInt lineSize = buf.Locate('\n');
		
		buf.Copy((TText8*)buf.Ptr(), lineSize - 1);
	
		if (buf.Length() > 6)
			ProcessLine(buf);

		// get onto next line
		filePos += (lineSize + 1);
		
		file.Seek(ESeekStart, filePos);
	
		}

	file.Close();
	fileServerSession.Close();
	}


void COptions::ProcessLine(TDesC8& aLine)
	{
	/* function  ProcessLine takes an eight bit descrioor and if it contains either 
	 *			 ContDb, NetTyp or NetRol as the first 6 characters and a digit 
	 *			 anywhere after uses this to populate the correct member variable
	 * parameter aLine is an 8 bit descriptor containing the line to be processed.
	 */

	TInt i;

	// search line for numeric value after 
	for (i = 5; i < aLine.Length() && (aLine[i] < '0' || aLine[i] > '9'); i++);

	// if no nummeric value on this line, then line is invalid and should be ignored
	if (i == aLine.Length())
		return;
	

	if (aLine.Left(6) == _L8("ContDb"))
		{
		iContactDbRole = ContactDbRole(aLine[i] - 48);
		}
	else if (aLine.Left(6) == _L8("NetTyp"))
		{
		iNetworkType = NetworkType(aLine[i] - 48);
		}
	else if (aLine.Left(6) == _L8("NetRol"))
		{
		iNetworkRole = NetworkRole(aLine[i] - 48);
		}
	}



EXPORT_C void COptions::WriteIniFile(void)
	{
	/* function  WriteIniFile writes the stored contacts database 
	 *			 role and the network role and type to the 
	 *			 \system\data\contsuync.ini file
	 */
	RFs fileServerSession;
	RFile file;
	
	TInt err = fileServerSession.Connect();

	err = fileServerSession.IsValidName(KIniFileName);

	err = file.Open(fileServerSession, KIniFileName, EFileWrite);

	if (err != KErrNone) // file does not exist - create it 
		{ 
		err=file.Create(fileServerSession, KIniFileName, EFileWrite); 
		}

	TBuf8<20> line;
	TBuf8<1> digit;

	file.Write(_L8("# ContSync. .ini file \r\n"));
	file.Write(_L8("# Warning manually editing this file could seriously damage your contacts\r\n"));
	file.Write(_L8("\r\n\r\n\r\n"));
	// write contacts db role line
	line.Copy(_L8("ContDb "));
	digit.Num((TUint)iContactDbRole);
	line.Append(digit);
	line.Append(_L("\r\n"));
	file.Write(line);

	// write Network type
	line.Copy(_L8("ContDb "));

	line.Copy(_L8("NetTyp "));

	digit.Num((TUint)iNetworkType);
	line.Append(digit);
	line.Append(_L("\r\n"));
	file.Write(line);

	// write Network role
	line.Copy(_L8("NetRol "));
	digit.Num((TUint)iNetworkRole);
	line.Append(digit);
	line.Append(_L("\r\n\r\n"));
	file.Write(line);



	

	file.Close();
	fileServerSession.Close();
	}
