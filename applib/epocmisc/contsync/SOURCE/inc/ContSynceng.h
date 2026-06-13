// ContSynceng.h
//


#ifndef __CONTSYNCENG_H
#define __CONTSYNCENG_H

#include <e32base.h>


#include "Options.h"
#include "CTinyTp.h"
#include "NWDebug.h"
#include "ContactsDb.h"
#include "CPacketTranslator.h"
#include <eikenv.h>



class CContsyncEng: public CBase
	{
public:
	IMPORT_C static CContsyncEng* NewL();
	IMPORT_C ~CContsyncEng();
	IMPORT_C void ConnectL();
	IMPORT_C void OpenDatabaseL();
	IMPORT_C void SynchroniseL(CEikonEnv* aEikonEnv);
	
	IMPORT_C COptions* GetOptions(void);
	

private:
	void ConstructL();
	void SyncToL(CEikonEnv* aEikonEnv);
	void SyncFromL(CEikonEnv* aEikonEnv);

private:
	COptions* iOptions;
	CNWInterface* iNWInterface;
	CContactsDb* iContactsDb;
	CPacketTranslator* iPacketTranslator;
	
	};

#endif
