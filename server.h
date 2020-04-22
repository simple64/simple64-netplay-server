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
    int hasData(uint32_t count);
    void sendInput(uint32_t count, QHostAddress address, int port, int spectator);
    void checkIfExists(uint8_t playerNumber, uint32_t count);
    QUdpSocket* udpSocket;
    QHash<uint32_t, uint32_t> inputs[4]; //<count, BUTTONS>
    QList<uint32_t> buttons[4];
};

#endif
