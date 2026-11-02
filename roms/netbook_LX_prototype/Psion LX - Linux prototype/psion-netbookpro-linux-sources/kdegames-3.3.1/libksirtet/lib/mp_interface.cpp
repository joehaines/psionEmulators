#include "mp_interface.h"
#include "mp_interface.moc"

#include <qpainter.h>
#include <qlayout.h>

#include <klocale.h>
#include <kmessagebox.h>
#include <kapplication.h>
#include <kkeydialog.h>

#include "defines.h"
#include "types.h"
#include "meeting.h"
#include "internal.h"
#include "keys.h"
#include "wizard.h"

/*****************************************************************************/
/* Constructor & Destructor                                                  */
/*****************************************************************************/
MPInterface::MPInterface(const MPGameInfo &_gameInfo,
                         uint nbActions, const ActionData *data,
						 QWidget *parent, const char *name)
: QWidget(parent, name), internal(0), gameInfo(_gameInfo), nbLocalHumans(0)
{
	Q_ASSERT( gameInfo.maxNbLocalPlayers>=1 );

    hbl = new QHBoxLayout(this, 0, 5);
	hbl->setResizeMode( QLayout::Fixed );

    _keyData = new KeyData(gameInfo.maxNbLocalPlayers, nbActions, data, this);
}

MPInterface::~MPInterface()
{
	delete internal;
}

/*****************************************************************************/
/* Game creation                                                             */
/*****************************************************************************/
void MPInterface::clear()
{
	if (internal) {
		stop();
		delete internal;
		internal = 0;
        _keyData->clear();
	}
}

void MPInterface::dialog()
{
	clear();

	// configuration wizard
	ConnectionData cd;
	MPWizard wiz(gameInfo, cd, this);
	if ( wiz.exec()==QDialog::Rejected ) {
		singleHuman(); // create a single game
		return;
	}

	// net meeting
	QPtrList<RemoteHostData> rhd;
	rhd.setAutoDelete(TRUE);
	if (cd.network) {
		cId id(kapp->name(), gameInfo.gameId);
		MPOptionWidget *ow = newOptionWidget();
		NetMeeting *nm;
		if (cd.server) nm = new ServerNetMeeting(id, cd.rhd, ow, rhd, this);
		else nm = new ClientNetMeeting(id, cd.rhd, ow, this);
		int res = nm->exec();
		if (ow) {
			if (res) ow->saveData();
			delete ow;
		}
		delete nm;
		if (!res) {
			singleHuman();
			return;
		}
	}

	createLocalGame(cd);
	if (cd.server) createServerGame(rhd);
	else createClientGame(cd.rhd);
}

void MPInterface::specialLocalGame(uint nbHumans, uint nbAIs)
{
	clear();

	ConnectionData cd;
	BoardData bd;
	PlayerComboBox::Type t;
    KConfigGroupSaver cg(kapp->config(), MP_GROUP);
	for (uint i=0; i<(nbHumans+nbAIs); i++) {
		bd.type = (i<nbHumans ? PlayerComboBox::Human : PlayerComboBox::AI);
		bd.name = QString::null;
		t = (PlayerComboBox::Type)
			cg.config()->readNumEntry(QString(MP_PLAYER_TYPE).arg(i),
						   	          PlayerComboBox::None);
		if ( bd.type==t )
			bd.name = cg.config()->readEntry(QString(MP_PLAYER_NAME).arg(i),
									         QString::null);
		if ( bd.name.isNull() )
			bd.name = (i<nbHumans ? i18n("Human %1").arg(i+1)
					   : i18n("AI %1").arg(i-nbHumans+1));
		cd.rhd.bds += bd;
	}
	cd.server  = TRUE;
	cd.network = FALSE;
	Q_ASSERT( (nbHumans+nbAIs)<=gameInfo.maxNbLocalPlayers );
	Q_ASSERT( gameInfo.AIAllowed || nbAIs==0 );

	createLocalGame(cd);
	QPtrList<RemoteHostData> rhd;
	createServerGame(rhd);
}

void MPInterface::createServerGame(const QPtrList<RemoteHostData> &rhd)
{
	internal = (rhd.count()
		 ? (Local *)new NetworkServer(this, boards, rhd, gameInfo.interval)
		 : (Local *)new LocalServer(this, boards, gameInfo.interval));
	init();
}

void MPInterface::createClientGame(const RemoteHostData &rhd)
{
	QPtrList<RemoteHostData> r;
	r.append((RemoteHostData *)&rhd);
	internal = new Client(this, boards, r);
	init();
}

void MPInterface::createLocalGame(const ConnectionData &cd)
{
	_server = cd.server;
	nbLocalHumans = 0;
	for (uint i=0; i<cd.rhd.bds.count(); i++)
		if ( cd.rhd.bds[i].type==PlayerComboBox::Human ) nbLocalHumans++;

	// add or remove boards
	uint old_s = boards.count();
	uint new_s = cd.rhd.bds.count();
	for (uint i=new_s; i<old_s; i++) {
		delete boards[i].ptr;
		boards.remove(boards.at(i));
	}
	Data d;
	for(uint i=old_s; i<new_s; i++) {
		d.ptr = newBoard(i);
		hbl->addWidget(d.ptr);
		d.ptr->show();
		connect(d.ptr, SIGNAL(enableKeys(bool)), SLOT(enableKeys(bool)));
		boards += d;
	}

	// init local boards
    _keyData->setCurrentNb(nbLocalHumans);
	int k = 0;
	for (uint i=0; i<boards.count(); i++) {
		bool h = ( cd.rhd.bds[i].type==PlayerComboBox::Human );
		boards[i].humanIndex = (h ? k : -1);
		if (h) {
            _keyData->createActionCollection(k, boards[i].ptr);
			k++;
		}
		boards[i].name = cd.rhd.bds[i].name;
		boards[i].ptr->init(!h, cd.network || boards.count()>1, _server, i==0,
							cd.rhd.bds[i].name);
	}
}

/*****************************************************************************/
/* Key management                                                            */
/*****************************************************************************/
void MPInterface::setDefaultKeycodes(uint nb, uint i, const int *def)
{
    _keyData->setKeycodes(nb, i, def);
}

void MPInterface::addKeys(KKeyDialog &d)
{
    _keyData->addKeys(d);
}

void MPInterface::saveKeys()
{
    _keyData->save();
}

void MPInterface::enableKeys(bool enable)
{
    if ( nbLocalHumans==0 ) return;
	// find the sending board
	uint i;
	for (i=0; i<boards.count(); i++) if ( sender()==boards[i].ptr ) break;
	int hi = boards[i].humanIndex;
	if ( hi==-1 ) return; // AI board (no keys)
    _keyData->setEnabled(hi, enable);
}

void MPInterface::keyPressEvent(QKeyEvent *e)
{
    _keyData->keyEvent(e, true);
}

void MPInterface::keyReleaseEvent(QKeyEvent *e)
{
    _keyData->keyEvent(e, false);
}

/*****************************************************************************/
/* Misc. functions                                                           */
/*****************************************************************************/
uint MPInterface::nbPlayers() const
{
	return internal->nbPlayers();
}

QString MPInterface::playerName(uint i) const
{
	Q_ASSERT(_server);
	return internal->playerName(i);
}

QDataStream &MPInterface::readingStream(uint i) const
{
	Q_ASSERT(_server);
	return internal->ioBuffer(i)->reading;
}

QDataStream &MPInterface::writingStream(uint i) const
{
	return internal->ioBuffer(i)->writing;
}

QDataStream &MPInterface::dataToClientsStream() const
{
	Q_ASSERT(_server);
	return *internal->globalStream();
}

void MPInterface::immediateWrite()
{
	internal->writeData(_server);
}

void MPInterface::hostDisconnected(uint, const QString &msg)
{
	errorBox(msg, QString::null, this);

	if ( !disconnected ) { // to avoid multiple calls
		disconnected = TRUE;
		// the zero timer is used to be outside the "internal" class
		QTimer::singleShot(0, this, SLOT(singleHumanSlot()));
	}
}

void MPInterface::singleHumanSlot()
{
	disconnected = FALSE;
	singleHuman();
}

void MPInterface::paintEvent(QPaintEvent *e)
{
	QPainter p(this);
	p.fillRect(e->rect(), darkGray);
}
