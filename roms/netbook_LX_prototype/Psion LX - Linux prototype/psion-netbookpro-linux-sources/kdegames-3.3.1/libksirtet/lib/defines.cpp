#include "defines.h"

#include <klocale.h>

void errorBox(const QString &msg1, const QString &msg2, QWidget *parent)
{
    QString str;
    if ( msg2.isNull() ) str = msg1;
    else str = i18n("%1:\n%2").arg(msg1).arg(msg2);
    KMessageBox::error(parent, str);
}

QString socketError(const KExtendedSocket *socket)
{
    return KExtendedSocket::strError(socket->status(), socket->systemError());
}

bool checkSocket(int res, const KExtendedSocket *socket,
                 const QString &msg, QWidget *parent)
{
    if ( res==0 ) return false;
    errorBox(msg, socketError(socket), parent);
    return true;
}
