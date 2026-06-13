// ContSync.h
//


#ifndef __CONTSYNC_H
#define __CONTSYNC_H

#include <e32std.h>

#ifdef COMMUNICATOR
#include <coeccntx.h>
#include <eikdialg.h>
#include <eikenv.h>
#include <eikappui.h>
#include <eikapp.h>
#include <eikdoc.h>
#include <eikmenup.h>
#include <eikchlst.h>
#include <eikon.hrh>

#else

#include <coeccntx.h>
#include <eikappui.h>
#include <eikapp.h>
#include <eikdoc.h>
#include <eikenv.h>
#include <eikcmds.hrh>
#include <eikfnlab.h>
#include <eiktbar.h>
#include <eikdialg.hrh>
#include <eikdialg.h>
#include <eikchlst.h>

#endif

#include <ContSync.rsg>
#include "ContSync.hrh"
#include "ContSynceng.h"
#include "Options.h"

const TUid KUidContsync = { 0x101F6BA4 };


//
// class CContsyncAppView
//

class CContsyncAppView : public CCoeControl, public MCoeControlBrushContext
	{
  public:
	void ConstructL(const TRect& aRect);
	~CContsyncAppView();
  private: // from CCoeControl
	void Draw(const TRect& /*aRect*/) const;
	void OpenBitmapL();
  private:
	CFbsBitmap* iMyBitmap;
	


	};

class COptionsDialog;
//
// CContsyncAppUi
//

class CContsyncAppUi : public CEikAppUi
	{
  public:
	void ConstructL();
	~CContsyncAppUi();
  private: // from CEikAppUi
	void HandleCommandL(TInt aCommand);
  private:
	CContsyncAppView* iAppView;
	CContsyncEng *iModel;
	};



//
// CContsyncDocument
//

class CContsyncDocument : public CEikDocument
	{
  public:
	// construct/destruct
	CContsyncDocument(CEikApplication& aApp);
	~CContsyncDocument();
	static CContsyncDocument* NewL(CEikApplication& aApp);
	void ConstructL();
	void ResetModelL();
	CContsyncEng* Model() {return iModel;};
	
  private: // from CEikDocument
	CEikAppUi* CreateAppUiL();
	CContsyncEng* iModel;

	};


//
// CContsyncApplication
//

class CContsyncApplication : public CEikApplication
	{
  private: // from CApaApplication
	CApaDocument* CreateDocumentL();
	TUid AppDllUid() const;
	};




//
// COptionsDialog
//
class COptionsDialog : public CEikDialog
	{
public:
	// this should take an option class which is visible to this and syncheng
	// options class can then be responsible for loading/saving its own state
	// in an ini file.
	COptionsDialog(CContsyncEng* aModel);
private:
	// From CEikDialog
	void PreLayoutDynInitL();		// initialisation - use this to set initialise the fields
									//					to values from options class
	TBool OkToExitL(TInt aKeyCode);	// termination - use this to read the fields, obviously
									//				 from th options class.
private:
	CContsyncEng* iModel;

	};
	

#endif

