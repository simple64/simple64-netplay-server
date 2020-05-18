#ifndef SERVERTHREAD_H
#define SERVERTHREAD_H
#include <QThread>
#include "udpServer.h"
#include "tcpServer.h"

class ServerThread : public QThread
{
    Q_OBJECT
    void run() Q_DECL_OVERRIDE;
public:
    ServerThread(int _port, QObject *parent = 0);
signals:
    void killServer(int port);
    void desynced(int port);
private slots:
    void desync(int port);
private:
    int port;
    UdpServer *udpServer;
    TcpServer *tcpServer;
};

#endif
