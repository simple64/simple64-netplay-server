#ifndef SERVER_H
#define SERVER_H
#include <QObject>
#include <QUdpSocket>

class Server : public QObject
{
    Q_OBJECT
public:
    void initSocket();
    void readPendingDatagrams();
private:
    QUdpSocket* udpSocket;
};

#endif
