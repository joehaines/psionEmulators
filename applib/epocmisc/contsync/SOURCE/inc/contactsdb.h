// contactsdbdll.h
//



#ifndef CONTACTSDBDLL
#define CONTACTSDBDLL




#include <e32std.h>
#include <e32base.h>
#include <cntdb.h>
#include <cntitem.h>
#include <cntfldst.h>

#include <eikenv.h>

class CContactsDb: public CBase
	{
public:
	
	virtual void UpdateContactL(TContactItemId aId, CContactItem& aContactItem) = 0;
	~CContactsDb();
	TBool OpenDbL();
	TBool CreateIterL();
	TBool IterFirstL();
	TBool IterNextL();
	CContactItem* GetCurrentContact();
	TBool OpenCurrentContactL();
	void CloseCurrentContactL();
	void CreateContactSetL();
	CContactTemplate* GetTemplateL();
	TContactItemId ContactExists(CContactItem& aContactItem);
	void EnterContactL(CContactItem& aContactItem);

protected:
	CContactDatabase* ContactDatabase();
	void ConstructL();

	
private:
	
	class Lookup:public CBase
		{
	public:
		Lookup(const TDesC& aFirstName, const TDesC& aSurname, const TDesC& aCompany, TContactItemId aId);
		~Lookup();
		TDesC& FirstName();
		TDesC& Surname();
		TDesC& Company();
		TContactItemId Id();
	private:
		HBufC* iFirstName;
		HBufC* iSurname;
		HBufC* iCompany;
		TContactItemId iId;
		};

private:
	TInt iFieldCount;
	TInt iContactCount;
	CContactDatabase* iCContactDatabase;
	TContactIter* iContactIter;
	CContactIdArray* iContactIdArray;
	TContactItemId iContactId, iLastContactId;
	CContactItem* iContactItem;
	CContactItemFieldDef* iContactItemFieldDef;
	CContactItemFieldSet* iContactItemFieldSet;
	CContactItemField* iContactItemField;
	RPointerArray<Lookup> iLookupTable;

	};

class CPsionContactsDb: public CContactsDb
	{
public:
	static CPsionContactsDb* NewL();
	void UpdateContactL(TContactItemId aId, CContactItem& aContactItem);
	};

class CCommunicatorContactsDb: public CContactsDb
	{
public:
	static CCommunicatorContactsDb* NewL();
	void UpdateContactL(TContactItemId aId, CContactItem& aContactItem);
	};


#endif