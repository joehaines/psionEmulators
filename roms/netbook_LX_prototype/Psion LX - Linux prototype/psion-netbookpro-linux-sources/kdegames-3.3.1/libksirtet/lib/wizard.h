#ifndef WIZARD_H
#define WIZARD_H

#include <qlabel.h>
#include <qlineedit.h>
#include <qvbox.h>
#include <qvgroupbox.h>
#include <qpushbutton.h>

#include <knuminput.h>
#include <kconfig.h>
#include <kwizard.h>

#include "pline.h"
#include "mp_interface.h"

class ConnectionData;

class MPWizard : public KWizard
{
 Q_OBJECT

 public:
	MPWizard(const MPGameInfo &gi, ConnectionData &cd,
			 QWidget *parent = 0, const char *name = 0);

	void showPage(QWidget *page);

 signals:
	void configureKeys(uint);

 protected slots:
	void accept();

 private slots:
	void typeChanged(int t);
	void lineTypeChanged(int);
	void configureKeysSlot();

 private:
	ConnectionData        &cd;
	enum Type { Local, Server, Client };
	Type                   type;
	QVBox                 *typePage, *localPage;
	WidgetList<PlayerLine> *wl;
	QLabel                 *lserver;
	QLineEdit              *eserver;
	KIntNumInput           *eport;
	QVGroupBox             *net;
//	QPushButton            *keys;

	void setupTypePage();
	void setupLocalPage(const MPGameInfo &gi);
	QString name(uint i) const;
};

#endif // WIZARD_H
