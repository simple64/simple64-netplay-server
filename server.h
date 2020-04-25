#ifndef SERVER_H
#define SERVER_H
#include <QObject>
#include <QUdpSocket>
#include <QHash>

class Server : public QObject
{
    Q_OBJECT
public:
    void initSocket();
    void readPendingDatagrams();
private:
    void sendInput(uint32_t count, QHostAddress address, int port, uint8_t playerNum, uint8_t spectator);
    void checkIfExists(uint8_t playerNumber, uint32_t count);
    void sendRegResponse(uint8_t playerNumber, uint32_t reg_id, QHostAddress address, int port);
    QUdpSocket* udpSocket;
    QHash<uint32_t, QPair<uint32_t, uint8_t>> inputs[4]; //<count, <BUTTONS, Plugin>>
    QHash<uint8_t, uint32_t> reg; //player number, reg_id
    QList<QPair<uint32_t, uint8_t>> buttons[4];
    uint32_t lead_count[4];
};

#endif
