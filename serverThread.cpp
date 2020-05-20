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
    connect(udpServer, &UdpServer::desynced, this, &ServerThread::desync);
    connect(tcpServer, &TcpServer::register_player, udpServer, &UdpServer::register_player);
    connect(tcpServer, &TcpServer::disconnect_player, udpServer, &UdpServer::disconnect_player);

    exec();

    delete udpServer;
    delete tcpServer;
    emit killServer(port);
}

void ServerThread::desync(int port)
{
    emit desynced(port);
}
