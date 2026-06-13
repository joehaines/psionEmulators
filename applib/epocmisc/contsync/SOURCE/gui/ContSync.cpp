// ContSync.cpp
//


#include "ContSync.h"



//
// EXPORTed functions
//

EXPORT_C CApaApplication* NewApplication()
	{
	return new CContsyncApplication;
	}

GLDEF_C TInt E32Dll(TDllReason)
	{
	return KErrNone;
	}


////////////////////////////////////////////////////////////////
//
// Application class, CContsyncApplication
//
////////////////////////////////////////////////////////////////

TUid CContsyncApplication::AppDllUid() const
	{
	return KUidContsync;
	}

CApaDocument* CContsyncApplication::CreateDocumentL()
	{
	// Construct the document using its NewL() function, rather 
	// than using new(ELeave), because it requires two-phase
	// construction.
	return CContsyncDocument::NewL(*this);
	}


////////////////////////////////////////////////////////////////
//
// Document class, CContsyncDocument
//
////////////////////////////////////////////////////////////////

// C++ constructor
CContsyncDocument::CContsyncDocument(CEikApplication& aApp)
		: CEikDocument(aApp)
	{
	}

// The document requires two-phase construction because it
// owns the model (CContsyncEng) and therefore has to allocate 
// memory for it.
CContsyncDocument* CContsyncDocument::NewL(CEikApplication& aApp)
	{
	CContsyncDocument* self = new (ELeave) CContsyncDocument(aApp);
	CleanupStack::PushL(self);
	self->ConstructL();
	self->ResetModelL();
	CleanupStack::Pop();
	return self;
	}

void CContsyncDocument::ConstructL()
	{
	iModel = CContsyncEng::NewL();
	
	}

// All resources allocated in ConstructL() must be released in 
// the destructor.
CContsyncDocument::~CContsyncDocument()
	{
	delete iModel;
	}

void CContsyncDocument::ResetModelL()
	{
	
	}

CEikAppUi* CContsyncDocument::CreateAppUiL()
	{
    return new(ELeave) CContsyncAppUi;
	}


////////////////////////////////////////////////////////////////
//
// App UI class, CContsyncAppUi
//
////////////////////////////////////////////////////////////////

void CContsyncAppUi::ConstructL()
    {
    BaseConstructL();
	
	iModel = ((CContsyncDocument*)iDocument)->Model();
    iAppView = new(ELeave) CContsyncAppView;
    iAppView->ConstructL(ClientRect());
    }

CContsyncAppUi::~CContsyncAppUi()
	{
    delete iAppView;
	}

void CContsyncAppUi::HandleCommandL(TInt aCommand)
	{
	/* function HandleCommand this is where the menu resources
	 *			are handled, i.e. the functions that are called
	 *			in response to menu invocations.
	 */
	switch (aCommand)
		{

	#include "menubuilder.h"		// DO NOT REMOVE

		}
	}


////////////////////////////////////////////////////////////////
//
// Application view class, CContsyncAppView
//
////////////////////////////////////////////////////////////////

void CContsyncAppView::ConstructL(const TRect& aRect)
    {
	/* Constructor  creates the main app view window, sets its size and loads
	 *				the bitmap splash screen ready for bit-blting 
	 */
	
    CreateWindowL();
#ifdef COMMUNICATOR
    SetRect(aRect);
#else
	SetRectL(aRect);
#endif
	OpenBitmapL();
    ActivateL();
    }


CContsyncAppView::~CContsyncAppView()
	{
	/* destruuctor destruoys the bitmap freeing up memory used
	 */
	delete iMyBitmap;
	}

void CContsyncAppView::Draw(const TRect& /*aRect*/) const
	{
	/* function Draw, redraws the screen every time it is refreshed
	 * this is wheere the bitmap is bit-blted
	 */

	CWindowGc& gc = SystemGc();
	// Clear the application view
	gc.Clear();
	
	// BitBlt splash screen
	// Set Position
#ifdef COMMUNICATOR
	TPoint pos(110,25); 
#else
	TPoint pos(150,50); 
#endif
	// blt the bitmap
	gc.BitBlt(pos, iMyBitmap);

	

	}

void CContsyncAppView::OpenBitmapL()
	{
	/* function  OpenBitmap set the location of the multi-bitmap file 
	 *			 containing the bitmap depending on build type, and 
	 *			 loads the bitmap into the member variable
	 */

#ifndef __WINS__

	#ifdef COMMUNICATOR
		_LIT(KMBMFileName,"d:\\system\\apps\\contsync\\splash.mbm"); 
	#else
		_LIT(KMBMFileName,"c:\\system\\apps\\contsync\\splash.mbm"); 
	#endif

#else
	_LIT(KMBMFileName,"z:\\system\\apps\\contsync\\splash.mbm"); 
#endif


	// load the bitmap from an .mbm file 
	CFbsBitmap* bitmap = new (ELeave) CFbsBitmap(); 

	User::LeaveIfError(bitmap->Load(KMBMFileName)); 
	
	// clean up 
	iMyBitmap = bitmap;

	}



COptionsDialog::COptionsDialog(CContsyncEng* aModel)
	{
	iModel = aModel;


	}

void COptionsDialog::PreLayoutDynInitL()
	{
		
	/* function PreLayoutDynInitL is called before the dialog is displayed
	 *			and can be used to complete the diaolgs options with current
	 *			settings
	 */

	

	STATIC_CAST(CEikChoiceList*,Control(EContactDbRole))->SetCurrentItem(iModel->GetOptions()->GetContactDbRole()); 
	STATIC_CAST(CEikChoiceList*,Control(ENetworkType))->SetCurrentItem(iModel->GetOptions()->GetNetworkType()); 
	STATIC_CAST(CEikChoiceList*,Control(ENetworkRole))->SetCurrentItem(iModel->GetOptions()->GetNetworkRole()); 

	}


TBool COptionsDialog::OkToExitL(TInt /*aKeyCode*/)
	{
/* function OkToExit is called upon the user selecting Ok from the dialog
	 *			and is used to collect the user's input
	 */

	// which contact set should be updated
	CEikChoiceList*	choiceList = STATIC_CAST(CEikChoiceList*, Control(EContactDbRole));
	
	TInt choice = choiceList->CurrentItem();

	iModel->GetOptions()->SetContactsDbRole(choice);
	
	// which network service should be used
	choiceList = STATIC_CAST(CEikChoiceList*, Control(ENetworkType));

	choice = choiceList->CurrentItem();

	iModel->GetOptions()->SetNetworkType(choice);

	// which network role: client / server
	choiceList = STATIC_CAST(CEikChoiceList*, Control(ENetworkRole));
	
	choice = choiceList->CurrentItem();

	iModel->GetOptions()->SetNetworkRole(choice);

	iModel->GetOptions()->WriteIniFile();

	return ETrue;
	}

 