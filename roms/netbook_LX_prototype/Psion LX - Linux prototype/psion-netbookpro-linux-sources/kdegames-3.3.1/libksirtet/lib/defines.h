#ifndef DEFINES_H
#define DEFINES_H

#include <kmessagebox.h>
#include <kextsock.h>

// constants
#define TALKER_MAX_LENGTH 35
#define NAME_MAX_LENGTH   15

// config keys
#define MP_GROUP          "Multi-Players"
#define MP_GAMETYPE       "Game type"
#define MP_PLAYER_NAME    "Player name #%1"
#define MP_PLAYER_TYPE    "Player type #%1"
#define MP_SERVER_ADDRESS "Server address"
#define MP_PORT           "Port"

void errorBox(const QString &msg1, const QString &msg2, QWidget *parent);
QString socketError(const KExtendedSocket *socket);
bool checkSocket(int res, const KExtendedSocket *,
                 const QString &msg, QWidget *parent);

#define R_ERROR_BOX(msg1, msg2) { \
    errorBox(msg1, msg2, this); \
    return; \
}

template <class Type>
bool XOR(Type a, Type b)
{
    return ( (!a && b) || (a && !b) );
}

#endif // DEFINES_H
