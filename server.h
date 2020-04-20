#ifndef SERVER_H
#define SERVER_H
#include <QObject>
#include <QUdpSocket>
#include <QHash>

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
    void sendInput(uint8_t playerNumber, uint32_t count);
    void checkIfExists(uint8_t playerNumber, uint32_t count);
    QUdpSocket* udpSocket;
    QHash<uint32_t, uint32_t> inputs[4]; //<count, BUTTONS>
    QList<uint32_t> buttons[4];
    struct player_info playerInfo[4];
};

#endif
