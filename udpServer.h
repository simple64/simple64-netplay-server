#ifndef UDPSERVER_H
#define UDPSERVER_H
#include <QObject>
#include <QUdpSocket>
#include <QHash>

class UdpServer : public QObject
{
    Q_OBJECT
public:
    UdpServer();
    ~UdpServer();
    void readPendingDatagrams();
    int getPort();
    void setPort(int _port);
protected:
    void timerEvent(QTimerEvent *te) Q_DECL_OVERRIDE;
signals:
    void killMe(int port);
    void desynced();
public slots:
    void register_player(uint32_t reg_id, uint8_t playerNum, uint8_t plugin);
    void disconnect_player(uint32_t reg_id);
private:
    void sendInput(uint32_t count, QHostAddress address, int port, uint8_t playerNum, uint8_t spectator);
    void checkIfExists(uint8_t playerNumber, uint32_t count);
    QUdpSocket udpSocket;
    QHash<uint32_t, QPair<uint32_t, uint8_t>> inputs[4]; //<count, <BUTTONS, Plugin>>
    QHash<uint32_t, uint64_t> sync_hash; //cp0 hashes
    QHash<uint32_t, QPair<uint8_t, uint8_t>> player_keepalive; //reg_id, <keepalive, playernumber>
    QList<QPair<uint32_t, uint8_t>> buttons[4];
    uint32_t lead_count[4];
    uint8_t buffer_size[4];
    int buffer_health[4];
    int timerId;
    int port;
    uint8_t status;
};

#endif
