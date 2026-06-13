// ContSynceng.cpp
//


#include "ContSynceng.h"




EXPORT_C CContsyncEng* CContsyncEng::NewL()
	{
	/* two phase construction - first phase
	 * return	 Pointer to self
	 */
	CContsyncEng* self = new (ELeave) CContsyncEng;
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop();
	return self;
	}// end function CContsyncEng::NewL

void CContsyncEng::ConstructL()
	{
	/* two phase construction - second phase constructs all used objects
	 */
	
	
	iOptions = COptions::NewL();

	

	
	}// end function ConstructL


EXPORT_C CContsyncEng::~CContsyncEng()
	{
	/* destructor - ensures that all memory is cleaned up properly
	 *
	 */
	
	delete iNWInterface;
	delete iContactsDb;
	delete iPacketTranslator;
	delete iOptions;
	}// end function ~CContsyncEng



EXPORT_C COptions* CContsyncEng::GetOptions(void)
	{
	/* function GetOptions, givess a handle to the opions 
	 *			to enable options update
	 * return	pointer to the local iOPtions
	 */
	return iOptions;
	
	}

EXPORT_C void CContsyncEng::ConnectL()
	{
	/* function  Connect, calls the Network Interface to connect to
	 *			 the remote device.
	 */

	if (iNWInterface != 0)
		delete iNWInterface;

	
	if (iOptions->GetNetworkType() == COptions::ETinyTP)
		{
		if (iOptions->GetNetworkRole() == COptions::EClient)
			{
			iNWInterface = CTinyTPClient::NewL();
			}
		else if (iOptions->GetNetworkRole() == COptions::EServer)
			{
			iNWInterface = CTinyTPServer::NewL();
			}
		}
	else if (iOptions->GetNetworkType() == COptions::EDebug)
		{
		iNWInterface = CNWDebug::NewL();
		}

	
	TVersion version = User::Version();
	TVersionName versionName = version.Name();
	// Version name 1.02(166) = Psion
	if (versionName == _L("1.02(166)"))
		{
		iContactsDb = CPsionContactsDb::NewL();
		iPacketTranslator = CPsionPacketTranslator::NewL(iNWInterface);
		}
	else 
		{
		iContactsDb = CCommunicatorContactsDb::NewL();
		iPacketTranslator = CCommunicatorPacketTranslator::NewL(iNWInterface);
		}


	
	if (iNWInterface->Connect() == EFalse)
		{
		User::Leave(KErrCouldNotConnect);
		}
	
	

	}// end function Connect



EXPORT_C void CContsyncEng::OpenDatabaseL()
	{
	/* function  OpenDatabase,calls the ContactsDb to open the contacts
	 *			 database
	 */
	iContactsDb->OpenDbL();
	}// end function OpenDatabase


EXPORT_C void CContsyncEng::SynchroniseL(CEikonEnv* aEikonEnv)
	{
	/* function  Synchronise, synchronises the contacts by first receiving 
	 *			 a contact set and then transmitting a contact set.
	 */

	
	iContactsDb->CreateIterL();
	switch(iOptions->GetContactDbRole())
		{
		case COptions::EMaster :
			{
			SyncToL(aEikonEnv);
			SyncFromL(aEikonEnv);
			break;
			}
		case COptions::ESlave :
			{
			SyncFromL(aEikonEnv);
			SyncToL(aEikonEnv);
			break;
			}
		case COptions::EThis :
			{
			SyncToL(aEikonEnv);
			break;
			}
		case COptions::EOther :
			{
			SyncFromL(aEikonEnv);
			break;
			}
		};
		

	}// end function Synchronise

void CContsyncEng::SyncFromL(CEikonEnv* aEikonEnv)
	{
	/* function  SyncFrom receives a set of contacts from the network interface
	 *			 and updates the contacts database one at a time
	 */

	TBuf<10> temp = _L("sync");
	
	// cretes a contact lookuip table for CContactsDb::ContactExists
	iContactsDb->CreateContactSetL();
	

#ifndef __WINS__ 
	iNWInterface->Send(temp);
#endif	
	
	iNWInterface->Receive();
	
	// create contact template for use when creating contact cards
	CContactTemplate *contactTemplate = iContactsDb->GetTemplateL();
	CleanupStack::PushL(contactTemplate);
	
	
	while (iNWInterface->GetBuffer() != _L("end"))
		{
	
		aEikonEnv->InfoMsg(iNWInterface->GetBuffer());

		if (iNWInterface->GetBuffer() != _L("sync")) 
		{
		
			CContactCard *contactCard = CContactCard::NewLC(contactTemplate);
			
			iPacketTranslator->DecodeL(contactCard);

			TContactItemId id = iContactsDb->ContactExists(*contactCard);
																																										  
			// if contact does not exist id =  -1
			if (id == -1)
				{
				// enter the contact
				iContactsDb->EnterContactL(*contactCard);
				
				}
			else
				{
				// if contact does exist, then must update the contact.
				iContactsDb->UpdateContactL(id, *contactCard);
				}
			
			CleanupStack::Pop();// ContactCard, now owned by contacts Db
			delete contactCard; 
			contactCard=0;

		}

		User::After(1);

		// Instruct Network Interface to request new packet(Contact)
		iNWInterface->Receive();
			
		}// end while network buffer not "end"

	CleanupStack::PopAndDestroy(); // contact template
	
	}// end function SyncFrom

void CContsyncEng::SyncToL(CEikonEnv* aEikonEnv)
	{
	/* function  SyncTo, sends the contact set over the network interface 
	 */
	
#ifndef __WINS__
	iNWInterface->Receive(); //sync
#else
	iNWInterface->SetBuffer(_L("sync"));
#endif
	
	aEikonEnv->InfoMsg(iNWInterface->GetBuffer());

	if (iNWInterface->GetBuffer() == _L("sync"))
	{
		TBool moreContacts = ETrue;
		
		aEikonEnv->InfoMsg(_L("about to iter first"));

		iContactsDb->IterFirstL();
		aEikonEnv->InfoMsg(_L("iter firsted"));
		
		// while there are more contact and able to open a contact
		while (moreContacts && iContactsDb->OpenCurrentContactL())
			{
				
				iPacketTranslator->EncodeL(iContactsDb->GetCurrentContact());
				
				
				iContactsDb->CloseCurrentContactL();
				
				aEikonEnv->InfoMsg(iNWInterface->GetBuffer());
				// send Network buffer populated by the encode command
				
				iNWInterface->Send();
				
				moreContacts = iContactsDb->IterNextL();	
			}// end while more contacts to encode
		
		}// end if receive sync command
		
	
	iNWInterface->SetBuffer(_L("end"));
	
	iNWInterface->Send();
	
	}// end funcion SyncTo







// requirement for E32 DLLs
EXPORT_C TInt E32Dll(TDllReason)
	{
	return 0;
	}
