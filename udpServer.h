#ifndef UDPSERVER_H
#define UDPSERVER_H
#include <QObject>
#include <QUdpSocket>
#include <QHash>

class UdpServer : public QObject
{
    Q_OBJECT
public:
    UdpServer(char _buffer_target);
    void readPendingDatagrams();
    int getPort();
    void setPort(int _port);
    void close();
    void setInputDelay(int playerNum, int inputDelay);
protected:
    void timerEvent(QTimerEvent *te) Q_DECL_OVERRIDE;
signals:
    void killMe(int port);
    void writeLog(QString message, int port);
    void desynced();
public slots:
    void register_player(quint32 reg_id, quint8 playerNum, quint8 plugin);
    void disconnect_player(quint32 reg_id);
private:
    void sendInput(quint32 count, QHostAddress address, int port, quint8 playerNum, quint8 spectator);
    bool checkIfExists(quint8 playerNumber, quint32 count);
    void insertInput(int playerNum, int count, QPair<quint32, quint8> pair);
    QUdpSocket udpSocket;
    QHash<quint32, QPair<quint32, quint8>> inputs[4]; //<count, <BUTTONS, Plugin>>
    QHash<quint32, quint64> sync_hash; //cp0 hashes
    QHash<quint32, QPair<quint8, quint8>> player_keepalive; //reg_id, <keepalive, playernumber>
    QList<QPair<quint32, quint8>> buttons[4];
    quint32 lead_count[4];
    quint8 buffer_size[4];
    int buffer_health[4];
    int input_delay[4];
    int timerId;
    int port;
    quint8 status;
    char buffer_target;
};

#endif
