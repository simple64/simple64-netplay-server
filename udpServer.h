#ifndef UDPSERVER_H
#define UDPSERVER_H
#include <QObject>
#include <QUdpSocket>
#include <QCache>

class InputState : public QObject
{
    Q_OBJECT
public:
    QPair<uint32_t, uint8_t> data; //<BUTTONS, Plugin>
};

class HashState : public QObject
{
    Q_OBJECT
public:
    QList<uint64_t> data;
};

class UdpServer : public QObject
{
    Q_OBJECT
public:
    UdpServer(int _port);
    ~UdpServer();
    void readPendingDatagrams();
    int getPort();
protected:
    void timerEvent(QTimerEvent *te) Q_DECL_OVERRIDE;
signals:
    void killMe(int port);
    void desynced(int port);
public slots:
    void register_player(uint32_t reg_id, uint8_t playerNum, uint8_t plugin);
private:
    void sendInput(uint32_t count, QHostAddress address, int port, uint8_t playerNum, uint8_t spectator);
    void checkIfExists(uint8_t playerNumber, uint32_t count);
    QUdpSocket* udpSocket;
    QCache<uint32_t, InputState> inputs[4]; //<count, <BUTTONS, Plugin>>
    QCache<uint32_t, HashState> sync_hash; //cp0 hashes
    QHash<uint32_t, QPair<uint8_t, uint8_t>> player_keepalive; //reg_id, <keepalive, playernumber>
    QList<QPair<uint32_t, uint8_t>> buttons[4];
    uint32_t lead_count[4];
    uint8_t buffer_size[4];
    int buffer_health[4];
    int timerId;
    int port;
    uint8_t status;
    uint8_t desync;
};

#endif
