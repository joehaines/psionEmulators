#include "meeting.h"

#include <qmessagebox.h>
#include <qpushbutton.h>

#include <klocale.h>
#include <kmessagebox.h>

#include "defines.h"
#include "mp_option.h"

#define LIST_INTERVAL 0


NetMeeting::NetMeeting(const cId &_id, Socket *socket,
                       MPOptionWidget *option,
					   bool _server, QWidget *parent, const char * name)
: KDialogBase(Plain, i18n("Network Meeting"),
			  (_server ? Ok|Cancel|Help : Cancel|Help),
			  (_server ? Ok : Cancel), parent, name),
  server(_server), ow(option), id(_id), socketRemoved(FALSE)
{
	sm.append(socket, SocketManager::ReadWrite);
	sm[0]->notifier()->setEnabled(TRUE);

/* top layout */
	QVBoxLayout *top = new QVBoxLayout(plainPage(), spacingHint());
	top->setResizeMode(QLayout::Fixed);

	// server line
	spl = new MeetingLine(server, server, true, plainPage());
	top->addWidget(spl);

	// widget list
	wl = new WidgetList<MeetingLine>(LIST_INTERVAL, plainPage());
	wl->hide();
	top->addWidget(wl);

	labWait = new QLabel(i18n("Waiting for clients"), plainPage());
	labWait->setAlignment(AlignCenter);
	top->addWidget(labWait);

	// options widget
//	if (ow)	top->addWidget(ow); #### FIXME

    // status bar
	status = new QStatusBar(plainPage());
	status->setSizeGripEnabled(false);
	top->addWidget(status);

	// buttons
	enableButtonSeparator(TRUE);
	if (server) {
		setButtonOKText(i18n("Start Game"));
		enableButtonOK(FALSE);
	}
	setButtonCancelText(server ? i18n("Abort") : i18n("Quit"));
	enableButton(Help, FALSE);
}

NetMeeting::~NetMeeting()
{}

void NetMeeting::appendLine(const MeetingLineData &pld, bool server)
{
	MeetingLine *pl;
	pl = new MeetingLine(pld.own, server, false, wl);
	if (pld.own) connect(pl, SIGNAL(textChanged(const QString &)),
						 SLOT(textChanged(const QString &)));
	else message(i18n("A new client has just arrived (#%1)")
				 .arg(wl->size()+1));
	pl->setData(pld.ed);
	connect(pl, SIGNAL(typeChanged(MeetingCheckBox::Type)),
			SLOT(typeChanged(MeetingCheckBox::Type)));
	wl->append(pl);
	waiting();
}

void NetMeeting::removeLine(uint i)
{
	wl->remove(i);
	waiting();
}

void NetMeeting::waiting()
{
	if ( wl->size() ) {
		labWait->hide();
		wl->show();
	} else {
		labWait->show();
		wl->hide();
	}
	if (server) enableButtonOK(ready());
}

void NetMeeting::setType(const TypeInfo &ti)
{
	if ( ti.i==0 ) spl->setType(ti.type); // in fact should not append
	else {
		wl->widget(ti.i-1)->setType(ti.type);
		if (server) enableButtonOK(ready());
	}
}

void NetMeeting::setText(const TextInfo &ti)
{
	if ( ti.i==0 ) spl->setText(ti.text);
	else wl->widget(ti.i-1)->setText(ti.text);
}

bool NetMeeting::ready() const
{
	int nbReady = 0;
	for(uint k=0; k<wl->size(); k++) {
		switch ( wl->widget(k)->type() ) {
		 case MeetingCheckBox::Ready    : nbReady++; break;
		 case MeetingCheckBox::NotReady : return FALSE;
		 default : break;
		}
	}
	return ( nbReady!=0 );
}

void NetMeeting::cleanReject(const QString &str)
{
	sm.clean(); // remove the sockets immediately to avoid possible further mess
	if ( !str.isEmpty() )
		KMessageBox::information(this, str, caption());
	KDialogBase::reject();
}

#define WRITE(i)      if ( !sm[i]->write() ) { writeError(i); return; }
#define CHECK_READ(i) if ( !sm[i]->readingStream().readOk() ) { dataError(i);  return; }

// Read incoming data
void NetMeeting::readNotifier(int fd)
{
	int i = sm.find(fd);
    Q_ASSERT( i!=-1 );
	switch ( sm[i]->read() ) {
	 case -1: readError(i);  break;
	 case  0: brokeError(i); break;
	 default: readData(i);
	}
}

void NetMeeting::readData(uint i)
{
	// get message type
	MeetingMsgFlag mt;
	sm[i]->readingStream() >> mt;
	CHECK_READ(i);
	switch (mt) {
	 case EndFlag:      endFlag(i);     break;
	 case NewFlag:      newFlag(i);     break;
	 case Mod_TextFlag: modTextFlag(i); break;
	 case Mod_TypeFlag: modTypeFlag(i); break;
	 case IdFlag:       idFlag(i);      break;
	 case DelFlag:      delFlag(i);     break;
	 case Mod_OptFlag:  modOptFlag(i);  break;
	 case PlayFlag:     playFlag(i);    break;
	 default:       dataError(i);
	}

	if (socketRemoved) socketRemoved = FALSE;
	else if ( !sm[i]->readingStream().atEnd() )
		readData(i); // more pending data
}

void NetMeeting::readError(uint i)
    { netError(i, i18n("Error reading data from")); }
void NetMeeting::dataError(uint i)
    { netError(i, i18n("Unknown data from")); }
void NetMeeting::writeError(uint i)
    { netError(i, i18n("Error writing to")); }
void NetMeeting::brokeError(uint i)
    { netError(i, i18n("Link broken or empty data from")); }

bool NetMeeting::checkState(uint i, PlayerState s)
{
	bool ok = ( players[i]==s );
	if (!ok) dataError(i);
	return ok;
}

bool NetMeeting::checkAndSetState(uint i, PlayerState os, PlayerState ns)
{
	bool ok = checkState(i, os);
	if (ok) players[i] = ns;
	return ok;
}

void NetMeeting::reject()
{
	// send an End flag
	sm.commonWritingStream() << EndFlag;
	writeToAll();

	cleanReject();
}

void NetMeeting::accept()
{
    KDialogBase::accept();
}

void NetMeeting::message(const QString &str)
{
	status->message(str, 3000);
}

/** ServerNetMeeting *********************************************************/
ServerNetMeeting::ServerNetMeeting(const cId &id,
	const RemoteHostData &r, MPOptionWidget *option,
	QPtrList<RemoteHostData> &arhd, QWidget *parent, const char * name)
: NetMeeting(id, r.socket, option, TRUE, parent, name), rhd(arhd)
{
	connect(sm[0]->notifier(), SIGNAL(activated(int)), SLOT(newHost(int)));
	players.append(Accepted); // server

	// set server line
	ExtData ed(r.bds, "", MeetingCheckBox::Ready);
	spl->setData(ed);
	connect(spl, SIGNAL(textChanged(const QString &)),
			SLOT(textChanged(const QString &)));

	// options signal
	if (ow) connect(ow, SIGNAL(changed()), SLOT(optionsChanged()));
}

void ServerNetMeeting::writeToAll(uint i)
{
	for (uint k=1; k<sm.size(); k++) {
		if ( k==i ) continue;
		if ( !sm.writeCommon(k) ) writeError(k);
	}
	sm.commonWritingStream().clear();
}

void ServerNetMeeting::netError(uint i, const QString &type)
{
	Q_ASSERT( i!=0 );
	disconnectHost(i, i18n("%1 client #%2: disconnect it").arg(type).arg(i));
}

void ServerNetMeeting::disconnectHost(uint i, const QString &str)
{
	sm.remove(i, true);
	socketRemoved = TRUE;
	if ( players[i]==Accepted ) {
		removeLine(i-1);

		// Send a Del message to all (other) clients
		sm.commonWritingStream() << DelFlag << i;
		writeToAll();
	}
	players.remove(players.at(i));
	message(str);
}

void ServerNetMeeting::newHost(int)
{
    KExtendedSocket *s;
    int res = sm[0]->accept(s);
    if ( res!=0 ) {
        message(i18n("Failed to accept incoming client:\n%1")
                .arg(socketError(s)));
        return;
    }
	players.append(NewPlayer);
    Socket *socket = new Socket(s, true);
	uint i = sm.append(socket, SocketManager::ReadWrite);
	connect(sm[i]->notifier(), SIGNAL(activated(int)),
            SLOT(readNotifier(int)));
	sm[i]->notifier()->setEnabled(TRUE);
}

void ServerNetMeeting::idFlag(uint i)
{
	bool b = checkAndSetState(i, NewPlayer, IdChecked);
	Q_ASSERT(b);

	// get client id
	cId clientId;
	sm[i]->readingStream() >> clientId;
	CHECK_READ(i);

	// compare id
	id.check(clientId);

	// send result to client
	Stream &s = sm[i]->writingStream();
	s << IdFlag << id;
	WRITE(i);

	// if not accepted : remove socket and player from list
	if ( !id.accepted() )
        disconnectHost(i, i18n("Client rejected for incompatible ID"));
}

void ServerNetMeeting::endFlag(uint i)
{
	disconnectHost(i, i18n("Client #%1 has left").arg(i));
}

void ServerNetMeeting::newFlag(uint i)
{
	checkAndSetState(i, IdChecked, Accepted);

	// get line infos from new client (GameData struct)
	MeetingLineData pld;
	sm[i]->readingStream() >> pld.ed.bds;
	CHECK_READ(i);

	// complete the MeetingLineData struct with initial values
	pld.own     = FALSE;                 // client line
	pld.ed.type = MeetingCheckBox::NotReady; // not ready by default
    pld.ed.text = "";                    // empty line to begin with
	appendLine(pld, TRUE);

	// send to the new client already present lines including its own
	// (New flag + MeetingLineData struct)
	spl->data(pld.ed);
	sm[i]->writingStream() << NewFlag << pld.ed;
	for(uint k=1; k<sm.size(); k++) {
		wl->widget(k-1)->data(pld.ed);
		pld.own = ( k==i );
		sm[i]->writingStream() << NewFlag << pld;
	}
	WRITE(i);

	// send to all other clients the new line (New flag + MeetingLineData struct)
	wl->widget(i-1)->data(pld.ed);
	pld.own = FALSE;
	sm.commonWritingStream() << NewFlag << pld;
	writeToAll(i);
}

void ServerNetMeeting::modTextFlag(uint i)
{
	checkState(i-1, Accepted);

	// the client i has just sent a new text (QString)
	TextInfo ti;
	sm[i]->readingStream() >> ti.text;
	CHECK_READ(i);
	ti.i = i;
	setText(ti);

	// send it to all other clients (Mod_Text flag + TextInfo struct)
	sm.commonWritingStream() << Mod_TextFlag << ti;
	writeToAll(i);
}

void ServerNetMeeting::modTypeFlag(uint i)
{
	checkState(i-1, Accepted);

	// a client has just sent a new TCB type (TCB type)
	TypeInfo ti;
	sm[i]->readingStream() >> ti.type;
	CHECK_READ(i);
	ti.i = i;
	setType(ti);

	// send it to all other clients (Mod_Type flag + TypeInfo struct)
	sm.commonWritingStream() << Mod_TypeFlag << ti;
	writeToAll(i);
}

void ServerNetMeeting::textChanged(const QString &text)
{
	// server line text changed : send to every clients (Mod_Text flag + TextInfo struct)
	TextInfo ti; ti.i = 0; ti.text = text;
	sm.commonWritingStream() << Mod_TextFlag << ti;
	writeToAll();
}

void ServerNetMeeting::typeChanged(MeetingCheckBox::Type type)
{
	Q_ASSERT( sender()!=spl ); // server TCB not modifiable
	// the server has changed a client TCB

	// find the changed TCB index
	TypeInfo ty;
	ty.type = type;
	for (ty.i=0; ty.i<wl->size(); ty.i++)
		if ( sender()==wl->widget(ty.i) ) break;
	ty.i++;

	// TCB change : send to every clients (Mod_Type flag + TypeInfo struct)
	sm.commonWritingStream() << Mod_TypeFlag << ty;
	writeToAll();
	if (server) enableButtonOK(ready());
}

void ServerNetMeeting::accept()
{
	Q_ASSERT( ready() && rhd.count()==0 );

	// stop receiving data from clients (will be buffered by OS)
	for (uint k=0; k<sm.size(); k++) disconnect(sm[k]->notifier());
	sm.remove(0, true);

	// check which client will play and fill RemoteHostData array
	ExtData ed;
	bool willPlay;
	for (uint k=1; k<players.count(); k++) {
		willPlay = FALSE;

		if ( players[k]==Accepted ) { // client with lines
				wl->widget(k-1)->data(ed);
			if ( ed.type==MeetingCheckBox::Ready ) {
				willPlay  = TRUE;
				RemoteHostData *r = new RemoteHostData;
				r->socket = sm[0];
				r->bds    = ed.bds;
				rhd.append(r);
			}

			// send play message to client (Play flag
			// + bool [accepted/rejected])
			sm[0]->writingStream() << PlayFlag << (Q_UINT8)willPlay;
			// if write failed and the client is not playing : silently
			// put it aside ...
			if ( !sm[0]->write() && willPlay ) {
				cleanReject(i18n("Unable to write to client #%1 at game "
                                 "beginning."));
				return;
			}
		}

        sm[0]->notifier()->setEnabled(false);
		sm.remove(0, !willPlay);
	}

	NetMeeting::accept();
}

void ServerNetMeeting::optionsChanged()
{
	sm.commonWritingStream() << Mod_OptFlag;
	ow->dataOut( sm.commonWritingStream() );
	writeToAll();
}

/** ClientNetMeeting *********************************************************/
ClientNetMeeting::ClientNetMeeting(const cId &id,
		const RemoteHostData &rhd, MPOptionWidget *option,
	    QWidget *parent, const char * name)
: NetMeeting(id, rhd.socket, option, FALSE, parent, name), bds(rhd.bds)
{
	connect(sm[0]->notifier(), SIGNAL(activated(int)),
			SLOT(readNotifier(int)));
	players.append(NewPlayer); // server player

	// Send id to server (Id flag + Id struct)
	sm.commonWritingStream() << IdFlag << id;
	writeToAll(); // what happens if there is a message box appearing before exec() call ??
}

void ClientNetMeeting::netError(uint, const QString &str)
{
	cleanReject(i18n("%1 server: aborting connection.").arg(str));
}

void ClientNetMeeting::writeToAll(uint)
{
	if ( !sm.writeCommon(0) ) writeError(0);
	sm.commonWritingStream().clear();
}

void ClientNetMeeting::idFlag(uint)
{
	checkAndSetState(0, NewPlayer, IdChecked);

	// read Id result (Id flag + Id struct)
	cId serverId;
	sm[0]->readingStream() >> serverId;
	CHECK_READ(0);

	// check result
	if ( !serverId.accepted() ) cleanReject(serverId.errorMessage(id));
	else {
		// send client info (New flag + GameData struct)
		sm.commonWritingStream() << NewFlag << bds;
		writeToAll();
	}
}

void ClientNetMeeting::newFlag(uint)
{
	if ( players[0]==IdChecked ) {
		ExtData ed;
		sm[0]->readingStream() >> ed;
		spl->setData(ed);
		players[0] = Accepted;
	} else {
		MeetingLineData pld;
		sm[0]->readingStream() >> pld;
		appendLine(pld, FALSE);
	}
	CHECK_READ(0);
}

void ClientNetMeeting::modTextFlag(uint)
{
	// receive new text from server (TextInfo struct)
	TextInfo ti;
	sm[0]->readingStream() >> ti;
	CHECK_READ(0);
	setText(ti);
}

void ClientNetMeeting::modTypeFlag(uint)
{
	// receive new type from server (TypeInfo struct)
	TypeInfo ti;
	sm[0]->readingStream() >> ti;
	CHECK_READ(0);
	setType(ti);
}

void ClientNetMeeting::delFlag(uint)
{
	// receive client number (uint)
	uint k;
	sm[0]->readingStream() >> k;
	CHECK_READ(0);
	removeLine(k-1);
	message(i18n("Client %1 has left").arg(k));
}

void ClientNetMeeting::textChanged(const QString &text)
{
	// text changed : send to server (Mod_Text flag + QString)
	sm.commonWritingStream() << Mod_TextFlag << text;
	writeToAll();
}

void ClientNetMeeting::typeChanged(MeetingCheckBox::Type type)
{
	// type changed : send to server (Mod_Type flag + TCB)
	sm.commonWritingStream() << Mod_TypeFlag << type;
	writeToAll();
}

void ClientNetMeeting::playFlag(uint)
{
	// receive accept or reject (bool)
	Q_UINT8 i;
	sm[0]->readingStream() >> i;
	CHECK_READ(0);
    sm[0]->notifier()->setEnabled(false);
    sm.remove(0, i==0);
    socketRemoved = true;
	if (i) accept();
	else cleanReject(i18n("The game has begun without you\n"
                          "(You have been excluded by the server)."));
}

void ClientNetMeeting::modOptFlag(uint)
{
	// read new option data
	ow->dataIn( sm[0]->readingStream() );
	CHECK_READ(0);
}

void ClientNetMeeting::endFlag(uint)
{
	// abort from server
	cleanReject(i18n("The server has aborted the game."));
}
#include "meeting.moc"
