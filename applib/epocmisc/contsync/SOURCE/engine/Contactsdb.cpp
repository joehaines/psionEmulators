// contactsdb.cpp
//


#include "contactsdb.h"





CContactsDb::~CContactsDb()
	{
	/* destructor - ensures that all memory is cleaned up
	 *
	 */
	
	delete iContactIter;
	delete iContactIdArray;
	delete iContactItem;
	delete iContactItemFieldDef;
	delete iContactItemFieldSet;
	delete iContactItemField;
	delete iCContactDatabase;

	// destroy the lookup table freeing up all allocated memory
	iLookupTable.ResetAndDestroy();
	}

void CContactsDb::ConstructL()
	{
	/* two phase construction - second phase ensures that local variable are in correct state
	 */

	iFieldCount = 0;
	iContactCount = 0;
	iLastContactId = NULL;
	}

CContactTemplate* CContactsDb::GetTemplateL()
	{
	/* function GetTamplateL gets the contacts database's default contact card template
	 *			this creates memory and gives ownership to the caller
	 * return	pointer to the contact card template
	 */

	TContactItemId templateItemId = iCContactDatabase->TemplateId();
	CContactTemplate* ct = (CContactTemplate*)iCContactDatabase->OpenContactL(templateItemId); 
	iCContactDatabase->CloseContactL(templateItemId);
	return ct;
	}

void CContactsDb::EnterContactL(CContactItem& aContactItem)
	{
	/* function  EnterContactL adds a new contact to the contact database
	 *			 also creates memory, which the contacts database takes ownership of
	 * parameter aContactItem is a pointer to the contact item to be added to the database
	 */

	iCContactDatabase->AddNewContactL(aContactItem);
	}

TBool CContactsDb::OpenDbL()
	{
	/* function  OpenDbL opens the default contact database
	 *           creates memory in the form of a CContactDatabase which is held locally
	 * return	 ETrue if contact was successfully added, EFalse otherwise
	 */

	iCContactDatabase = CContactDatabase::OpenL();
	
	if (iCContactDatabase == NULL)
		{
		return EFalse;
		}

	return ETrue;
	}
 
TBool CContactsDb::CreateIterL()
	{
	/* function  CreateIterL creates an iterator which can be used to step through the 
	 *			 database.This creates memory which is also stored locally and thus must 
	 *			 be deleted in the destructor
	 * return	 ETrue if iterator was successfully created, EFalse otherwise
	 */
	iContactIter = new (ELeave) TContactIter(*iCContactDatabase);

	if (iContactIter == NULL)
		{
		return EFalse;
		}
	return ETrue;
	}


TBool CContactsDb::IterFirstL()
	{
	/* function  IterFirstL positions the iterator on the first contact card in the database
	 *           and stores its position in iContactId
	 * return	 ETrue if first contact card found, EFalse otherwise
	 */
	iContactId = 0;
	iContactId = iContactIter->FirstL();
	
	if (iContactId == 0)
		{
		return EFalse;
		}

	
	while(OpenCurrentContactL() &&iContactItem->Type() != KUidContactCard)
		{
		CloseCurrentContactL();
		IterNextL();
		}

	CloseCurrentContactL();
		
	return ETrue;
	}


TBool CContactsDb::IterNextL()
	{
	/* function  IterNextL positions the iterator on the next contact card in the database
	 *           and stores its position in iContactId
	 * return	 ETrue if next contact card found, EFalse if no more contacts
	 */

	iLastContactId = -1;
	iContactId = 0;
	iContactId = iContactIter->NextL();

	if (iContactId == 0)
		{
		return EFalse;
		}
	return ETrue;
	}

TBool CContactsDb::OpenCurrentContactL()
	{
	/* function  OpenCurrentContactL opens the contact within the contact database indicated 
	 *		 	 by the position stored in the local iContactId and stores it in the local
	 *			 iContactItem. If a previous contact is already opened it is first closed to 
	 *			 prevent memory leaks.
	 * return	 ETrue if the contact was successfully opened, EFalse otherwise
	 */

	if (iLastContactId != -1)
			iCContactDatabase->CloseContactL(iLastContactId);

	TRAPD(err, iContactItem = iCContactDatabase->OpenContactL(iContactId));
	if (err != KErrNone)
		return EFalse;

	// prevents loosing contacts in memory when IterNextL Is called before CloseContactL
	iLastContactId = iContactId;

	return ETrue;
	}

void CContactsDb::CloseCurrentContactL()
	{
	/* function	 CloseCurrentContactL closes the contact last opened with OpenContactL and 
	 *			 and frees any memory assigned to that contact
	 */

	iCContactDatabase->CloseContactL(iLastContactId);
	delete iContactItem;
	iContactItem = 0;
	
	iLastContactId = NULL;
	}

CContactItem* CContactsDb::GetCurrentContact()
	{
	/* function  GetCurrentContactL 
	 * return	 a pointer to the contact opened with OpenContactL
	 */
	return iContactItem;
	}


void CContactsDb::CreateContactSetL(/*CEikonEnv* aEikonEnv*/)
	{
	/* function	 CreateContactSetL creates a look up table of the contacts in the contact database
	 *			 consisting of their name, surname, company name. This provides a quick means of 
	 *			 finding a contact within the contacts database without opening all the cards for 
	 *			 every new contact to be found.
	 */

	IterFirstL();
	
	TBuf<40> firstName;
	TBuf<40> surname;
	TBuf<80> companyName;
	

	if (OpenCurrentContactL())
		{

		do{
			// not every contact item is a contact card, and there is no
			if (iContactItem->Type() == KUidContactCard)
				{

				// firstNameNum is the index of the First Name field within iContactItem's field set
				TInt firstNameNum = iContactItem->CardFields().Find(KUidContactFieldGivenName);
				// surnameNum is the index of the Surname field within iContactItem's field set
				TInt surnameNum = iContactItem->CardFields().Find(KUidContactFieldFamilyName);
				// companyNum is the index of the ComanyName within iContactItem's field set
				TInt companyNum = iContactItem->CardFields().Find(KUidContactFieldCompanyName);
			
				if (firstNameNum > -1)
					{
					firstName.Copy(iContactItem->CardFields()[firstNameNum].TextStorage()->Text());
					}
				else
					{
					firstName.Copy(_L(""));
					}
	
				if (surnameNum > -1 && iContactItem->CardFields()[surnameNum].TextStorage()->Text().Length() > 1)
					{
					surname.Copy(iContactItem->CardFields()[surnameNum].TextStorage()->Text());
					}
				else
					{
					surname.Copy(_L(""));
					}

				if (companyNum > -1)
					{
					companyName.Copy(iContactItem->CardFields()[companyNum].TextStorage()->Text());
					}
				else
					{
					companyName.Copy(_L(""));
					}
	
				// append a lookup reference to the loookup table
				Lookup* lookup = new Lookup(firstName, surname, companyName, iContactItem->Id());
				iLookupTable.Append(lookup);

				}
		
			CloseCurrentContactL();	

			
		// while there are more contacts in the database
		}while (IterNextL() && OpenCurrentContactL());
		// if out of loop no contact to close
	
		}
	}

TContactItemId  CContactsDb::ContactExists(CContactItem& aContactItem)
	{
	/* function  ContactExists uses the lookup table provided by CreateContactSet
	 *			 to see is the supplied contact already exists in the contact database
	 * parameter aContactItem is the contact item we wish to lookup
	 * return	 a TContactItemId indcating the contacts position in the database
	 */
	
	// get the position of the name surname and company name within the supplied contact
	TInt firstNameNum = aContactItem.CardFields().Find(KUidContactFieldGivenName);
	TInt surnameNum = aContactItem.CardFields().Find(KUidContactFieldFamilyName);
	TInt companyNum = aContactItem.CardFields().Find(KUidContactFieldCompanyName);

	TBuf<40> firstName;
	TBuf<40> surname;
	TBuf<80> companyName;

	// craete a local copy, for reader clarity
	firstName.Copy(aContactItem.CardFields()[firstNameNum].TextStorage()->Text());
	surname.Copy(aContactItem.CardFields()[surnameNum].TextStorage()->Text());
	companyName.Copy(aContactItem.CardFields()[companyNum].TextStorage()->Text());

	

	// search through the lookup table and if found, return the Id of that contact
	for (int i = 0; i < iLookupTable.Count(); i++)
		{
		if (iLookupTable[i]->FirstName() == firstName && iLookupTable[i]->Surname() == surname)
			return iLookupTable[i]->Id();
		}

	// Note company name not used at present, but this may provide extra search options later

	// contact not found return -1; ie must add contact to the database
	return -1;
	}
CContactDatabase*  CContactsDb::ContactDatabase()
	{

	return iCContactDatabase;
	}




CContactsDb::Lookup::Lookup(const TDesC& aFirstName, const TDesC& aSurname, const TDesC& aCompany, TContactItemId aId)
	{
	// single stage constructor
	// initialises all the fields to those supplied allocating memory for each
	iFirstName = aFirstName.Alloc();
	iSurname = aSurname.Alloc();
	iCompany = aCompany.Alloc();
	iId = aId;
	}

CContactsDb::Lookup::~Lookup()
	{
	// delete the stored fields 
	delete iCompany;
	delete iFirstName;
	delete iSurname;
	
	}

TDesC& CContactsDb::Lookup::FirstName()
	{
	/* function FirstName returns the first name field
	 */
	
	return *iFirstName;
	}

TDesC& CContactsDb::Lookup::Surname()
	{
	/* function Surname returns the surname field
	 */

	return *iSurname;
	}

TDesC& CContactsDb::Lookup::Company()
	{
	/* function Company returns the company name field
	 */

	return *iCompany;
	}

TContactItemId CContactsDb::Lookup::Id()
	{
	/* function Id returned the Id field
	*/
	return iId;
	}


CPsionContactsDb* CPsionContactsDb::NewL()
	{
	/* two phase construction - first phase
	 * return	 Pointer to self
	 */

	CPsionContactsDb* self = new (ELeave) CPsionContactsDb;
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop();
	return self;
	}



void CPsionContactsDb::UpdateContactL(TContactItemId aId, CContactItem& aContactItem)
	{
	/* function  UpadteContactL upadtes a CContactCard with in the contacts database
	 *			 with the fiedls supplied by a new ContactCard
	 * parameter aId is the id for the ContactCard in the database
	 * parameter aContactItem if the Contact card holding the new fields which must be 
	 *           use to update those in the database.
	 */

	// Assume that new contact has data to be added to old and that any existing data
	// is outdated by the new data
	CContactItem* existingContactItem = ContactDatabase()->OpenContactL(aId);
	CleanupStack::PushL(existingContactItem);
	
	TInt fieldCount = 0, maxCount = aContactItem.CardFields().Count();
	

	while (fieldCount < maxCount)
		{
		
	
		/*
		Psion contact added manually
		#	Label			Uid			Uid 1		Uid 2 
		0	Title			1000178c	0			0			0	0
		1	First name		1000137c	0			0			0	0
		2	Middle name		1000178a	0			0			0	0
		3	Last name		1000137d	0			0			0	0
		4	Suffix			1000178b	0			0			0	0
		5	Mobile			1000130e	100039db	10003e71	0	0
		6	Home tel		1000130e	100039db	0			0	0
		7	Home fax		10001791	100039db	100039de	0	0
		8	Pager			1000130e	10003e72	100039db	0	0
		9	Home email		1000178e	100039db	0			0	0
		10	Home address	1000130c	100039db	0			0	0
		11	Company			1000130d	0			0			0	0
		12	Job title		1000130d	0			0			0	0
		13	Work mobile		1000130e	10003e71	100039da	0	0
		14	Work tel		1000130e	100039da	0			0	0
		15	Work fax		10001791	100039da	100039de	0	0
		16	Work pager		1000130e	10003e72	100039da	0	0
		17	Work email		1000178e	100039da	0			0	0
		18	Web page		10004035	0			0			0	0
		19 	Work address	1000130c	100039da	0			0	0
		20	Birthday		10004034	0			0			0	0
		21	Notes			1000401c	0			0			0	0
		22	Display name	1000401c	0			0			0	0

		*/
		
		/*
		a PsiWin Added contact :(
		0	Title			1000178c	
		1	First name		1000137c
		2	Middle name		1000178a
		3	Last name		1000137d
		4	Suffix			1000178b
		5	Mobile			1000130e	100039db	10003e71
		6	Home tel		1000130e	100039db
		7	Home fax		10001791	100039db	100039de
		8	Pager			1000130e	10003e72	100039db
		9	Home email		1000178e	100039db	
		10	Home PO Box		10004df4	100039db	
		11	Home ext Addr	10004df5	100039db	
		12	Home address	10004df4	100039db	
		13	Home city		10004df4	100039db	
		14	Home regiom		10004df4	100039db
		15	Home p'code		10004df4	100039db
		16	Home country	10004df4	100039db
		17	Company			1000130d	
		18	Job Title		
		19	Work Mobile		1000130e	10003e71	100039da
		20	Work tel		1000130e	100039da	
		21	Work fax		10001791	100039da
		22	Work pager		1000130e	10003e72	100039da
		23	Work email		1000178e	100039db	100039da
		24	Work email		1000178e	100039da
		25	Web page		10004035	
		26	Work PO Box		10004df4	100039da	
		27	Work ext Addr	10004df5	100039da
		28	Work address	10004df4	100039da
		29	Work city		10004df4	100039da
		30	Work region		10004df4	100039da	
		31  Work p'code		10004df8	100039da
		32	Work Country	10004df4	100039da
		33	Birthday		10004034
		*/

		



		TPtrC label = aContactItem.CardFields()[fieldCount].Label();
		TInt pos;

		if (label.Length() > 0)
			{

			for (pos = 0; pos < maxCount && existingContactItem->CardFields()[pos].Label() != label; pos ++);

			if (pos != maxCount && pos > -1)
				{

				
				if (existingContactItem->CardFields()[pos].StorageType() == KStorageTypeText)
					{
					if (aContactItem.CardFields()[fieldCount].TextStorage()->Text() != existingContactItem->CardFields()[pos].TextStorage()->Text())
						{	
						TBuf<256> buf;
						buf.Copy(aContactItem.CardFields()[fieldCount].TextStorage()->Text());
						existingContactItem->CardFields()[pos].TextStorage()->SetTextL(buf);
						existingContactItem->CardFields()[pos].SetHidden(EFalse);
						existingContactItem->CardFields()[pos].SetDisabled(EFalse);
						}// 
					}// text type
				else if (existingContactItem->CardFields()[pos].StorageType() == KStorageTypeDateTime)
					{
					if (aContactItem.CardFields()[fieldCount].DateTimeStorage()->Time() != existingContactItem->CardFields()[pos].DateTimeStorage()->Time())
						{	
						TTime dateTime = aContactItem.CardFields()[fieldCount].DateTimeStorage()->Time();
						existingContactItem->CardFields()[pos].DateTimeStorage()->SetTime(dateTime);
						}//
					}// end if date/time storage
				}// end if pos > -1	
			}// end if label length > 0, useful field

		fieldCount++;
		
		}// end while loop
	
	// commit the contact to the contacts database
	ContactDatabase()->CommitContactL(*existingContactItem);

	CleanupStack::PopAndDestroy();// existingContactItem
	
	}

CCommunicatorContactsDb* CCommunicatorContactsDb::NewL()
	{
	/* two phase construction - first phase
	 * return	 Pointer to self
	 */

	CCommunicatorContactsDb* self = new (ELeave) CCommunicatorContactsDb;
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop();
	return self;
	}

void CCommunicatorContactsDb::UpdateContactL(TContactItemId aId, CContactItem& aContactItem)
	{
	/* function  UpadteContactL upadtes a CContactCard with in the contacts database
	 *			 with the fiedls supplied by a new ContactCard
	 * parameter aId is the id for the ContactCard in the database
	 * parameter aContactItem if the Contact card holding the new fields which must be 
	 *           use to update those in the database.
	 */

	// Assume that new contact has data to be added to old and that any existing data
	// is outdated by the new data
	CContactItem* existingContactItem = ContactDatabase()->OpenContactL(aId);
	CleanupStack::PushL(existingContactItem);
	
	TInt fieldCount = 0, maxCount = aContactItem.CardFields().Count();
	

	while (fieldCount < maxCount)
		{
		
	
		/*
		Psion contact added manually
		#	Label			Uid			Uid 1		Uid 2 
		0	Title			1000178c	0			0			0	0
		1	First name		1000137c	0			0			0	0
		2	Middle name		1000178a	0			0			0	0
		3	Last name		1000137d	0			0			0	0
		4	Suffix			1000178b	0			0			0	0
		5	Mobile			1000130e	100039db	10003e71	0	0
		6	Home tel		1000130e	100039db	0			0	0
		7	Home fax		10001791	100039db	100039de	0	0
		8	Pager			1000130e	10003e72	100039db	0	0
		9	Home email		1000178e	100039db	0			0	0
		10	Home address	1000130c	100039db	0			0	0
		11	Company			1000130d	0			0			0	0
		12	Job title		1000130d	0			0			0	0
		13	Work mobile		1000130e	10003e71	100039da	0	0
		14	Work tel		1000130e	100039da	0			0	0
		15	Work fax		10001791	100039da	100039de	0	0
		16	Work pager		1000130e	10003e72	100039da	0	0
		17	Work email		1000178e	100039da	0			0	0
		18	Web page		10004035	0			0			0	0
		19 	Work address	1000130c	100039da	0			0	0
		20	Birthday		10004034	0			0			0	0
		21	Notes			1000401c	0			0			0	0
		22	Display name	1000401c	0			0			0	0

		*/
		
		/*
		a PsiWin Added contact :(
		0	Title			1000178c	
		1	First name		1000137c
		2	Middle name		1000178a
		3	Last name		1000137d
		4	Suffix			1000178b
		5	Mobile			1000130e	100039db	10003e71
		6	Home tel		1000130e	100039db
		7	Home fax		10001791	100039db	100039de
		8	Pager			1000130e	10003e72	100039db
		9	Home email		1000178e	100039db	
		10	Home PO Box		10004df4	100039db	
		11	Home ext Addr	10004df5	100039db	
		12	Home address	10004df4	100039db	
		13	Home city		10004df4	100039db	
		14	Home regiom		10004df4	100039db
		15	Home p'code		10004df4	100039db
		16	Home country	10004df4	100039db
		17	Company			1000130d	
		18	Job Title		
		19	Work Mobile		1000130e	10003e71	100039da
		20	Work tel		1000130e	100039da	
		21	Work fax		10001791	100039da
		22	Work pager		1000130e	10003e72	100039da
		23	Work email		1000178e	100039db	100039da
		24	Work email		1000178e	100039da
		25	Web page		10004035	
		26	Work PO Box		10004df4	100039da	
		27	Work ext Addr	10004df5	100039da
		28	Work address	10004df4	100039da
		29	Work city		10004df4	100039da
		30	Work region		10004df4	100039da	
		31  Work p'code		10004df8	100039da
		32	Work Country	10004df4	100039da
		33	Birthday		10004034
		*/

		



		TPtrC label = aContactItem.CardFields()[fieldCount].Label();
		TInt pos;

		if (label.Length() > 0)
			{

			for (pos = 0; pos < maxCount && existingContactItem->CardFields()[pos].Label() != label; pos ++);

			if (pos != maxCount && pos > -1)
				{

				
				if (existingContactItem->CardFields()[pos].StorageType() == KStorageTypeText)
					{
					if (aContactItem.CardFields()[fieldCount].TextStorage()->Text() != existingContactItem->CardFields()[pos].TextStorage()->Text())
						{	
						TBuf<256> buf;
						buf.Copy(aContactItem.CardFields()[fieldCount].TextStorage()->Text());
						existingContactItem->CardFields()[pos].TextStorage()->SetTextL(buf);
						existingContactItem->CardFields()[pos].SetHidden(EFalse);
						existingContactItem->CardFields()[pos].SetDisabled(EFalse);
						}// 
					}// text type
				else if (existingContactItem->CardFields()[pos].StorageType() == KStorageTypeDateTime)
					{
					if (aContactItem.CardFields()[fieldCount].DateTimeStorage()->Time() != existingContactItem->CardFields()[pos].DateTimeStorage()->Time())
						{	
						TTime dateTime = aContactItem.CardFields()[fieldCount].DateTimeStorage()->Time();
						existingContactItem->CardFields()[pos].DateTimeStorage()->SetTime(dateTime);
						}//
					}// end if date/time storage
				}// end if pos > -1	
			}// end if label length > 0, useful field

		fieldCount++;
		
		}// end while loop
	
	// commit the contact to the contacts database
	ContactDatabase()->CommitContactL(*existingContactItem);

	CleanupStack::PopAndDestroy();// existingContactItem
	
	}


