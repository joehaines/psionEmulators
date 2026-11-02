#ifndef MISCUI_H
#define MISCUI_H

#include <qcombobox.h>
#include <qcheckbox.h>


//-----------------------------------------------------------------------------
class MeetingCheckBox : public QWidget
{
 Q_OBJECT
 public:
	enum Type { Ready, NotReady, Excluded };
	MeetingCheckBox(Type, bool owner, bool server, QWidget *parent);

    void setType(Type);
	Type type() const;

 signals:
    void changed(int);

 private slots:
    void changedSlot();

 private:
    QCheckBox *_ready, *_excluded;
};

//-----------------------------------------------------------------------------
class PlayerComboBox : public QComboBox
{
 Q_OBJECT
 public:
	enum Type { Human = 0, AI, None };
	PlayerComboBox(Type, bool canBeNone, bool acceptAI, QWidget *parent);

	Type type() const { return (Type)currentItem(); }

 signals:
    void changed(int);
};

#endif // MISCUI_H
