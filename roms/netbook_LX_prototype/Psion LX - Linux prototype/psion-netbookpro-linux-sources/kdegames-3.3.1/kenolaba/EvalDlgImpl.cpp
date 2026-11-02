/**
 * Some Implementation details for configEval Dialog
 *
 */

#include <qvalidator.h>
#include <qlineedit.h>
#include <qlcdnumber.h>
#include <klineeditdlg.h>
#include <qlistbox.h>
#include <qpushbutton.h>

#include <kconfig.h>
#include <klocale.h>
#include <kdebug.h>
#include <kapplication.h>

#include "EvalDlgImpl.h"
#include "Board.h"
#include "EvalScheme.h"

EvalDlgImpl::EvalDlgImpl(QWidget* parent, Board* board)
  :EvalDlg(parent)
{
  _board = board;
  _origScheme = board->evalScheme();
  _scheme  = new EvalScheme(*_origScheme);

  connect( evalDelete, SIGNAL( clicked() ), this, SLOT( deleteEntry() ) );
  connect( evalSaveAs, SIGNAL( clicked() ), this, SLOT( saveas() ) );
  connect( evalList, SIGNAL( highlighted(int) ), this, SLOT( select(int) ) ); 

  KConfig* config = kapp->config();
  config->setGroup("General");
  QStringList list = config->readListEntry("EvalSchemes");
  evalList->insertItem( i18n("Current") );
  evalList->insertItem( i18n("Default") );
  for(int i=0;i<list.count();i++)
    evalList->insertItem(list[i]);

  evalList->setSelected(0, TRUE);

  updateWidgets();
  connectEditLines();
}

EvalDlgImpl::~EvalDlgImpl()
{
  delete _scheme;
}


void EvalDlgImpl::connectEditLines()
{
  connect( moveEval1, SIGNAL(textChanged(const QString&)), this, SLOT(updateMove()) );          
  connect( moveEval2, SIGNAL(textChanged(const QString&)), this, SLOT(updateMove()) );          
  connect( moveEval3, SIGNAL(textChanged(const QString&)), this, SLOT(updateMove()) );          
  connect( moveEval4, SIGNAL(textChanged(const QString&)), this, SLOT(updateMove()) );          
  connect( moveEval5, SIGNAL(textChanged(const QString&)), this, SLOT(updateMove()) );          
  connect( moveEval6, SIGNAL(textChanged(const QString&)), this, SLOT(updateMove()) );          
  connect( moveEval7, SIGNAL(textChanged(const QString&)), this, SLOT(updateMove()) );
  connect( moveEval8, SIGNAL(textChanged(const QString&)), this, SLOT(updateMove()) );
  connect( moveEval9, SIGNAL(textChanged(const QString&)), this, SLOT(updateMove()) );
  connect( posEval1, SIGNAL(textChanged(const QString&)), this, SLOT(updateFields()) );     
  connect( posEval2, SIGNAL(textChanged(const QString&)), this, SLOT(updateFields()) ); 
  connect( posEval3, SIGNAL(textChanged(const QString&)), this, SLOT(updateFields()) );     
  connect( posEval4, SIGNAL(textChanged(const QString&)), this, SLOT(updateFields()) );     
  connect( posEval5, SIGNAL(textChanged(const QString&)), this, SLOT(updateFields()) );     
  connect( diffEval2, SIGNAL(textChanged(const QString&)), this, SLOT(updateFields()) );
  connect( diffEval3, SIGNAL(textChanged(const QString&)), this, SLOT(updateFields()) );
  connect( diffEval4, SIGNAL(textChanged(const QString&)), this, SLOT(updateFields()) );
  connect( diffEval5, SIGNAL(textChanged(const QString&)), this, SLOT(updateFields()) );
  connect( rowEval2, SIGNAL(textChanged(const QString&)), this, SLOT(updateInARow()) );
  connect( rowEval3, SIGNAL(textChanged(const QString&)), this, SLOT(updateInARow()) );
  connect( rowEval4, SIGNAL(textChanged(const QString&)), this, SLOT(updateInARow()) );
  connect( rowEval5, SIGNAL(textChanged(const QString&)), this, SLOT(updateInARow()) );
  connect( countEval1, SIGNAL(textChanged(const QString&)), this, SLOT(updateCount()) );
  connect( countEval2, SIGNAL(textChanged(const QString&)), this, SLOT(updateCount()) );
  connect( countEval3, SIGNAL(textChanged(const QString&)), this, SLOT(updateCount()) );
  connect( countEval4, SIGNAL(textChanged(const QString&)), this, SLOT(updateCount()) );
  connect( countEval5, SIGNAL(textChanged(const QString&)), this, SLOT(updateCount()) );
}

void EvalDlgImpl::disconnectEditLines()
{
  moveEval1->disconnect();
  moveEval2->disconnect();
  moveEval3->disconnect();
  moveEval4->disconnect();
  moveEval5->disconnect();
  moveEval6->disconnect();
  moveEval7->disconnect();
  moveEval8->disconnect();
  moveEval9->disconnect();
  posEval1->disconnect();
  posEval2->disconnect();
  posEval3->disconnect();
  posEval4->disconnect();
  posEval5->disconnect();
  diffEval2->disconnect();
  diffEval3->disconnect();
  diffEval4->disconnect();
  diffEval5->disconnect();
  rowEval2->disconnect();
  rowEval3->disconnect();
  rowEval4->disconnect();
  rowEval5->disconnect();
  countEval1->disconnect();
  countEval2->disconnect();
  countEval3->disconnect();
  countEval4->disconnect();
  countEval5->disconnect();
}

void EvalDlgImpl::updateWidgets()
{
  // Moves
  moveEval1->setText( QString::number(_scheme->moveValue(Move::move1)) );
  moveEval2->setText( QString::number(_scheme->moveValue(Move::move2)) );
  moveEval3->setText( QString::number(_scheme->moveValue(Move::move3)) );
  moveEval4->setText( QString::number(_scheme->moveValue(Move::push1with2)) );
  moveEval5->setText( QString::number(_scheme->moveValue(Move::push1with3)) );
  moveEval6->setText( QString::number(_scheme->moveValue(Move::push2)) );
  moveEval7->setText( QString::number(_scheme->moveValue(Move::out1with2)) );
  moveEval8->setText( QString::number(_scheme->moveValue(Move::out1with3)) );
  moveEval9->setText( QString::number(_scheme->moveValue(Move::out2)) );

  // Position
  posEval1->setText( QString::number(_scheme->ringValue(0)) );
  posEval2->setText( QString::number(_scheme->ringValue(1)) );
  posEval3->setText( QString::number(_scheme->ringValue(2)) );
  posEval4->setText( QString::number(_scheme->ringValue(3)) );
  posEval5->setText( QString::number(_scheme->ringValue(4)) );

  diffEval2->setText( QString::number(_scheme->ringDiff(1)) );
  diffEval3->setText( QString::number(_scheme->ringDiff(2)) );
  diffEval4->setText( QString::number(_scheme->ringDiff(3)) );
  diffEval5->setText( QString::number(_scheme->ringDiff(4)) );

  // InARow
  rowEval2->setText( QString::number(_scheme->inARowValue(0)) );
  rowEval3->setText( QString::number(_scheme->inARowValue(1)) );
  rowEval4->setText( QString::number(_scheme->inARowValue(2)) );
  rowEval5->setText( QString::number(_scheme->inARowValue(3)) );

  // Count
  countEval1->setText( QString::number(_scheme->stoneValue(1)) );
  countEval2->setText( QString::number(_scheme->stoneValue(2)) );
  countEval3->setText( QString::number(_scheme->stoneValue(3)) );
  countEval4->setText( QString::number(_scheme->stoneValue(4)) );
  countEval5->setText( QString::number(_scheme->stoneValue(5)) );

  updateEval();
}

void EvalDlgImpl::updateEval()
{
  int value;

  // set temporary the new scheme
  _board->setEvalScheme(_scheme);
  value = - _board->calcEvaluation();
  _board->setEvalScheme(_origScheme);

  kdDebug(12011) << "Updated Eval: " << value << endl;

  if (value<-15999 || value>15999) value=0;
  actualEval->display(value);
}

void EvalDlgImpl::updateMove()
{
  _scheme->setMoveValue(Move::move1,  moveEval1->text().toInt());
  _scheme->setMoveValue(Move::move2,  moveEval2->text().toInt());
  _scheme->setMoveValue(Move::left2,  moveEval2->text().toInt());
  _scheme->setMoveValue(Move::right2, moveEval2->text().toInt());
  _scheme->setMoveValue(Move::move3,  moveEval3->text().toInt());
  _scheme->setMoveValue(Move::left3,  moveEval3->text().toInt());
  _scheme->setMoveValue(Move::right3, moveEval3->text().toInt());

  _scheme->setMoveValue(Move::push1with2, moveEval4->text().toInt());
  _scheme->setMoveValue(Move::push1with3, moveEval5->text().toInt());
  _scheme->setMoveValue(Move::push2,      moveEval6->text().toInt());

  _scheme->setMoveValue(Move::out1with2, moveEval7->text().toInt());
  _scheme->setMoveValue(Move::out1with3, moveEval8->text().toInt());
  _scheme->setMoveValue(Move::out2,      moveEval9->text().toInt());
  
  updateEval();
}

void EvalDlgImpl::updateCount()
{
  _scheme->setStoneValue(1, countEval1->text().toInt());
  _scheme->setStoneValue(2, countEval2->text().toInt());
  _scheme->setStoneValue(3, countEval3->text().toInt());
  _scheme->setStoneValue(4, countEval4->text().toInt());
  _scheme->setStoneValue(5, countEval5->text().toInt());

  updateEval();
}

void EvalDlgImpl::updateFields()
{
  _scheme->setRingValue(0, posEval1->text().toInt());
  _scheme->setRingValue(1, posEval2->text().toInt());
  _scheme->setRingValue(2, posEval3->text().toInt());
  _scheme->setRingValue(3, posEval4->text().toInt());
  _scheme->setRingValue(4, posEval5->text().toInt());
  _scheme->setRingDiff(1, diffEval2->text().toInt());
  _scheme->setRingDiff(2, diffEval3->text().toInt());
  _scheme->setRingDiff(3, diffEval4->text().toInt());
  _scheme->setRingDiff(4, diffEval5->text().toInt());

  updateEval();
}

void EvalDlgImpl::updateInARow()
{
  _scheme->setInARowValue(0, rowEval2->text().toInt());
  _scheme->setInARowValue(1, rowEval3->text().toInt());
  _scheme->setInARowValue(2, rowEval4->text().toInt());
  _scheme->setInARowValue(3, rowEval5->text().toInt());

  updateEval();
}


void EvalDlgImpl::deleteEntry()
{
  int i = evalList->currentItem();
  // You cannot delete Pseudo Items 0 (Current) and 1 (Default)

  if (i>1) {
    QString name = evalList->text(i);
    evalList->removeItem(i);
    
    KConfig* config = kapp->config();
    config->setGroup("General");
    QStringList list = config->readListEntry("EvalSchemes");
    list.remove(name);
    config->writeEntry("EvalSchemes", list);
    config->sync();
  }
}

void EvalDlgImpl::saveas()
{
  KLineEditDlg dlg(i18n("Name for scheme:"), QString::null, this);
  dlg.setCaption(i18n("Save Scheme"));

  if (dlg.exec()) {
    QString name=dlg.text();
    KConfig* config = kapp->config();
    config->setGroup("General");
    QStringList list = config->readListEntry("EvalSchemes");
    QListBoxItem *it = evalList->findItem(name);
    if (!it) {
      evalList->insertItem(name);
      it = evalList->findItem(name);
      list << name;
      config->writeEntry("EvalSchemes", list);
    }
    evalList->setSelected(it, TRUE);

    EvalScheme savedScheme(*_scheme);
    savedScheme.setName(name);
    savedScheme.save(config);
    config->sync();
  }
}

void EvalDlgImpl::select(int i)
{
  QString name = evalList->text(i);

  delete _scheme;
  _scheme = 0;

  // 2 fixed entries: 0 is Current (origScheme), 1 is Default

  if (i == 0)  _scheme = new EvalScheme(*_origScheme);
  else if (i==1) {
    _scheme = new EvalScheme(name);
  }
  else {
    // read in the Scheme from the config file
   _scheme = new EvalScheme(name);
    KConfig* config = kapp->config();
   _scheme->read(config);
  }

  kdDebug(12011) << "Selected " << name << ", Index " << i << endl;

  disconnectEditLines();
  updateWidgets();
  connectEditLines();
}
#include "EvalDlgImpl.moc"
