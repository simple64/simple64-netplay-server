#include "serverThread.h"
#include "udpServer.h"
#include "tcpServer.h"

ServerThread::ServerThread(int _port, QObject *parent)
    : QThread(parent)
{
    port = _port;
}

void ServerThread::run()
{
    udpServer = new UdpServer(port);
    tcpServer = new TcpServer;
    tcpServer->setPort(port);
    connect(udpServer, &UdpServer::killMe, this, &ServerThread::quit);

    exec();

    delete udpServer;
    delete tcpServer;
    emit killServer(port);
}
