
#ifndef CONTACTSYNCDEF
#define CONTACTSYNCDEF


const int KMaxContactDataSize = 1000;
const int KMaxFieldSize = 100;
const int KMaxLabelSize = 100;


#ifndef _UNICODE
	


// These have ben lifted from the ER6 SDK, obviously their
// addition to the ER5 OS postdates the release of the SDK
#define KUidContactFieldPostOfficeValue				0x10004DF4
#define KUidContactFieldExtendedAddressValue		0x10004DF5
#define KUidContactFieldLocalityValue				0x10004DF6
#define KUidContactFieldRegionValue					0x10004DF7
#define KUidContactFieldPostCodeValue				0x10004DF8
#define KUidContactFieldCountryValue				0x10004DF9

#define KUidContactFieldJobTitleValue				0x10009398

#define KIntContactFieldVCardMapPOSTOFFICE			0x10004DEA
#define KIntContactFieldVCardMapEXTENDEDADR			0x10004DEB
#define KIntContactFieldVCardMapLOCALITY			0x10004DEC
#define KIntContactFieldVCardMapREGION				0x10004DED
#define KIntContactFieldVCardMapPOSTCODE			0x10004DEE
#define KIntContactFieldVCardMapCOUNTRY				0x10004DEF


const TUid KUidContactFieldPostOffice={KUidContactFieldPostOfficeValue};
const TUid KUidContactFieldExtendedAddress={KUidContactFieldExtendedAddressValue};
const TUid KUidContactFieldLocality={KUidContactFieldLocalityValue};
const TUid KUidContactFieldRegion={KUidContactFieldRegionValue};
const TUid KUidContactFieldPostcode={KUidContactFieldPostCodeValue};
const TUid KUidContactFieldCountry={KUidContactFieldCountryValue};

const TUid KUidContactFieldJobTitle={KUidContactFieldJobTitleValue};

const TUid KUidContactFieldVCardMapPOSTOFFICE={KIntContactFieldVCardMapPOSTOFFICE};
const TUid KUidContactFieldVCardMapEXTENDEDADR={KIntContactFieldVCardMapEXTENDEDADR};
const TUid KUidContactFieldVCardMapLOCALITY={KIntContactFieldVCardMapLOCALITY};
const TUid KUidContactFieldVCardMapREGION={KIntContactFieldVCardMapREGION};
const TUid KUidContactFieldVCardMapPOSTCODE={KIntContactFieldVCardMapPOSTCODE};
const TUid KUidContactFieldVCardMapCOUNTRY={KIntContactFieldVCardMapCOUNTRY};

#endif



#endif