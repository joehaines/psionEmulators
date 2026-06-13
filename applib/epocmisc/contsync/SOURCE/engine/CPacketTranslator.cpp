#include "CPacketTranslator.h"
#include "ContSyncDef.h"



void CPacketTranslator::ConstructL(CNWInterface* aNWInterface)
	{
	/* two phase construction - second phase
	 * parameter CNWInterface stored as a reference to the network interface 
	 *			 buffer for output of encode and input of decode
	 */

	iNWInterface = aNWInterface;
	}	



void CPacketTranslator::AddFieldL(CContactItem* aContactCard, TDesC& aLabel, TDesC& aField)
	{
	/* function  AddFieldL adds a field to a ContactCard
	 * parameter aConactCard is the card the field is to added to
	 * parameter aLabel is the field Label
	 * parameter aField is the field content
	 */
	if (StorageType(aLabel) == KStorageTypeText)	
		{
		// creates memory and is pushed on cleanup stack during creation
		CContactItemField* contactField = CContactItemField::NewLC(StorageType(aLabel), FieldType(aLabel));
		
		// creates memory
		contactField->TextStorage()->SetTextL(aField);
		
		if (aLabel.Left(4) == _L("Work"))
			{
			contactField->SetUserFlags(EContactCategoryWork);
			contactField->SetMapping(KUidContactFieldVCardMapWORK);
			}
		else if (aLabel.Left(4) == _L("Home"))
			{
			contactField->SetUserFlags(EContactCategoryHome);
			contactField->SetMapping(KUidContactFieldVCardMapHOME);
			
			}

		
		if(aLabel.Length() > 5 && aLabel.Right(6) == _L("mobile"))
			{
			contactField->AddFieldTypeL(KUidContactFieldVCardMapCELL);
			}
		else if(aLabel.Length() > 3 && aLabel.Right(3) == _L("tel"))
			{
			contactField->AddFieldTypeL(KUidContactFieldVCardMapVOICE);
			}

		// creates memory
		contactField->SetLabelL(aLabel);

		aContactCard->InsertFieldL(*contactField, 10);
		CleanupStack::Pop();//ContactField		
			
		}// end if KStorageTypeText
	else if (StorageType(aLabel) == KStorageTypeDateTime)
		{
		CContactItemField* contactField = CContactItemField::NewL(StorageType(aLabel), FieldType(aLabel));
		CleanupStack::PushL(contactField);
		TTime dateTime;
		dateTime.Set(aField);
		contactField->DateTimeStorage()->SetTime(dateTime);
		aContactCard->AddFieldL(*contactField);
		CleanupStack::Pop();//ContactField
		}// end else if KStorageTypeDateTime
		
	}// end function AddFieldL

TStorageType CPacketTranslator::StorageType(TDesC& aLabel)
	{
	/* function  StorageType finds the storage type associacted with a label
	 *			 currently there are only two supported type text and date/time
	 * parameter aLabel is the label of the field whose storage type is to be found
	 * return	TStorageType KStorageTypeText, KStorageTypeDateTime or NULL if not found
	 */
	TStorageType storageType = NULL;

	if (aLabel == _L("Title") || aLabel == _L("First name") || aLabel == _L("Middle name") ||
		aLabel == _L("Last name") || aLabel == _L("Suffix") || aLabel == _L("Mobile") || 
		aLabel == _L("Mobile") || aLabel == _L("Home tel") || aLabel == _L("Home fax")|| 
		aLabel == _L("Pager") || aLabel == _L("Home Email") || aLabel == _L("Home PO box") || 
		aLabel == _L("Home ext address") || aLabel == _L("Home address") || aLabel == _L("Home city") || 
		aLabel == _L("Home region") || aLabel == _L("Home p'code") || aLabel == _L("Home country") || 
		aLabel == _L("Company") || aLabel == _L("Job title") || aLabel == _L("Work mobile") || 
		aLabel == _L("Work tel") || aLabel == _L("Work fax") || aLabel == _L("Work pager") || 
		aLabel == _L("Work email") || aLabel == _L("Web page") || aLabel == _L("Work PO box") || 
		aLabel == _L("Work ext address") || aLabel == _L("Work address") || aLabel == _L("Work city") || 
		aLabel == _L("Work region") || aLabel == _L("Work p'code") || aLabel == _L("Work country") || 
		aLabel == _L("Notes") || aLabel == _L("Display Name"))
			storageType = KStorageTypeText;
	else if (aLabel == _L("Birthday"))
		storageType = KStorageTypeDateTime;
	
	return storageType;
	}



TFieldType CPacketTranslator::FieldType(TDesC& aLabel)
	{
	/* function  FieldType returns the field type Uid associated with a field Label string
	 * parameter aLabel is the label text whoose equivalent TFieldType Uid is to be found
	 * return	 TFieldType copntaining the Uid associated with aLabel or a 
	 *			 special KUidContactFieldNone
	 */

	TFieldType fieldType = KUidContactFieldNone;

	if (aLabel == _L("Title"))
		fieldType = KUidContactFieldPrefixName;
	else if ( aLabel == _L("First name"))
		fieldType = KUidContactFieldGivenName;
	else if (aLabel == _L("Middle name"))
		fieldType = KUidContactFieldAdditionalName;
	else if (aLabel == _L("Last name"))
		fieldType = KUidContactFieldFamilyName;
	else if (aLabel == _L("Suffix"))
		fieldType = KUidContactFieldSuffixName;
	else if (aLabel == _L("Mobile"))
		fieldType = KUidContactFieldPhoneNumber;
	else if (aLabel == _L("Home tel")) 
		fieldType = KUidContactFieldPhoneNumber;
	else if (aLabel == _L("Home fax"))
		fieldType = KUidContactFieldFax;
	else if (aLabel == _L("Pager")) 
		fieldType = KUidContactFieldPhoneNumber;
	else if (aLabel == _L("Home email")) 
		fieldType = KUidContactFieldEMail;
	else if (aLabel == _L("Home PO box")) 
		fieldType = KUidContactFieldPostOffice;
	else if (aLabel == _L("Home ext address")) 
		fieldType = KUidContactFieldExtendedAddress;
	else if (aLabel == _L("Home address")) 
		fieldType = KUidContactFieldAddress;
	else if (aLabel == _L("Home city")) 
		fieldType = KUidContactFieldLocality;
	else if (aLabel == _L("Home region")) 
		fieldType = KUidContactFieldRegion;
	else if (aLabel == _L("Home p'code")) 
		fieldType = KUidContactFieldPostcode;
	else if (aLabel == _L("Home country")) 
		fieldType = KUidContactFieldCountry;
	else if (aLabel == _L("Company")) 
		fieldType = KUidContactFieldCompanyName;
	else if (aLabel == _L("Job title")) 
		fieldType = KUidContactFieldJobTitle;
	else if (aLabel == _L("Work mobile")) 
		fieldType = KUidContactFieldPhoneNumber;
	else if (aLabel == _L("Work tel")) 
		fieldType = KUidContactFieldPhoneNumber;
	else if (aLabel == _L("Work fax")) 
		fieldType = KUidContactFieldFax;
	else if (aLabel == _L("Work pager")) 
		fieldType = KUidContactFieldPhoneNumber;
	else if (aLabel == _L("Work email")) 
		fieldType = KUidContactFieldEMail;
	else if (aLabel == _L("Web page")) 
		fieldType = KUidContactFieldUrl;
	else if (aLabel == _L("Work PO box")) 
		fieldType = KUidContactFieldPostOffice;
	else if (aLabel == _L("Work ext address")) 
		fieldType = KUidContactFieldExtendedAddress;
	else if (aLabel == _L("Work address")) 
		fieldType = KUidContactFieldAddress;
	else if (aLabel == _L("Work city")) 
		fieldType = KUidContactFieldLocality;
	else if (aLabel == _L("Work region")) 
		fieldType = KUidContactFieldRegion;
	else if (aLabel == _L("Work p'code")) 
		fieldType = KUidContactFieldPostcode;
	else if (aLabel == _L("Work country")) 
		fieldType = KUidContactFieldCountry;
	else if (aLabel == _L("Notes"))
		fieldType = KUidContactFieldNote;
	else if (aLabel == _L("Birthday"))
		fieldType = KUidContactFieldBirthday;
	else if (aLabel == _L("Display name"))
		fieldType = KUidContactFieldNone;
	
	
	return fieldType;
	}



TTime CPacketTranslator::DateConvert(TDesC& aDate)
	{
	/* function  DateConvert converts date format from descriptor dd/mm/yyyy to TTime
	 * parameter aDate is the holding descriptor for dd/mm/yyyy
	 * return	 TTime holding the equivalent date
	 */  
	
	// to creates a TTime object date must be yyyymmdd format

	// create buf and append yyyy
	TBuf<9> hDate = aDate.Right(4);
					
	// append mm
	// must Subtract 1 for TTime
					
	TLex lex = aDate.Mid(3,2);
	TInt val;
		
	lex.Val(val);

	TBuf<2> num;
	num.Num(val - 1);
	hDate.Append(num);



	// append dd
					
					
	lex = aDate.Left(2);
	lex.Val(val);
	num.Num(val - 1);
	hDate.Append(num);
					
	hDate.Append(_L(":"));
			
	// create the TTime object				
	TTime dateTime;
	dateTime.Set(hDate);
	
	
	return dateTime;

	}

CNWInterface* CPacketTranslator::NWInterface()
	{
	return iNWInterface;

	}

CPsionPacketTranslator* CPsionPacketTranslator::NewL(CNWInterface* aNWInterface)
	{	
	/* two phase construction - first phase
	 * parameter CNWInterface passed to ConstructL gives a reference to 
	 *	    	 the network interface buffer for output of encode and 
	 *			 input of decode
	 * return	 Pointer to self
	 */
	CPsionPacketTranslator* self = new (ELeave) CPsionPacketTranslator();
	CleanupStack::PushL(self);
	self->ConstructL(aNWInterface);
	CleanupStack::Pop();
	return self;
	}

void CPsionPacketTranslator::EncodeL(CContactItem* aContactItem)
	{
	/* function Encode: encodes a CContactItem* into a network packet
	 * parameter aContactItem provides the contact data to be encoded
	 *
	 * encoded data is written to the Network Buffer ready for transmission
	 */

	// empty the network buffer
	NWInterface()->SetBuffer(_L(""));

	// ensure that the contact type is one supported 
	if (aContactItem->Type() == KUidContactCard)
		{
		// pointer to database owned fields, no need to push onto cleanup stack
		CContactItemFieldSet* fieldSet;
		fieldSet = &aContactItem->CardFields();
		TInt fieldCount = 0, maxCount = fieldSet->Count();
		TBuf<256> temp;

		// step through contact fields until reach maxcount, i.e. the last field
		while (fieldCount < maxCount)
			{
			// ensure that the field is a supported type and not disabled
			if ((((*fieldSet)[fieldCount]).StorageType() == KStorageTypeText ||
				((*fieldSet)[fieldCount]).StorageType() == KStorageTypeDateTime) &&
				!((*fieldSet)[fieldCount]).IsDisabled())
				{
				// empty the local buffer
				temp.FillZ();
				// copy the label into ther local buffer
				temp.Copy(((*fieldSet)[fieldCount]).Label());
				
				// nokia does not support notes fields
				if (temp != _L("Notes"))
					{
					temp.Append(_L(","));

					if (temp.Length() > 1)
						{
						NWInterface()->AppendBuffer(temp);
						// handle the date/time birthday field
						if (((*fieldSet)[fieldCount]).ContentType().ContainsFieldType(KUidContactFieldBirthday))
							{
							TTime time = ((*fieldSet)[fieldCount]).DateTimeStorage()->Time();
							time.FormatL(temp, _L("%D%M%Y%/0%1%/1%2%/2%3%/3"));
							}
						// handle the text fields
						else
							{
							temp.Copy(((*fieldSet)[fieldCount]).TextStorage()->Text());
							}
						temp.Append(_L(";"));
						NWInterface()->AppendBuffer(temp);
						}// end if length > 1	
					}// end of if not notes field
				}// end if text or date/time
				
				fieldCount++;
			}// end while more fields


		}// end if ccontactiutem == CContactCard
	}// end function Encode


void CPsionPacketTranslator::DecodeL(CContactCard* aContactCard)
	{
	/* function DecodeL: decodes a network packet into a CContactsCard
	 * parameter aContactCard provides the contact card to which the network
	 *			 buffer should be decoded into
	 */

	

	// create a local copy of the network buffer for clarity
	TBuf<KMaxContactDataSize> buf;
	buf.Copy(NWInterface()->GetBuffer());

	TChar comma = ',', semiColon = ';';
	
	TBuf<KMaxFieldSize> field;
	TBuf<KMaxLabelSize> label;
	TInt bufPtr = 0, elementSize;

	while (bufPtr < (buf.Length() - 1))
		{
		
		// get label size;
		for (elementSize = bufPtr; (TChar)(buf[elementSize]) != comma ; elementSize++);
		// copy label
		label.Copy(&buf[bufPtr], elementSize - bufPtr);
		
		// move bufptr past label, reday for field component
		bufPtr += (elementSize - bufPtr) + 1;// +1 steps past ';'

		// get field size		
		for (elementSize = bufPtr; (TChar)(buf[elementSize]) != semiColon ; elementSize++);

		// copy field
		
		field.Copy(&buf[bufPtr], elementSize - bufPtr);
		

		// only update fields where required
		if (field.Length() == 0 || FieldType(label) == KUidContactFieldNone)
			{
			bufPtr++; //step over ';'	
			}
		else		
			{
			// move bufPtr past field
			bufPtr += (elementSize - bufPtr) + 1;// +1 steps past ';'
			
			// get the relevant field number of the label for the field to be decoded into
			TInt fieldNo = GetFieldNumber(*aContactCard, label);
			

			TBool fieldExists = EFalse;
			// if recognised field
			if (fieldNo != KErrNotFound)
				{
				// Update field creates memory, by use of SetTextL
				fieldExists = UpdateFieldL(label, *aContactCard, fieldNo, field);
					
				// only required in Linda version
				//aContactCard->CardFields()[fieldNo].SetLabelL(label);	
				}
			// if unrecognised field or update field was ubnable to loacte
			if (!fieldExists)
				{
				// the the field
				AddFieldL(aContactCard, label, field);

				}// end if !field exists
				
			}// end if no field data
			
		}// end while buffer data

	}// end function DecodeL



TInt CPsionPacketTranslator::GetFieldNumber(CContactCard &aContactCard, TDesC& aLabel)
	{
	/* function  GetFieldNumber: returns the field number of a field within a CConatactCard 
	 *			 corresponding to a supplied label
	 * parameter aContactCard is the card to find the field within
	 * parameter aLabel is the label of the field to be found
	 * return	 a Tint indexing fields position within aContactCard or KErrNotFound
	 */

	/* note: a CContactCard may have more than one field of the same type, defined in FieldType()
	 * for example home tel and work tel, therefore it is necessary to also check that the fields
	 * have the correct mappings
	 */
	
	// find the field with the given label type within aContactCard


		
	TInt fieldNo = aContactCard.CardFields().Find(FieldType(aLabel));

	if (fieldNo != KErrNotFound)
		{
	
		/* Field Definitions - Psion
		zero valued field not set

		0	Title			1000178c	0			0			0		0
		1	First name		1000137c	0			0			0		0
		2	Middle name		1000178a	0			0			0		0
		3	Last name		1000137d	0			0			0		0
		4	Suffix			1000178b	0			0			0		0
		5	Mobile			1000130e	100039db	10003e71	0		0
		6	Home tel		1000130e	100039db	0			0		0
		7	Home fax		10001791	100039db	100039de	0		0
		8	Pager			1000130e	10003e72	100039db	0		0
		9	Home email		1000178e	100039db	0			0		0
		10	Home address	1000130c	100039db	0			0		0
		11	Company			1000130d	0			0			0		0
		12	Job title		1000130d	0			0			0		0
		13	Work mobile		1000130e	10003e71	100039da	0		0
		14	Work tel		1000130e	100039da	0			0		0
		15	Work fax		10001791	100039da	100039de	0		0
		16	Work pager		1000130e	10003e72	100039da	0		0
		17	Work email		1000178e	100039da	0			0		0
		18	Web page		10004035	0			0			0		0
		19 	Work address	1000130c	100039da	0			0		0
		20	Birthday		10004034	0			0			0		0
		21	Notes			1000401c	0			0			0		0
		22	Display name	1000401c	0			0			0		0

		*/

		// ensure that this field type is a text field
		if (StorageType(aLabel) == KStorageTypeText)
			{
			// if the label indicates a work field
			if (aLabel.Left(4) == _L("Work") /*&& !aContactCard.CardFields()[fieldNo].ContentType().ContainsFieldType(KUidContactFieldVCardMapWORK)*/)
				{
				if (aLabel.Right(6) == _L("mobile") /*&& !aContactCard.CardFields()[fieldNo].ContentType().ContainsFieldType(KUidContactFieldVCardMapCELL)*/)
					{
					while (fieldNo > -1 && !(aContactCard.CardFields()[fieldNo].ContentType().ContainsFieldType(KUidContactFieldVCardMapCELL)
						&& aContactCard.CardFields()[fieldNo].ContentType().ContainsFieldType(KUidContactFieldVCardMapWORK)
						&& !aContactCard.CardFields()[fieldNo].ContentType().ContainsFieldType(KUidContactFieldVCardMapHOME)))
						{
						fieldNo = aContactCard.CardFields().FindNext(FieldType(aLabel), fieldNo + 1);
						}
					
					}
				else if (aLabel.Right(3) == _L("tel") /*&& !aContactCard.CardFields()[fieldNo].ContentType().ContainsFieldType(KUidContactFieldVCardMapTEL)*/)
					{
					while (fieldNo > -1 && !(aContactCard.CardFields()[fieldNo].ContentType().ContainsFieldType(KUidContactFieldVCardMapTEL)
						&& aContactCard.CardFields()[fieldNo].ContentType().ContainsFieldType(KUidContactFieldVCardMapWORK)
						&& !aContactCard.CardFields()[fieldNo].ContentType().ContainsFieldType(KUidContactFieldVCardMapCELL)))
						{
						fieldNo = aContactCard.CardFields().FindNext(FieldType(aLabel), fieldNo + 1);
						}
					
					}
				else
					{
					while (fieldNo > -1 && !(aContactCard.CardFields()[fieldNo].ContentType().ContainsFieldType(KUidContactFieldVCardMapWORK)
						&& !aContactCard.CardFields()[fieldNo].ContentType().ContainsFieldType(KUidContactFieldVCardMapHOME)))
						{
						fieldNo = aContactCard.CardFields().FindNext(FieldType(aLabel), fieldNo + 1);
						}
					
					}
			
				}
			else if (aLabel.Left(4) == _L("Home")/* && !aContactCard.CardFields()[fieldNo].ContentType().ContainsFieldType(KUidContactFieldVCardMapHOME)*/)
				{
				if (aLabel.Right(3) == _L("tel"))
					{
					while (fieldNo > -1 && !(aContactCard.CardFields()[fieldNo].ContentType().ContainsFieldType(KUidContactFieldVCardMapHOME)
						&& aContactCard.CardFields()[fieldNo].ContentType().ContainsFieldType(KUidContactFieldVCardMapTEL)
						&& !aContactCard.CardFields()[fieldNo].ContentType().ContainsFieldType(KUidContactFieldVCardMapCELL)))
						{
						fieldNo = aContactCard.CardFields().FindNext(FieldType(aLabel), fieldNo + 1);
						}
					}
				else
					{
					while (fieldNo > -1 && !aContactCard.CardFields()[fieldNo].ContentType().ContainsFieldType(KUidContactFieldVCardMapHOME))
						{
						fieldNo = aContactCard.CardFields().FindNext(FieldType(aLabel), fieldNo + 1);
						}
					}
				}
			else if (aLabel.Length() > 5 && aLabel.Left(6) == _L("Mobile"))
				{
				while (fieldNo > -1 && !(aContactCard.CardFields()[fieldNo].ContentType().ContainsFieldType(KUidContactFieldVCardMapHOME)
					&& aContactCard.CardFields()[fieldNo].ContentType().ContainsFieldType(KUidContactFieldVCardMapCELL)))
					{
					fieldNo = aContactCard.CardFields().FindNext(FieldType(aLabel), fieldNo + 1);
					}
				
				

				}// end else mobile
			}// end else if storage type text
		}// end if field not found
	
	return fieldNo;
	}// end function GetFieldNumber



TBool CPsionPacketTranslator::UpdateFieldL(TDesC &aLabel, CContactCard &aContactCard, TInt aFieldNo, TDesC& aField)
	{
	/* UpdateField copies aField into the field number aFieldNo of aContactCard 
	 * parameter aLabel is used to check the storage type and field type of a Label to ensure
	 *			 the fields are copied in the correct format.
	 * parameter aContactCard is the card containing the field to be updated
	 * parameter aFieldNo is the field number within aConatctCard to be updated
	 * parameter aFieild is a descriptor holding the data to be copied into the field
	 * return type is boolean, indicating whether or not the field was updated
	*/
	
	// check that field number is found
	if (aFieldNo > -1)
		{
		// if field is text field
		if (StorageType(aLabel) == KStorageTypeText)
			{
			if (FieldType(aLabel) != KUidContactFieldAddress)
				{
				// Warning SetTextL Creates Memory
				aContactCard.CardFields()[aFieldNo].TextStorage()->SetTextL(aField);
				}// end if not address type
			else // aField is address type, therefore needs converting for between devices
				{
				TBuf<256> spacedField;
				spacedField.Copy(aField);
				_LIT(KSpaceString," "); 
				TInt pos = 0;

				// replace char 06 with ", " for formatting;
				while ((pos = spacedField.Locate(06)) > 0)
					spacedField.Replace(pos, 1, KSpaceString);
				


				TBuf<256> existingField = aContactCard.CardFields()[aFieldNo].TextStorage()->Text();
				existingField.Append(spacedField);
				aContactCard.CardFields()[aFieldNo].TextStorage()->SetTextL(existingField);
				}

			// ensures that the field iis visible and usable
			aContactCard.CardFields()[aFieldNo].SetHidden(EFalse);
			// set private not available in ER5
#ifdef COMMUNICATOR 
			aContactCard.CardFields()[aFieldNo].SetPrivate(EFalse);
#endif
			aContactCard.CardFields()[aFieldNo].SetDisabled(EFalse);
			// field has been updated
			return ETrue;
			}//end if KStorageTypeText
						

		else if (StorageType(aLabel) == KStorageTypeDateTime)
			{
			// ensure that supplied field exists
			if (aField.Length() > 0)
				{
				TBuf<30> dateString;
				_LIT(KDateString1,"%E%D%X%N%Y %1 %2 %3"); 
				
				// convert the date from into that for the field
				TTime dateTime = DateConvert(aField);
				dateTime.FormatL(dateString,KDateString1); 
						
				//Set the date field
				aContactCard.CardFields()[aFieldNo].DateTimeStorage()->SetTime(dateTime );
				
				// ensure that the field is visible and usable
				aContactCard.CardFields()[aFieldNo].SetHidden(EFalse);
				// set private not available in ER5
#ifdef COMMUNICATOR
				aContactCard.CardFields()[aFieldNo].SetPrivate(EFalse);
#endif
				aContactCard.CardFields()[aFieldNo].SetDisabled(EFalse);
				}
			
			// field has been updated
			return ETrue;
		
			}// end if KStorageTypeDateTime

		}//end if fieldNo > -1
	
	//field has not been updated
	return EFalse;

	}// end function UpdateFieldL


CCommunicatorPacketTranslator* CCommunicatorPacketTranslator::NewL(CNWInterface* aNWInterface)
	{
	/* two phase construction - first phase
	 * parameter CNWInterface passed to ConstructL gives a reference to 
	 *	    		 the network interface buffer for output of encode and 
	 *			 input of decode
	 * return	 Pointer to self
	 */


	CCommunicatorPacketTranslator* self = new (ELeave) CCommunicatorPacketTranslator();
	CleanupStack::PushL(self);
	self->ConstructL( aNWInterface);
	CleanupStack::Pop();
	return self;
	}



void CCommunicatorPacketTranslator::EncodeL(CContactItem* aContactItem)
	{
	/* function Encode: encodes a CContactItem* into a network packet
	 * parameter aContactItem provides the contact data to be encoded
	 *
	 * encoded data is written to the Network Buffer ready for transmission
	 */
	
	// empty the network buffer
	NWInterface()->SetBuffer(_L(""));

	// ensure that the contact type is one supported 
	// Note Psion does not support Default Voice mailbox
	if (aContactItem->Type() == KUidContactCard && aContactItem->CardFields()[1].TextStorage()->Text() != _L("Default voice mailbox"))
		{
		
		// pointer to database owned fields, no need to push onto cleanup stack
		CContactItemFieldSet* fieldSet;
		fieldSet = &aContactItem->CardFields();

		TInt fieldCount = 0, maxCount = fieldSet->Count();
		
		TBuf<256> temp;

		// step through contact fields until reach maxcount, i.e. the last field
		while (fieldCount < maxCount)
			{
			// ensure that the field is a supported type and not disabled
			if ((((*fieldSet)[fieldCount]).StorageType() == KStorageTypeText ||
				((*fieldSet)[fieldCount]).StorageType() == KStorageTypeDateTime) &&
				!((*fieldSet)[fieldCount]).IsDisabled())
				{
				// empty the local buffer
				temp.FillZ();
				
				// copy the label associated with this field into ther local buffer
				temp.Copy(FieldLabel(&((*fieldSet)[fieldCount].ContentType()) ));
		
				// the psion does not support this field
				if (temp != _L("Display name"))
					{
					// create a buffer for the field data
					TBuf<KMaxFieldSize> field;
					// ensure that the buffer is empty, when called in a loop
					field.Zero();

					// handle the date/time birthday field
					if (((*fieldSet)[fieldCount]).ContentType().ContainsFieldType(KUidContactFieldBirthday))
						{
						TTime time = ((*fieldSet)[fieldCount]).DateTimeStorage()->Time();
						time.FormatL(field, _L("%D%M%Y%/0%1%/1%2%/2%3%/3"));
						}
					// handle the text fields
					else
						{
						field.Copy(((*fieldSet)[fieldCount]).TextStorage()->Text());
						}
					
					// is the field has any data, as no point appending to buffer if not 
					if (field.Length() > 0)
						{
						//create this field label tuple = label,field;
						temp.Append(_L(","));
						temp.Append(field);
						temp.Append(_L(";"));
						// append the field lable tuple to the network buffer
						NWInterface()->AppendBuffer(temp);
						}// end if field has data

					}// end if not Display Name
				}// end if text or date/time

			fieldCount++;
			}// end while more fields


		}// end if ccontactiutem == CContactCard

	}// end CPacketTranslator::EncodeL



void CCommunicatorPacketTranslator::DecodeL(CContactCard* aContactCard)
	{
	/* function DecodeL: decodes a network packet into a CContactsCard
	 * parameter aContactCard provides the contact card to which the network
	 *			 buffer should be decoded into
	 */

	// create a local copy of the network buffer for clarity
	TBuf<KMaxContactDataSize> buf;
	buf.Copy(NWInterface()->GetBuffer());

	TChar comma = ',', semiColon = ';';
	
	TBuf<KMaxFieldSize> field;
	TBuf<KMaxLabelSize> label;
	TInt bufPtr = 0, elementSize;

	while (bufPtr < (buf.Length() - 1))
		{
		
		// get label size;
		for (elementSize = bufPtr; (TChar)(buf[elementSize]) != comma ; elementSize++);
		// copy label
		label.Copy(&buf[bufPtr], elementSize - bufPtr);
		
		// move bufptr past label, reday for field component
		bufPtr += (elementSize - bufPtr) + 1;// +1 steps past ';'

		// get field size		
		for (elementSize = bufPtr; (TChar)(buf[elementSize]) != semiColon ; elementSize++);	
		// copy field
		field.Copy(&buf[bufPtr], elementSize - bufPtr);
		
		
		// only update fields where required
		if (field.Length() == 0) 
			{
			bufPtr++; //step over ';'
			}
		else 
			{
			// move bufPtr past field
			bufPtr += (elementSize - bufPtr) + 1;// +1 steps past ';'

			if (FieldType(label) != KUidContactFieldNone)
				{
				
				// get the relevant field number of the label for the field to be decoded into
				TInt fieldNo = GetFieldNumber(*aContactCard, label);

				TBool fieldExists = EFalse;
				
				// if recognised field
				if (fieldNo != KErrNotFound)
					{
					
					// Update field creates memory, by use of SetTextL
					fieldExists = UpdateFieldL(label, *aContactCard, fieldNo, field);
					
					// creates Memory for label, for which we have no control
					aContactCard->CardFields()[fieldNo].SetLabelL(label);	
					
					}
				
				// if unrecognised field or update field was unable to locate
				if (!fieldExists)
					{
					
					// add the field
					AddFieldL(aContactCard, label, field);
					
					}// end if !field exists

 
				}// end if no field data
			}
	
		}// end while buffer data
	}// end function DecodeL


TInt CCommunicatorPacketTranslator::GetFieldNumber(CContactCard &aContactCard, TDesC& aLabel)
	{
	/* function  GetFieldNumber: returns the field number of a field within a CContactCard 
	 *			 corresponding to a supplied label
	 * parameter aContactCard is the card to find the field within
	 * parameter aLabel is the label of the field to be found
	 * return	 a Tint indexing fields position within aContactCard or KErrNotFound
	 */

	

	/* note: a CContactCard may have more than one field of the same type, defined in FieldType()
	 * for example home tel and work tel, therefore it is necessary to also check that the fields
	 * have the correct mappings
	 */
	
	// find the field with the given label type within aContactCard

	
	// find the field with the given label type within aContactCard
	TInt fieldNo = aContactCard.CardFields().Find(FieldType(aLabel));

	if (fieldNo != KErrNotFound)
		{
		/* Field Definitions - Nokia
		Telephone Numbers - this is only some of the definitions
					   |	Contains Field Type
		Field# | Label | tel | cell | voice | home | work
		   4   | W.Tel |  1  |  0   |   1   |   0  |  1
		   5   | W.Mob |  1  |  1   |   1   |   0  |  1
		  17   | P.GSM |  1  |  1   |   1   |   1  |  0
		  18   | P.Tel |  1  |  0   |   1   |   1  |  0
		*/
		
		// ensure that this field type is a text field
		if (StorageType(aLabel) == KStorageTypeText)
			{
			// if the label indicates a work field
			if (aLabel.Left(4) == _L("Work"))
				{
				// if the label indicates a mobile field
				if (aLabel.Right(6) == _L("mobile"))
					{
					// while the current field is not work or mobile and we havn't run out of fields
					while (fieldNo > -1 && !(aContactCard.CardFields()[fieldNo].ContentType().ContainsFieldType(KUidContactFieldVCardMapCELL)
						&& aContactCard.CardFields()[fieldNo].ContentType().ContainsFieldType(KUidContactFieldVCardMapWORK)))
						{
						// find the next field type corresponding to aLabel (telephone number)
						fieldNo = aContactCard.CardFields().FindNext(FieldType(aLabel), fieldNo + 1);
						}
					}// end if mobile
				// if the label indicates a terrestrial telephone number
				else if (aLabel.Right(3) == _L("tel"))
					{
					// while the current field is not work or telephone and we haven't run out of fields
					while (fieldNo > -1 && 
						!(aContactCard.CardFields()[fieldNo].ContentType().ContainsFieldType(KUidContactFieldVCardMapWORK)
						&& aContactCard.CardFields()[fieldNo].ContentType().ContainsFieldType(KUidContactFieldVCardMapVOICE)) )
						{
						// find the next field type corresponding to aLabel (telephone number)
						fieldNo = aContactCard.CardFields().FindNext(FieldType(aLabel), KUidContactFieldVCardMapWORK, fieldNo + 1);
						}
					}// end if telephone
				else if (aLabel.Right(3) == _L("fax"))
					{
					// while the current field is not work or telephone and we haven't run out of fields
					while (fieldNo > -1 && 
						!(aContactCard.CardFields()[fieldNo].ContentType().ContainsFieldType(KUidContactFieldVCardMapWORK)
						&& aContactCard.CardFields()[fieldNo].ContentType().ContainsFieldType(KUidContactFieldVCardMapFAX)) )
						{
						// find the next field type corresponding to aLabel (telephone number)
						fieldNo = aContactCard.CardFields().FindNext(FieldType(aLabel), KUidContactFieldVCardMapWORK, fieldNo + 1);
						}
					}// end if telephone
				// this is not a telephone field and is therefore just a work type field
				else
					{
					// while the current field is not work and we havn't run out of fields
					while (fieldNo > -1 && !aContactCard.CardFields()[fieldNo].ContentType().ContainsFieldType(KUidContactFieldVCardMapWORK))
						{
						// find the next field type corresponding to aLabel (usually address)
						fieldNo = aContactCard.CardFields().FindNext(FieldType(aLabel), fieldNo + 1);
						}
					}// end else just work field
				}// end if work label
			// else if label indicates a home field
			else if (aLabel.Left(4) == _L("Home"))
				{
				// if label indicates a telephone field
				if (aLabel.Right(3) == _L("tel"))
					{
					// while the current field is not home or telephone and is mobile, or we haven't run out of fields
					while (fieldNo > -1 && !(aContactCard.CardFields()[fieldNo].ContentType().ContainsFieldType(KUidContactFieldVCardMapHOME)
						&& aContactCard.CardFields()[fieldNo].ContentType().ContainsFieldType(KUidContactFieldVCardMapTEL)
						&& aContactCard.CardFields()[fieldNo].ContentType().ContainsFieldType(KUidContactFieldVCardMapVOICE)
						&& !aContactCard.CardFields()[fieldNo].ContentType().ContainsFieldType(KUidContactFieldVCardMapCELL)))
						{
						fieldNo = aContactCard.CardFields().FindNext(FieldType(aLabel), fieldNo + 1);
						}
					}// end if telephone field
				else if (aLabel.Right(3) == _L("tel"))
					{
					// while the current field is not home or telephone and is mobile, or we haven't run out of fields
					while (fieldNo > -1 && !(aContactCard.CardFields()[fieldNo].ContentType().ContainsFieldType(KUidContactFieldVCardMapHOME)
						&& aContactCard.CardFields()[fieldNo].ContentType().ContainsFieldType(KUidContactFieldVCardMapFAX)
						&& !aContactCard.CardFields()[fieldNo].ContentType().ContainsFieldType(KUidContactFieldVCardMapCELL)))
						{
						fieldNo = aContactCard.CardFields().FindNext(FieldType(aLabel), fieldNo + 1);
						}
					}// end if telephone field
				// this is not a telephone field and is therefore just a home type field
				else
					{
					// while the current field is not home type and we havn't run out of fields
					while (fieldNo > -1 && !aContactCard.CardFields()[fieldNo].ContentType().ContainsFieldType(KUidContactFieldVCardMapHOME))
						{
						fieldNo = aContactCard.CardFields().FindNext(FieldType(aLabel), fieldNo + 1);
						}
					}
				}// end else plain home type field
			// else must be mobile label; equivalent to Home mobile
			else if (aLabel.Length() > 5 && aLabel.Left(6) == _L("Mobile"))
				{
				while (fieldNo > -1 && !(aContactCard.CardFields()[fieldNo].ContentType().ContainsFieldType(KUidContactFieldVCardMapHOME)
					&& aContactCard.CardFields()[fieldNo].ContentType().ContainsFieldType(KUidContactFieldVCardMapCELL)))
					{
					fieldNo = aContactCard.CardFields().FindNext(FieldType(aLabel), fieldNo + 1);
					}
				}// end else mobile
			}// end else if storage type text
		}// end if field not found
	
	return fieldNo;
	}// end function GetFieldNumber


TBool CCommunicatorPacketTranslator::UpdateFieldL(TDesC &aLabel, CContactCard &aContactCard, TInt aFieldNo, TDesC& aField)
	{
	/* UpdateField copies aField into the field number aFieldNo of aContactCard 
	 * parameter aLabel is used to check the storage type and field type of a Label to ensure
	 *			 the fields are copied in the correct format.
	 * parameter aContactCard is the card containing the field to be updated
	 * parameter aFieldNo is the field number within aConatctCard to be updated
	 * parameter aFieild is a descriptor holding the data to be copied into the field
	 * return type is boolean, indicating whether or not the field was updated
	*/
	
	// check that field number is found
	if (aFieldNo > -1)
		{
		// if field is text field
		if (StorageType(aLabel) == KStorageTypeText)
			{

			if (FieldType(aLabel) != KUidContactFieldAddress)
				{
				// Warning SetTextL Creates Memory
				aContactCard.CardFields()[aFieldNo].TextStorage()->SetTextL(aField);
				}// end if not address type

			else // aField is address type, therefore needs converting for between devices
				{
				TBuf<256> spacedField;
				spacedField.Copy(aField);
				_LIT(KSpaceString," "); 

				TInt pos = spacedField.Locate(06);;
				while (pos != KErrNotFound)
					{
					spacedField.Replace(pos, 1, KSpaceString);
					pos = spacedField.Locate(06);
					}

				// Warning SetTextL Creates Memory
				aContactCard.CardFields()[aFieldNo].TextStorage()->SetTextL(spacedField);

				}// end else address type

			// ensures that the field is visible and usable
			aContactCard.CardFields()[aFieldNo].SetHidden(EFalse);
//			aContactCard.CardFields()[aFieldNo].SetPrivate(EFalse);
			aContactCard.CardFields()[aFieldNo].SetDisabled(EFalse);
			
			// field has been updated
			return ETrue;
			}// end if KStorageTypeText

			
		else if (StorageType(aLabel) == KStorageTypeDateTime)
			{

			// ensure that supplied field exists
			if (aField.Length() > 0)
				{
				
				TBuf<30> dateString;
				_LIT(KDateString1,"%E%D%X%N%Y %1 %2 %3"); 
				
				// convert the date from into that for the field
				TTime dateTime = DateConvert(aField);
				dateTime.FormatL(dateString,KDateString1); 
						
				//Set the date field
				aContactCard.CardFields()[aFieldNo].DateTimeStorage()->SetTime(dateTime );
				// ensure that the field is visible and usable
				aContactCard.CardFields()[aFieldNo].SetHidden(EFalse);
//				aContactCard.CardFields()[aFieldNo].SetPrivate(EFalse);
				aContactCard.CardFields()[aFieldNo].SetDisabled(EFalse);
				}
			
			// field has been updated
			return ETrue;


			}// end if KStorageTypeDateTime
	
		}// end if fieldNo > -1
	
	// field has not been updated
	return EFalse;

	}// end function UpdateFieldL



TDesC& CCommunicatorPacketTranslator::FieldLabel(const CContentType *aContentType)
	{
	/* function  FieldLabel takes a CContenttype and returns the equivalent field label
	 *			 this is because the 9210 does not store field labels
	 * parameter aContentType is the content type if a field in terms of its filed type
	 *			 and extended vCard Mappings
	 * return	 a reference to a local iLabel containing the field types label
	 */

	iLabel.Zero();

	// if the field toye is registered this will be true
	if (aContentType->FieldTypeCount() > 0)
		{
		// deal with the simple fields, i.e. those with only one FieldType
		if (aContentType->FieldType(0) == KUidContactFieldPrefixName)
			iLabel = _L("Title");
		else if (aContentType->FieldType(0) == KUidContactFieldGivenName)
			iLabel = _L("First name");
		else if (aContentType->FieldType(0) == KUidContactFieldAdditionalName)
			iLabel = _L("Middle name");
		else if (aContentType->FieldType(0) == KUidContactFieldFamilyName)
			iLabel = _L("Last name");
		else if (aContentType->FieldType(0) == KUidContactFieldSuffixName)
			iLabel = _L("Suffix");
		else if (aContentType->FieldType(0) == KUidContactFieldCompanyName) 
			iLabel = _L("Company");
		else if (aContentType->FieldType(0) == KUidContactFieldJobTitle) 
			iLabel = _L("Job title");
		else if (aContentType->FieldType(0) == KUidContactFieldNote)
			iLabel = _L("Notes");
		else if (aContentType->FieldType(0) == KUidContactFieldBirthday)
			iLabel = _L("Birthday");
		else if (aContentType->FieldType(0) == KUidContactFieldNone)
			iLabel = _L("Display name");
		else if (aContentType->FieldType(0) == KUidContactFieldUrl) // note psion has only one
					iLabel = _L("Web page");	
		// home type fields, these will have at least two field types
		else if (aContentType->ContainsFieldType(KUidContactFieldVCardMapHOME))
			{
			if (aContentType->FieldType(0) == KUidContactFieldPhoneNumber 
				&& aContentType->ContainsFieldType(KUidContactFieldVCardMapTEL)
				&& aContentType->ContainsFieldType(KUidContactFieldVCardMapCELL))
				iLabel = _L("Mobile");
			else if (aContentType->FieldType(0) == KUidContactFieldPhoneNumber 
				&& aContentType->ContainsFieldType(KUidContactFieldVCardMapVOICE)
				&& aContentType->ContainsFieldType(KUidContactFieldVCardMapTEL))
				iLabel = _L("Home tel");
			else if (aContentType->FieldType(0) == KUidContactFieldPhoneNumber 
				&& aContentType->ContainsFieldType(KUidContactFieldVCardMapFAX))
				iLabel = _L("Home fax");
			else if (aContentType->FieldType(0) == KUidContactFieldPhoneNumber 
				&& aContentType->ContainsFieldType(KUidContactFieldVCardMapPAGER))
				iLabel = _L("Pager");
			else if (aContentType->FieldType(0) == KUidContactFieldEMail) 
				iLabel = _L("Home email");
			else if (aContentType->FieldType(0) == KUidContactFieldPostOffice) 
				iLabel = _L("Home PO box");
			else if (aContentType->FieldType(0) == KUidContactFieldExtendedAddress) 
				iLabel = _L("Home ext address");
			else if (aContentType->FieldType(0) == KUidContactFieldAddress) 
				iLabel = _L("Home address");
			else if (aContentType->FieldType(0) == KUidContactFieldLocality) 
				iLabel = _L("Home city");
			else if (aContentType->FieldType(0) == KUidContactFieldRegion) 
				iLabel = _L("Home region");
			else if (aContentType->FieldType(0) == KUidContactFieldPostcode) 
				iLabel = _L("Home p'code");
			else if (aContentType->FieldType(0) == KUidContactFieldCountry) 
				iLabel = _L("Home country");
			}
		// work type fields will also have at least two field types
		else if (aContentType->ContainsFieldType(KUidContactFieldVCardMapWORK))
			{
			if (aContentType->FieldType(0) == KUidContactFieldPhoneNumber 
				&& aContentType->ContainsFieldType(KUidContactFieldVCardMapCELL)) 
				iLabel = _L("Work mobile");
			else if (aContentType->FieldType(0) == KUidContactFieldPhoneNumber 
				&& aContentType->ContainsFieldType(KUidContactFieldVCardMapVOICE)) 
				iLabel = _L("Work tel");
			else if (aContentType->FieldType(0) == KUidContactFieldPhoneNumber 
				&& aContentType->ContainsFieldType(KUidContactFieldVCardMapFAX)) 
				iLabel = _L("Work fax");
			else if (aContentType->FieldType(0) == KUidContactFieldPhoneNumber 
				&& aContentType->ContainsFieldType(KUidContactFieldVCardMapPAGER))
				iLabel = _L("Work pager");
			else if (aContentType->ContainsFieldType(KUidContactFieldVCardMapWORK))
				{	
				if (aContentType->FieldType(0) == KUidContactFieldEMail) 
					iLabel = _L("Work email");
				else if (aContentType->FieldType(0) == KUidContactFieldPostOffice) 
					iLabel = _L("Work PO box");
				else if (aContentType->FieldType(0) == KUidContactFieldExtendedAddress) 
					iLabel = _L("Work ext address");
				else if (aContentType->FieldType(0) == KUidContactFieldAddress) 
					iLabel = _L("Work address");
				else if (aContentType->FieldType(0) == KUidContactFieldLocality) 
					iLabel = _L("Work city");
				else if (aContentType->FieldType(0) == KUidContactFieldRegion) 
					iLabel = _L("Work region");
				else if (aContentType->FieldType(0) == KUidContactFieldPostcode) 
					iLabel = _L("Work p'code");
				else if (aContentType->FieldType(0) == KUidContactFieldCountry) 
					iLabel = _L("Work country");
				}
			}
		else 
			{
			if (aContentType->FieldType(0) == KUidContactFieldPhoneNumber 
			&& aContentType->ContainsFieldType(KUidContactFieldVCardMapVOICE)
			&& aContentType->ContainsFieldType(KUidContactFieldVCardMapTEL))
				{
				iLabel = _L("Home tel");
				}
		
			}
		}	
	return iLabel;
	}


