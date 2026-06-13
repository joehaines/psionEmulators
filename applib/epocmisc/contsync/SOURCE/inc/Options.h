#ifndef OPTIONS
#define OPTIONS








#include <e32base.h>
#include <f32file.h>




#ifndef __WINS__
	_LIT(KIniFileName,"c:\\system\\data\\contsync.ini"); 
#else
	//_LIT(KIniFileName,"z:\\system\\apps\\contsync\\contsync.tst"); 
	_LIT(KIniFileName,"c:\\system\\data\\contsync.ini"); 
#endif





class COptions : public CBase
	{


public:
	typedef enum ContactDbRole{EMaster, ESlave, EThis, EOther};
	typedef enum NetworkType{ETinyTP, EDebug};
	typedef enum NetworkRole{EClient, EServer};
	
	static COptions* NewL();
	IMPORT_C void SetContactsDbRole(TInt aContactDbRole);
	IMPORT_C void SetNetworkType(TInt aNetworkType);
	IMPORT_C void SetNetworkRole(TInt aNetworkRole);
	
	IMPORT_C ContactDbRole GetContactDbRole(void);
	IMPORT_C NetworkType GetNetworkType(void);
	IMPORT_C NetworkRole GetNetworkRole(void);
	IMPORT_C void WriteIniFile(void);

private:
	void ConstructL();
	void ReadIniFile();
	void ProcessLine(TDesC8& aLine);
	


private:
	ContactDbRole iContactDbRole;
	NetworkType iNetworkType;
	NetworkRole	iNetworkRole;

	
	};



#endif