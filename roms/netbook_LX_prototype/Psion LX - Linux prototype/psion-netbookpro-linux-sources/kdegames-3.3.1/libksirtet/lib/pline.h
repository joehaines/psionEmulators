#ifndef PLINE_H
#define PLINE_H

#include <qframe.h>
#include <qscrollbar.h>
#include <qlineedit.h>
#include <qlabel.h>
#include <qptrlist.h>
#include <qlayout.h>

#include "types.h"

class QPushButton;

/** Internal class : display a "player line" in netmeeting. */
class MeetingLine : public QFrame
{
 Q_OBJECT

 public:
    MeetingLine(bool isOwner, bool readerIsServer, bool serverLine,
				QWidget *parent, const char *name = 0);

	MeetingCheckBox::Type type() const { return tcb->type(); }
	void setType(MeetingCheckBox::Type type) { tcb->setType(type); }
	void setText(const QString &text) { qle->setText(text); }

	void setData(const ExtData &ed);
	void data(ExtData &ed) const;
	QString text() const { return qle->text(); }

 signals:
	void typeChanged(MeetingCheckBox::Type);
	void textChanged(const QString &);

 private slots:
	void _typeChanged(int t)
		{ emit typeChanged((MeetingCheckBox::Type)t); };
	void _textChanged(const QString &text) { emit textChanged(text); }

 protected:
	QHBoxLayout *hbl;

 private:
	MeetingCheckBox       *tcb;
	QLabel                *lname, *labH, *labAI;
	QValueList<BoardData>  bds;
	QLineEdit             *qle;
};

class PlayerLine : public QFrame
{
 Q_OBJECT

 public:
	PlayerLine(PlayerComboBox::Type type, const QString &txt,
			   bool humanSetting, bool AISetting,
			   bool canBeEmpty, bool acceptAI,
			   QWidget *parent = 0, const char *name = 0);

	PlayerComboBox::Type type() const { return pcb->type(); }
	QString name() const { return edit->text(); }

 signals:
	void setHuman();
	void setAI();
	void typeChanged(int);

 private slots:
	void setSlot();
	void typeChangedSlot(int);

 private:
	PlayerComboBox *pcb;
	QLineEdit      *edit;
	QPushButton    *setting;
	bool            hs, as;
};

/** Internal class : scrolable list of widgets. */
class GWidgetList : public QWidget
{
 Q_OBJECT

 public:
	GWidgetList(uint interval, QWidget *parent = 0, const char * name = 0);

	void remove(uint i);
	uint size() const { return widgets.count(); }

 protected:
	/** The widget must be created with this widget as parent. */
	void append(QWidget *);
	QWidget *widget(uint i) { return widgets.at(i); }

 private:
	QPtrList<QWidget> widgets;
	QVBoxLayout vbl;
};

template <class Type>
class WidgetList : public GWidgetList
{
 public:
	WidgetList(uint interval, QWidget *parent=0, const char *name=0)
	: GWidgetList(interval, parent, name) {}

	void append(Type *w) { GWidgetList::append(w); }
	Type *widget(uint i) { return (Type *)GWidgetList::widget(i); }
};

#endif // PLINE_H
