#include "wizard.h"
#include "wizard.moc"

#include <sys/types.h>
#include <netinet/in.h>

#include <qvbuttongroup.h>
#include <qradiobutton.h>
#include <qhbox.h>
#include <qvbox.h>
#include <qsignalmapper.h>
#include <qvgroupbox.h>
#include <qgrid.h>
#include <qfile.h>

#include <kapplication.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kdialogbase.h>

#include "types.h"
#include "defines.h"
#include "socket.h"

#ifdef __bsdi__
#define IPPORT_USERRESERVED IPPORT_DYNAMIC
#endif
#if defined(__NetBSD__) || defined(__FreeBSD__) || defined(__sgi)
#define IPPORT_USERRESERVED IPPORT_RESERVED
#endif
#define MIN_USER_PORT (unsigned short int)IPPORT_USERRESERVED
#define MAX_USER_PORT 65535

MPWizard::MPWizard(const MPGameInfo &gi, ConnectionData &_cd,
				   QWidget *parent, const char *name)
: KWizard(parent, name, TRUE), cd(_cd)
{
	setupTypePage();
	setupLocalPage(gi);
}

//-----------------------------------------------------------------------------
void MPWizard::setupTypePage()
{
    KConfigGroupSaver cg(kapp->config(), MP_GROUP);

	typePage = new QVBox(this);
	typePage->setMargin(KDialogBase::marginHint());

	QVButtonGroup *vbg = new QVButtonGroup(typePage);
	connect(vbg, SIGNAL(clicked(int)), SLOT(typeChanged(int)));
	QRadioButton *b;
	b = new QRadioButton(i18n("Create a local game"), vbg);
	b = new QRadioButton(i18n("Create a network game"), vbg);
	b = new QRadioButton(i18n("Join a network game"), vbg);
	type = (Type)cg.config()->readNumEntry(MP_GAMETYPE, 0);
	if ( type<0 || type>2 ) type = Local;
	vbg->setButton(type);

	typePage->setSpacing(KDialogBase::spacingHint());
	net = new QVGroupBox(i18n("Network Settings"), typePage);
	QGrid *grid = new QGrid(2, net);
	lserver = new QLabel(" ", grid);
	grid->setSpacing(KDialogBase::spacingHint());
	eserver = new QLineEdit(grid);
	(void)new QLabel(i18n("Port:"), grid);
	int p = cg.config()->readNumEntry(MP_PORT, (uint)MIN_USER_PORT);
	eport = new KIntNumInput(p, grid);
        eport->setRange(MIN_USER_PORT, MAX_USER_PORT, 1, false);

	addPage(typePage, i18n("Choose Game Type"));
	setHelpEnabled(typePage, FALSE);
	typeChanged(type);
}

//-----------------------------------------------------------------------------
void MPWizard::setupLocalPage(const MPGameInfo &gi)
{
	localPage = new QVBox(this);
	localPage->setMargin(KDialogBase::marginHint());

	wl = new WidgetList<PlayerLine>(5, localPage);
	QSignalMapper *husm = new QSignalMapper(this);
	if (gi.humanSettingSlot) connect(husm, SIGNAL(mapped(int)),
									 gi.humanSettingSlot);
	QSignalMapper *aism = new QSignalMapper(this);
	if (gi.AISettingSlot) connect(aism, SIGNAL(mapped(int)), gi.AISettingSlot);

    KConfigGroupSaver cg(kapp->config(), MP_GROUP);
	QString n;
	PlayerComboBox::Type type;
	PlayerLine *pl;
	Q_ASSERT( gi.maxNbLocalPlayers>0 );
	for (uint i=0; i<gi.maxNbLocalPlayers; i++) {
		type = (PlayerComboBox::Type)
			cg.config()->readNumEntry(QString(MP_PLAYER_TYPE).arg(i),
   			            (i==0 ? PlayerComboBox::Human : PlayerComboBox::None));
		n = cg.config()->readEntry(QString(MP_PLAYER_NAME).arg(i),
                             i18n("Player #%1").arg(i));

		pl = new PlayerLine(type, n, gi.humanSettingSlot, gi.AISettingSlot,
							i!=0, gi.AIAllowed, wl);
		connect(pl, SIGNAL(typeChanged(int)), SLOT(lineTypeChanged(int)));
		husm->setMapping(pl, i);
		connect(pl, SIGNAL(setHuman()), husm, SLOT(map()));
		aism->setMapping(pl, i);
		connect(pl, SIGNAL(setAI()), aism, SLOT(map()));
		wl->append(pl);
	}

	((QVBox *)localPage)->setSpacing(KDialogBase::spacingHint());

//	keys = new QPushButton(i18n("Configure Keys..."), localPage);
//	connect(keys, SIGNAL(clicked()), SLOT(configureKeysSlot()));

	addPage(localPage, i18n("Local Player's Settings"));
	setHelpEnabled(localPage, FALSE);
	lineTypeChanged(0);
}

QString MPWizard::name(uint i) const
{
	QString s = wl->widget(i)->name();
	if ( s.length()==0 ) s = i18n("Player #%1").arg(i);
	return s;
}

void MPWizard::typeChanged(int t)
{
	type = (Type)t;

	QString str;
	if ( type!=Client ) {
		str = "localhost";
		lserver->setText(i18n("Hostname:"));
	} else {
        KConfigGroupSaver cg(kapp->config(), MP_GROUP);
		str = cg.config()->readEntry(MP_SERVER_ADDRESS,
                               i18n("the.server.address"));
		lserver->setText(i18n("Server address:"));
	}
	eserver->setText(str);
	eserver->setEnabled(type==Client);
	eport->setEnabled(type!=Local);
	net->setEnabled(type!=Local);
}

void MPWizard::lineTypeChanged(int)
{
	bool b = FALSE;
	for (uint i=0; i<wl->size(); i++)
		if ( wl->widget(i)->type()==PlayerComboBox::Human ) {
			b = TRUE;
			break;
		}
//	keys->setEnabled(b);
}

void MPWizard::accept()
{
    KConfigGroupSaver cg(kapp->config(), MP_GROUP);

	cd.network         = ( type!=Local );
	cd.server          = ( type!=Client );

	if (cd.network) {
        //**********************************************************
        // create socket
        int flags = KExtendedSocket::inetSocket
                    | KExtendedSocket::streamSocket;
        if (cd.server) flags |= KExtendedSocket::passiveSocket;
        QString host = QFile::encodeName(eserver->text());
        KExtendedSocket *socket
            = new KExtendedSocket(host, eport->value(), flags);

        // do lookup
        int res = socket->lookup();
        if ( checkSocket(res, socket, i18n("Error looking up for \"%1\"")
                         .arg(host), this) ) {
            delete socket;
            return;
        }

        // connect (client) or listen (server)
        res = (cd.server ? socket->listen() : socket->connect());
        if ( checkSocket(res, socket, i18n("Error opening socket"), this) ) {
            delete socket;
            return;
        }

        cd.rhd.socket = new Socket(socket, true);

		if ( !cd.server )
            cg.config()->writeEntry(MP_SERVER_ADDRESS, eserver->text());
		cg.config()->writeEntry(MP_PORT, eport->value());
	}

    BoardData bd;
	for (uint i=0; i<wl->size(); i++) {
		if ( wl->widget(i)->type()==PlayerComboBox::None ) continue;
		bd.name = name(i);
		bd.type = wl->widget(i)->type();
		cd.rhd.bds += bd;
	}

	cg.config()->writeEntry(MP_GAMETYPE, (int)type);
	for (uint i=0; i<wl->size(); i++) {
		cg.config()->writeEntry(QString(MP_PLAYER_TYPE).arg(i),
						  (int)wl->widget(i)->type());
		cg.config()->writeEntry(QString(MP_PLAYER_NAME).arg(i), name(i));
	}

	KWizard::accept();
}

void MPWizard::showPage(QWidget *page)
{
	if ( page==localPage ) setFinishEnabled(localPage, TRUE);
	KWizard::showPage(page);
}

void MPWizard::configureKeysSlot()
{
	uint nb = 0;
	for (uint i=0; i<wl->size(); i++)
		if ( wl->widget(i)->type()==PlayerComboBox::Human ) nb++;
	emit configureKeys(nb);
}
