#ifndef SERVER_H
#define SERVER_H
#include <QObject>
#include <QUdpSocket>
#include <QHash>
#include "m64p_plugin.h"

struct input_state {
    BUTTONS Buttons[4];
};

struct player_info {
    QHostAddress address;
    int port;
    uint8_t count;
};

class Server : public QObject
{
    Q_OBJECT
public:
    void initSocket();
    void readPendingDatagrams();
private:
    QUdpSocket* udpSocket;
    QHash<uint8_t, input_state> inputs;
    struct player_info playerInfo[4];
};

#endif
