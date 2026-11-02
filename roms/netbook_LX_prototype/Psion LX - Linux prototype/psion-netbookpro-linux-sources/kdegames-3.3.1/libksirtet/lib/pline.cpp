#include "pline.h"
#include "pline.moc"

#include <qfont.h>
#include <qpushbutton.h>
#include <klocale.h>
#include "defines.h"

#define THIN_BORDER 4

MeetingLine::MeetingLine(bool isOwner, bool serverIsReader, bool serverLine,
						 QWidget *parent, const char *name)
: QFrame(parent, name)
{
	setFrameStyle(Panel | (serverLine ? Raised : Plain));

	// Top layout
	hbl = new QHBoxLayout(this, THIN_BORDER + frameWidth());

	/* TriCheckBox */
	tcb = new MeetingCheckBox(MeetingCheckBox::Ready, isOwner, serverIsReader,
                              this);
	if ( !XOR(isOwner, serverIsReader) ) tcb->setEnabled(FALSE);
	else connect(tcb, SIGNAL(changed(int)), SLOT(_typeChanged(int)));
	hbl->addWidget(tcb);

    /* Name */
	lname = new QLabel(" ", this);
	lname->setAlignment(AlignCenter);
	lname->setFrameStyle(QFrame::Panel | QFrame::Sunken);
	lname->setLineWidth(2);
	lname->setMidLineWidth(3);
    QFont f = lname->font();
    f.setBold(TRUE);
    lname->setFont(f);
	lname->setFixedSize(lname->fontMetrics().maxWidth()*NAME_MAX_LENGTH,
						lname->sizeHint().height());
	hbl->addWidget(lname);
	hbl->addStretch(1);

	// Nb humans
	labH = new QLabel(this);
	hbl->addWidget(labH);

	// Nb AIs
	labAI = new QLabel(this);
	hbl->addWidget(labAI);

	// talker
	qle = new QLineEdit(this);
	qle->setMaxLength(TALKER_MAX_LENGTH);
	qle->setFont( QFont("fixed", 12, QFont::Bold) );
	qle->setFixedSize(qle->fontMetrics().maxWidth()*TALKER_MAX_LENGTH,
					  qle->sizeHint().height());
	connect(qle, SIGNAL(textChanged(const QString &)),
			SLOT(_textChanged(const QString &)));
	qle->setEnabled(isOwner);
	hbl->addWidget(qle);
}

void MeetingLine::setData(const ExtData &ed)
{
	bds = ed.bds;
	uint nbh = 0, nba = 0;
	for (uint i=0; i<bds.count(); i++) {
		if ( bds[i].type==PlayerComboBox::Human ) nbh++;
		else if ( bds[i].type==PlayerComboBox::AI ) nba++;
	}
	labH->setText(i18n("Hu=%1").arg(nbh));
	labAI->setText(i18n("AI=%1").arg(nba));
 	lname->setText(bds[0].name);
	setType(ed.type);
	setText(ed.text);
}

void MeetingLine::data(ExtData &ed) const
{
	ed.bds  = bds;
	ed.type = tcb->type();
	ed.text = text();
}

/*****************************************************************************/
PlayerLine::PlayerLine(PlayerComboBox::Type type, const QString &txt,
					   bool humanSetting, bool AISetting,
					   bool canBeEmpty, bool acceptAI,
					   QWidget *parent, const char *name)
: QFrame(parent, name), hs(humanSetting), as(AISetting)
{
	setFrameStyle(Panel | Raised);

	// Top layout
	QHBoxLayout *hbl;
	hbl = new QHBoxLayout(this, THIN_BORDER + frameWidth());

	/* CheckBox */
	pcb = new PlayerComboBox(type, canBeEmpty, acceptAI, this);
	connect(pcb, SIGNAL(changed(int)), SLOT(typeChangedSlot(int)));
	hbl->addWidget(pcb);

    /* Name */
	edit = new QLineEdit(txt, this);
	edit->setMaxLength(NAME_MAX_LENGTH);
	edit->setFixedSize(edit->fontMetrics().maxWidth()*(NAME_MAX_LENGTH+2),
					   edit->sizeHint().height());
	hbl->addWidget(edit);

	/* settings button */
	setting = new QPushButton(i18n("Settings..."), this);
	connect(setting, SIGNAL(clicked()), SLOT(setSlot()));
	hbl->addWidget(setting);

	typeChangedSlot(type);
}

void PlayerLine::typeChangedSlot(int t)
{
	edit->setEnabled(type()!=PlayerComboBox::None);
	setting->setEnabled( (type()==PlayerComboBox::Human && hs)
						 || (type()==PlayerComboBox::AI && as) );
	emit typeChanged(t);
}

void PlayerLine::setSlot()
{
	if ( type()==PlayerComboBox::Human ) emit setHuman();
	else emit setAI();
}

/*****************************************************************************/
GWidgetList::GWidgetList(uint interval, QWidget *parent, const char * name)
	: QWidget(parent, name), vbl(this, interval)
{
	widgets.setAutoDelete(TRUE);
}

void GWidgetList::append(QWidget *wi)
{
	vbl.addWidget(wi);
	wi->show();
	widgets.append(wi);
}

void GWidgetList::remove(uint i)
{
	widgets.remove(i);
}
