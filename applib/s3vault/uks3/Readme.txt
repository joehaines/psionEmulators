Title:		UKsights
Version:	99b May 1999
Platform:	S3 and S5 (SIBO and EPOC)
Description:	Details of interesting places to visit in the British Isles
Terms:		Copyright Freeware



Hi,

These notes describe the 'UKsights' database file, which provides 
details of interesting places to visit in the British Isles. I never found a 
holiday book that consistently provided the details I needed on tourist 
attractions, so I produced this.

This version primarily contains enhancements to background information from
the previous version. 

'UKsights' consists of a single database file. Two versions are 
provided depending on which type of PDA it will be loaded onto.

The filenames are:

1.  UKsights		(EPOC version, e.g. Psion S5)

2.  Uksights.dbf	(SIBO version, e.g. Psion S3a/S3c/Siena etc)

The relevant file must be copied to the PDA without any conversion.
Typically using 'drag and drop' within the PsiWin application.
On the Psion S5, a data file may be placed almost anywhere, whilst on the
Psion S3 it should be copied into the \DAT\ directory on any disk.
The entries for the EPOC version can be sorted into a different order than
currently given, however, by default they are in geographical order:

1 London
2 Rest of England
3 Wales (Cymru)
4 Edinburgh
5 Rest of Scotland
6 Northern Ireland
7 Republic of Ireland (Eire)

The entries for England, Scotland, Wales and Northern Ireland are sorted
according to a generally anticlockwise tour. The Republic of Ireland is
still underrepresented, but that should be addressed in later releases.



The following is a list of the fields:

Geo:
An integer indicating geographical position. This field is hidden in the EPOC
version and non existent in the other version. After sorting on the 'Geo' 
field (default) records will be displayed in geographical order (See 
Appendix 1).

Chrono:
An integer best indicating the year associated with the sight, e.g., 
Shakespeare's  Birthplace '1564'. This field is hidden in the EPOC version 
and non existent in the other version. After sorting on the 'Chrono' field 
records will be displayed in chronological order. This gives a view of the 
historical development of the British Isles.

Name:
Name of the sight.

Description:
A brief description.

Address:
The geographical address.

Undergrnd:
The nearest underground station, where applicable, for sights in central 
London or Newcastle-upon-Tyne.

Notes:
General notes about the location.

Did u know:
Typically historical information and interesting facts to put things into 
context. Only available on the EPOC version.
Convertion of a large S5 memo field to any other type of fixed-size field 
(such as is used with the S3) is not particularly effective.

Tel:
Telephone number. 'R' after a number indicates a recorded message, 
usually interrogateable from a TouchTone telephone. To call a UK 
number from outside the UK, dial the international access code of your 
own country, followed by '44' for the UK, followed by the telephone 
number shown but with the initial '0' removed. For example, to call '01453 
123456' from the USA.  Dial '011 44 1453 123456'. Numbers for the Irish 
Republic are preceded with the '+' sign. The '+' indicates the international 
access code e.g. 00 in the UK, 011 in the USA.

Cost:
Unless stated otherwise, only the single adult entrance fee is given. Many 
cathedrals and museums allow a voluntary subscription, whilst some 
suggest a minimum contribution.

Open:
All opening times are in the 24hr format. They are as accurate as can be
established at present, however, details change frequently, and it would be 
advisable to ring the particular location before visiting, just to be certain. 
Most attractions are closed on obvious days such as Christmas and New 
Year. However, this has been omitted from the database to save space. 
The last admission time is shown after the closing time using the letters 
'LA' followed by the difference between last admission time and closing 
time. For example a closing time of 17.00 with a last admission time of 
16.00 would be shown as 17.00LA-01.00. There is a subtlety insofar as, if 
there is no space between the closing time and the 'LA' the last admission 
time applies only to the time to which it is juxtaposed, otherwise it applies 
to all the closing times preceding it in the field.
Unless stated otherwise, month ranges are inclusive, e.g., Mar-Oct denotes the
beginning of March until the end of October. A closing time followed by '/dusk'
means that closure will occur at the time shown or at dusk if that is earlier.

http://
A WWW home page address if available.


And that's about it.

For any comments, additions or general improvements, feel free to email me at:

gmulholland@geocities.com


The only other thing to mention is that I can't give any guarantees as to the 
accuracy of the data. Having said that, however, I've tried the best I can to 
maximise accuracy.

Also, if you fall out of a balloon, sink in a boat, get run over by a tour bus 
or if any harm, trouble or unpleasantness arises as a result of your reading 
or acting upon the information given; you have my sympathy but that's all. The
same conditions and degree of sympathy extend to the effects of accessing the
information on your PDA. I don't expect any problems to arise but I like to
cover myself.

Hope you find it useful.


Best regards,

Gerard Mulholland.





Appendix 1 - Database Contents List

~ Indicates changed records since previous release


	London
Heathrow Airport Observation Deck ~
Gatwick Airport Observation Deck
Palace of Westminster
Westminster Abbey & St Margaret's Church
Westminster Cathedral
Prime Minister's Residence
Banqueting Hse
Horse Guards' Parade Ground
Trooping The Colour
St James's (Royal) Pk
Cabinet War Rms / Museum
Guards Museum
Buckingham Palace
Royal Mews
The Queen's Gallery
St James's Palace
London Marathon ~
Memorial to Crimean War
Nelson's Column
National Gallery
St Martin-in-the-Fields
Charing Cross Rd
National Portrait Gallery ~
The Coliseum
Leicester Sq
Planet Hollywood
Pepsi Trocadero
Piccadilly Circus ~
Hamley's Toy Shop
Carnaby St
Soho
China Town ~
Museum of Mankind
Royal Academy of Arts
Spencer Hse ~
Royal Institution of Gt Britain
Christies Auction Hse
Green (Royal) Pk
Hard Rock Cafe
Wellington Museum ~
Royal Mint
National Army Museum
Chelsea Flower Show
Bungee Jumps
Harrods
Victoria & Albert Museum ~
Natural History Museum
Science Museum
Royal Albert Hall
Kensington Gdns. Extension of Hyde (Royal) Pk
Albert Memorial
Kensington Palace ~
Commonwealth Institute
Royal Tournament
University Boat Race
Notting Hill Carnival
Toy & Model Museum ~
Site of Tyburn Tree Gallows
Marble Arch
Speakers' Corner
Sotheby's Auction Hse
Wigmore Hall
Wallace Art Collection
Planetarium
Madame Tussaud's
Sherlock Holmes Museum
Saatchi Art Collection
Marylebone Cricket Club Museum ~
Regent's (Royal) Pk
London Zoo
Jewish Museum
Family Records Office
British Library Reading Rms
Science for Life Exhibition
BT Tower
BT Museum of Telecommunications ***CLOSED***
Pollock's Toy Museum
The BBC Experience
Oxford St
Covent Gdn
London Transport Museum
United Grand Masonic Lodge of England
Theatre Museum
Royal Opera Hse
British Museum
'Old' British Library Reading Rms
Dickens Hse Museum
Murder Mystery Dinner Theatre
Royal Courts of Justice ~
Doctor Johnson's Hse ~
Samuel Pepys Exhibition
St Paul's Cathedral
Museum of London
Barbican Ctr
Hse of Detention
National Postal Museum
Mansion Hse
Bank of England Museum ~
The Monument
Lloyds of London Building
The Guildhall
Lord Mayor's Show
Geffrye Museum
Tower of London
Tower Bridge Experience
Tower Hill Pageant ***CLOSED***
The Beefeater
London Pub Walks
Thames Sightseeing Cruises
Thames Night-time Cruise & Meal
Thames Lunch & Dinner Cruises
Cleopatra's Needle
Tate Gallery
Gt Balloon Experience
MI6 Building ~
Battersea Pk Zoo
Lambeth Palace
Imperial War Museum
Museum of Garden History
British Airways London Eye ~
Florence Nightingale Museum
London Aquarium
Royal Festival Hall ~
Hayward Gallery
National Film Theatre
Museum of The Moving Image
Royal National Theatre
Shakespeare's Globe Theatre & Museum
Southwark Cathedral
Golden Hinde ~
Clink Prison Museum
Design Museum
Bramah Tea & Coffee Museum
London Dungeon
Britain at War Experience
HMS (Her Majesty's Ship) Belfast ~
The Mayflower Public Hse
Canary Wharf Tower
National Maritime Museum
Queen's Hse
Old Royal Greenwich Observatory
Cutty Sark
Millennium Experience
Millennium Dome ~
Thames Barrier
The Guardian Interactive ~
Phone Guide to London
Telegraph Box Office
Adelphi Theatre
Albery Theatre
Aldwych Theatre
Apollo Theatre
Apollo Victoria Theatre
Labatt's Apollo Theatre
Arts Theatre
Cambridge Theatre
Comedy Theatre
Criterion Theatre
Dominion Theatre
Duchess Theatre
Fortune Theatre
Garrick Theatre
Gielgud Theatre
Haymarket Theatre
Her Majesty's Theatre
London Palladium
Lyceum Theatre
Lyric Theatre
New London Theatre
Old Vic Theatre
Open Air Theatre
Palace Theatre
Phoenix Theatre
Piccadilly Theatre
Playhouse Theatre
Prince Edward Theatre
Prince of Wales Theatre
Queen's Theatre
Royal Court Theatre
St Martin's Theatre
Savoy Theatre
Shaftesbury Theatre
Strand Theatre
Theatre Royal
Theatre Royal
Vaudeville Theatre
Victoria Palace Theatre
Whitehall Theatre
Wyndhams Theatre
The Original Bus Tour
Big Bus Tours
London Pride Bus Tours
London Discovery Walks
Jewish Military Museum
Alexandra Palace
Grave of Karl Marx
RAF Museum
Wembley Stadium
Public Record Office Museum
The Twickenham Experience & Museum
Hampton Court Palace
Home of Rest for Old Horses
Wimbledon Lawn Tennis Museum

	England
Blenheim Palace
Christ Church College & Cathedral
Oxford Story Exhibition
Garsington Opera
Waterperry Gdns
Diana Princess of Wales' Grave & Museum ~
Hughenden Manor
Bekonscot Model Village
Woburn Abbey
Woburn Safari Pk
Whipsnade Wild Animal Pk
Hatfield Hse
Old Palace Banquets
Fresh Tracks ~
King's College Chapel
Cambridge 'Guide Friday' Tours
Geoff's Bike Hire
Scudamore's Punt Hire
Cambridge University's Fitzwilliam Museum
University of Cambridge Botanic Gdns
American Military Cemetery
Mullard Radio Astronomy Observatory
Wimpole Hall
Duxford Air Museum
Peterborough Cathedral
Ely Cathedral ~
Oliver Cromwell's Hse
Lakeside
Nuclear Bunker
Old MacDonald's Educational Farm Pk
Essex Secret Bunker
Bradwell Nuclear Power Station
Colchester Castle
Colchester Zoo
Flatford Mill ~
Britain's National Stud ~
National Horse Racing Museum
RAF Mildenhall Annual NATO Airshow
Lavenham
Sutton Hoo ~
Staverton Gdns & Vineyard
Sizewell Visitor Ctr
Snape Maltings
Somerleyton Hall & Gdns
Pleasurewood Hills
Amazonia ~
Norwich Cathedral
Norfolk Broads Boat Hire
Carting Action Sports
Muckleburgh Collection
East Anglian Falconry Ctr
Holkham Hall & Bygones Museum
Sea Life Ctr
North Norfolk Steam Railway
Sandringham Estate
Grimes Graves & Thetford Forest
Donington Pk
Bosworth Battlefield & Visitor Ctr
British Gas Museum
River Soar Boat Hire
Stonehurst Farm
Rutland Cycle Hire
Rutland Water Cruises
Rutland Water Butterfly & Aquatic Ctr
Lincoln Cathedral
Rutland Open Air Theatre
World of Robin Hood
Chatsworth Hse
Bolsover Castle
Eureka
World of Honey Bee Bugs & Beasties
Royal Armouries
Thackray Medical Museum
Harewood Hse
National Museum of Photography Film & TV
Colour Museum
Brontë Parsonage Museum
Humber Bridge ~
John Bull World of Rock
Yorkshire Dales Falconry & Conservation Ctr
Yorkshire Air Museum ~
York Story Exhibition
York Dungeon
Guy Fawkes Night
Jorvik Viking Ctr
York Brewery Tour
Archaeological Resource Ctr
York Ghost Trail
York Minster
Yorkshire Museum
York Castle Museum
National Railway Museum
Boeing 737-200 Flight Simulator
Mother Shipton's Cave
Castle Howard
Eden Camp
Sea Life Ctr
Scarborough Castle
Millennium
Rievaulx Abbey
Lightwater Valley
Birds of Prey Ctr
Fountains Abbey & Studley Royal Gdns
Middleham Castle
Richmond Castle
North Yorkshire Moors Steam Railway
Wade's Causeway
Whitby Abbey
The Dracula Experience ~
Captain Cook's Birthplace ~
Captain Cook's Endeavour
Barnard Castle
Bowes Museum
Durham Cathedral
Marsden Rock & Grotto
Beamish
Metro Ctr
Theatre Royal
Tyne Theatre & Opera Hse
Discovery Museum
Lindisfarne Priory
Hadrian's Wall
Kielder Water & Forest Visitor Ctr
Sellafield (formerly Windscale) Visitor Ctr
Windermere
Lakeland Sheep & Wool Ctr
Amazonia
William Wordsworth's Birthplace
Castlerigg Stone Circle
Cars of The Stars Motor Museum
Blackpool Tower
Amazonia
The Magic of Coronation St
Sefton Coast ~
Her Majesty's Customs & Excise Museum
Granada Studios
Jodrell Bank
Chester Zoo
Alton Towers
Lichfield Cathedral
Aerospace Museum
Sea Life Ctr
Cadbury World Visitor Ctr
Witley Court
Hereford Cathedral
Warwick Castle
Coventry Cathedral
Twycross Zoo
Coughton Court
Ragley
Shakespeare's Birthplace
Stratford 'Guide Friday' Tours
Royal Shakespeare Theatre
The Shakespeare Walk
Heritage Motor Ctr ~
SS (Steam Ship) Gt Britain ~
The Exploratory
Bristol Cathedral
Clifton Suspension Bridge
Bristol Zoo Gdns
Roman Baths Pump Rm & Museum
International Helicopter Museum
Cheddar
Cheddar Cheese Co
Moorlynch Vineyard
Wookey Hole
Wells Cathedral
Glastonbury Abbey & Tor (Hill)
Haynes Motor Museum ~
Fleet Air Arm Museum
Dartmoor Nat Pk & Moorland Visitor Ctr
Buckfast Abbey
Plymouth Dome
Total Solar Eclipse in Devon & Cornwall ~
Bodmin Moor
King Arthur's Castle
Tate Gallery
Land's End
Minack Amphitheatre & Exhibition Ctr
St Michael's Mount
Goonhilly Earth Station ~
Glendurgan
Lost Gdns of Heligan
Stone Age Giant
Portland 'Castle'
Tank Museum
Longleat Estate
Stonehenge ~
Avebury Stone Circles
Salisbury Cathedral
Wilton Hse
Legoland
Windsor Castle
Savill Gdn
Frogmore Hse Gdns & Mausoleum
'New' Forest
Beaulieu Hse
Exbury Gdns
Highclere Castle & Gdns ~
Winchester Cathedral
Marwell Zoological Pk
Jane Austen's Hse & Gdns
Stratfield Saye Hse
Submarine World
Portchester Castle
D-Day Museum ~
Historic Ships Exhibition
The Needles
Gt Thorpe Pk
Kew Gdns
Gatwick Zoo
Royal Horticultural Society's Gdn
Sussex Falconry Ctr
Fishbourne Roman Palace
Chichester Cathedral
Dolls' Hse Museum
Petworth Hse & Pk
Bignor Roman Villa
Pulborough Brooks Nature Reserve
Southdown Gliding Club
Arundel Castle
The Body Shop Tour
Leonardslee Gdns
Deers Leap Pk
Bluebell Railway
Sea Life Ctr
Paradise
Beachy Head & Exhibition Ctr
Herstmonceux Science Ctr
Battle Abbey
Hever Castle
Chartwell Hse
Knole
Penshurst Place & Gdns
Groombridge Place
Lamberhurst Vineyards
Lullingstone Castle
Lullingstone Roman Villa
Rochester Castle
Chatham
Museum of Kent Life
Leeds Castle
Dungeness Nuclear Power Stations
Old Lighthouse
U-475 Russian Submarine
Channel Tunnel
White Cliffs Experience
Dover Castle
Walmer 'Castle' & Gdns
Deal 'Castle'
Canterbury Cathedral
Canterbury Tales
Claremont Landscape Gdn
Chessington World of Adventures
Balloon Flights
Virgin Balloon Flights
Center Parcs
St George's Day ~

	Wales / Cymru
Farm World
Motor Safari
Doctor Who Experience & Model Railway World
Llangollen Annual International Musical Eisteddfod
Llangollen Steam Railway
Bunny Farm
Afonwen Craft & Antique Ctr
Bodelwyddan Castle
Harlequin Puppet Theatre
Alice In Wonderland Ctr
Conwy Castle
Bangor Museum & Art Gallery
Llanfairpwllgwyngyllgogerychwyrndrobwllllantysiliogogogoch
Museum of Childhood
Stone Science
Wylfa Visitor Ctr
Bird World
Anglesey Sea Zoo
Caernarfon Castle
Segontium Roman Museum
Caernarfon Air World
Welsh Slate Mine Museum
Dinorwic Discovery (Electric Mtn)
Llanberis Pass
Snowdon Mtn & Railway
Pathfinder Mtn Activities
Bodvel Hall
Glasfryn Activity Pk
Lloyd George Museum & Childhood Home
Sygun Copper Mine
Llechwedd Slate Caverns
Harlech Castle
Welsh Gold
King Arthur's Labyrinth
Celtica
Borth Animalarium
Coastal Voyages
Dolaucothi Gold Mines
Museum of Welsh Woollen Industry
Portmeirion
Cilgerran Gorge Canoeing
Pembroke Castle
Carmarthen Museum
Dan-yr-Ogof Showcaves
Welsh Whisky Visitor Ctr ~
Caerphilly Castle
Llanerch Vineyard
Museum of Welsh Life
Turner Hse Gallery
Cardiff Castle
Welsh Industrial & Maritime Museum
Techniquest
National Museum & Gallery
Caerleon Roman Legionary Museum

	Scotland
Floors Castle ~
Dawyck Botanic Gdn
Museum of Flight

	Edinburgh
Royal Yacht Britannia
Edinburgh Castle
Edinburgh Tattoo
Camera Obscura ~
The Royal Mile
'Supper With Burns'
The Robert Burns Tour
Museum of Childhood
St Giles' Cathedral ~
Scotch Whisky Heritage Ctr
The Witchery Tour
Ghost & Torture Tour
Underground City Tour
Confession By Torture Tour
Palace of Holyroodhouse
Edinburgh 'Guide Friday' Tours
Hard Rock Cafe
Bank of Scotland Museum
Royal Museum
Alexander Graham Bell's Birthplace ~
Scottish Telecom World of Communications
National Gallery of Scotland
Scottish National Portrait Gallery
Scottish National Gallery of Modern Art
Edinburgh Festivals
Hogmany
The Edinburgh Experience
Princes St
Edinburgh Zoo
Family Records Office
Royal Botanic Gdn
Royal Commonwealth Pool
Arthur's Seat
Hopetoun Hse
Linlithgow Palace
Bannockburn Heritage Ctr
Stirling Castle
Castle Campbell
Deep Sea World
St Andrews Castle
Scone Palace
Drummond Castle Gdns ~
Glamis Castle ~
Pitlochry Power Station Visitor Ctr
Queen's View
Killiecrankie Visitor Ctr
Fasque
Braemar Castle
Balmoral Castle
Grampian Transport Museum
The Gordon Highlanders Museum
Loanhead of Daviot Stone Circle
Archaeolink
Castle Lighthouse & Museum
Duff Hse
Strathisla Distillery
Glenfiddich Distillery
Craigellachie Bridge
Elgin Cathedral
Biblical Gdn
Ron Parker Sculpture
Cawdor Castle
Fort George
Culloden Moor & Visitor Ctr
Inverness Annual Highland Games
Aviemore
Loch Morlich Watersports
Waltzing Waters
Loch Ness
Cluanie Pk
Caithness Crystal Visitor Ctr
Duncansby Stacks
John O' Groats
Orkney Boat & Coach Tours
Maes Howe
Dounreay Visitor Ctr
HMS Vulcan Naval Base
Coastal Cruises
Inverewe Gdns
Skye Bridge ~
Isle of Lewis Ferry Trip
Calanais Standing Stones & Visitor Ctr
The Jacobite Stream Train
Highland Mysteryworld
Glencoe Visitor Ctr
Cruachan Hydro-Electric Power Station
Younger Botanic Gdn Benmore
Glasgow Cathedral
Burns' Cottage & Museum
Culzean Castle & Country Pk
Castle Kennedy Gdns
Logan Botanic Gdns

	Northern Ireland
South Down
The Flagstaff ~
Silent Valley
Spelga Dam & Reservoir
Tollymore Forest Pk
Castlewellan Forest Pk
Legananny Dolmen ~
Down Cathedral
Strangford Lough & Ferry
Exploris Aquarium ~
Seaforde Butterfly Hse
Mount Stewart ~
Ulster Folk & Transport Museums ~
Her Majesty's Prison Maze
Glens of Antrim
Carrick-A-Rede
Giant's Causeway & Visitor Ctr ~
Castle Coole
Lough Erne Cruises
Lough Neagh Discovery Ctr
Navan Hill Fort & Visitor Ctr
Armagh Planetarium
Gosford Castle Forest Pk ~
Orangemen's Day ~
UK National Weather Report

	Republic of Ireland / Éire
New Grange
Heywood Gdns
Clonmacnoise
Trinity College
Dublin Castle ~
St Mary's Abbey
Historical Walking Tours of Dublin
Dublin City Bus Tour
Dublin Grand Bus Tour
St Stephen's Green ~
Garden of Remembrance
Dublin's Viking Adventure
Dublin Writers' Museum ~
Kilmainham Jail
War Memorial Gdns
The Shaw Birthplace
National Botanic Gdns
James (Augustine Aloysius) Joyce Museum
St Enda's Pk
Castletown Hse
Glendalough
John F. Kennedy Arboretum ~
Irish National Heritage Pk
New Ross River Cruising Restaurant
Waterford River Cruising Restaurant
Waterford Crystal Visitor Ctr
Dunmore Cave
Kilkenny Castle
Jerpoint Abbey
Wreck of French Armada
Crag Cave
Bunratty Castle & Folk Pk
Kilrush Exhibition ~
Cliffs of Moher
The Burren
Aillwee Cave
Conemarra ~
Grave of W B (William Butler) Yeats
Co Donegal
Silver League
St Patrick's Day ~





Appendix 2 - Database Abbreviations

~		new/updated record since previous version
AD		anno domini ('after birth of Christ')
aka		also known as
am,pm		ante,post-meridiem
approx		approximately
AV		audio-visual (presentation)
Ave		Avenue
BC		before Christ
BH		Bank (public) Holiday
Co		County
ctr		centre
Eas		Easter
esp		especially
ft		feet (3 ft = 1 yd)
gal		gallon (1 gal = 4.55 litres)
gdn		garden
gt		great
HM		Her Majesty
HP		horsepower
HQ		headquarters
HRH		His/Her Royal Highness
hr,min,sec	hour,minute,second
Hse		house
inc,exc		including,excluding
IP		Internet protocol family
Jan-Dec		January-December
Jnt		road junction
LA		last admission
lt		late
m		metre/million
mkt		market
ml		mile (1 ml = 1.6 km)
Mo-Su		Monday-Sunday
MP		Member of Parliament
ms		millisecond
Mtn		Mountain
Nat		National
no		number
OAP		old-age pensioner
Pk		Park
PR		public relations
R		(after tel no) Recorded message
RAF		Royal Air Force
Rd		Road
ret		return (fare)
rm		room
Sq		Square
St		Street/Saint
Sum,Win		Summer,Winter
tel		telephone
Term		Terminal
typ		typically
URL		Uniform Resource Locator
USAF		United States Air Force
usu		usually
wk,mth,yr	week,month,year
WW1,2		World War 1,2
yd		yard (1 yd = 0.9144 metres)
>		greater than
'		foot/minute of arc
"		inch (1" = 2.54cm)




