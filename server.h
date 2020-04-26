#ifndef SERVER_H
#define SERVER_H
#include <QObject>
#include <QUdpSocket>
#include <QCache>

class InputState : public QObject
{
    Q_OBJECT
public:
    QPair<uint32_t, uint8_t> data; //<BUTTONS, Plugin>
};

class Server : public QObject
{
    Q_OBJECT
public:
    void initSocket();
    void readPendingDatagrams();
protected:
    void timerEvent(QTimerEvent *te) Q_DECL_OVERRIDE;
private:
    void sendInput(uint32_t count, QHostAddress address, int port, uint8_t playerNum, uint8_t spectator);
    void checkIfExists(uint8_t playerNumber, uint32_t count);
    void sendRegResponse(uint8_t playerNumber, uint32_t reg_id, QHostAddress address, int port);
    QUdpSocket* udpSocket;
    QCache<uint32_t, InputState> inputs[4]; //<count, <BUTTONS, Plugin>>
    QHash<uint8_t, uint32_t> reg; //player number, reg_id
    QList<QPair<uint32_t, uint8_t>> buttons[4];
    uint32_t lead_count[4];
    uint8_t buffer_size;
    int buffer_health;
    int timerId;
};

#endif
