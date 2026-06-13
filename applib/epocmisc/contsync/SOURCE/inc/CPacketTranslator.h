

#ifndef CPACKETTRANSLATOR
#define CPACKETTRANSLATOR



#include <cntitem.h>
#include <cntfldst.h>
#include <e32std.h>


#include "CTinyTP.h"





class CPacketTranslator: public CBase
	{
public:
	virtual void EncodeL(CContactItem* aContactItem/*, CEikonEnv* aEikEnv, TBool aBoolean*/) = 0;
	virtual void DecodeL(CContactCard* aContactCard) = 0;
	virtual TBool UpdateFieldL(TDesC &aLabel, CContactCard &aContactCard, TInt aFieldNo, TDesC& aField) = 0;
	virtual TInt GetFieldNumber(CContactCard &aContactCard, TDesC& aLabel) = 0;

protected:
	void ConstructL(CNWInterface* aNWInterface);	
	void AddFieldL(CContactItem* aContactCard, TDesC& aLabel, TDesC& aField);
	TStorageType StorageType(TDesC& aLabel);
	TFieldType FieldType(TDesC& aLabel);
	TTime DateConvert(TDesC& aDate);
	CNWInterface* NWInterface();

private:
	CContactTemplate* iContactTemplate;
	CContactCard* iContactCard;
	CNWInterface* iNWInterface;
	};




class CPsionPacketTranslator: public CPacketTranslator
	{
public:
	static CPsionPacketTranslator* NewL(CNWInterface* aNWInterface);
	void EncodeL(CContactItem* aContactItem/*, CEikonEnv* aEikEnv, TBool aBoolean*/);
	void DecodeL(CContactCard* aContactCard);
	TBool UpdateFieldL(TDesC &aLabel, CContactCard &aContactCard, TInt aFieldNo, TDesC& aField);
	TInt GetFieldNumber(CContactCard &aContactCard, TDesC& aLabel);
private:

	};



class CCommunicatorPacketTranslator: public CPacketTranslator
	{
public:
	static CCommunicatorPacketTranslator* NewL(CNWInterface* aNWInterface);
	void EncodeL(CContactItem* aContactItem/*, CEikonEnv* aEikEnv, TBool aBoolean*/);
	void DecodeL(CContactCard* aContactCard);
	TBool UpdateFieldL(TDesC &aLabel, CContactCard &aContactCard, TInt aFieldNo, TDesC& aField);
	TInt GetFieldNumber(CContactCard &aContactCard, TDesC& aLabel);
	TDesC& FieldLabel(const CContentType *aContentType);
private:
	TBuf<KMaxLabelSize> iLabel;
	};



#endif