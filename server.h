#ifndef SERVER_H
#define SERVER_H
#include <QObject>
#include <QUdpSocket>
#include <QCache>
#include "m64p_plugin.h"

class InputState : public QObject
{
    Q_OBJECT
public:
    BUTTONS Buttons;
};

struct player_info {
    QHostAddress address;
    int port;
};

class Server : public QObject
{
    Q_OBJECT
public:
    void initSocket();
    void readPendingDatagrams();
private:
    void sendInput(int playerNumber, uint8_t count);
    void checkIfExists(int playerNumber, uint8_t count);
    QUdpSocket* udpSocket;
    QCache<uint8_t, InputState> inputs[4];
    QList<BUTTONS> buttons[4];
    struct player_info playerInfo[4];
};

#endif
