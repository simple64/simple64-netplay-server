#include "serverThread.h"
#include "udpServer.h"
#include "tcpServer.h"

ServerThread::ServerThread(int _port, QObject *parent)
    : QThread(parent)
{
    registered = 0;
    port = _port;
}

void ServerThread::run()
{
    char buffer_target = 2;
    UdpServer udpServer(buffer_target);
    TcpServer tcpServer(buffer_target);
    udpServer.setPort(port);
    tcpServer.setPort(port);
    connect(&udpServer, &UdpServer::writeLog, this, &ServerThread::receiveLog);
    connect(&udpServer, &UdpServer::killMe, this, &ServerThread::quit);
    connect(&udpServer, &UdpServer::desynced, this, &ServerThread::desync);
    connect(&tcpServer, &TcpServer::register_player, &udpServer, &UdpServer::register_player);
    connect(&tcpServer, &TcpServer::register_player, this, &ServerThread::player_registered);
    connect(&tcpServer, &TcpServer::disconnect_player, &udpServer, &UdpServer::disconnect_player);
    connect(this, &ServerThread::sendClientNumber, &tcpServer, &TcpServer::getClientNumber);

    exec();

    udpServer.close();
    tcpServer.close();

    emit killServer(port);
}

void ServerThread::receiveLog(QString message, int _port)
{
    emit writeLog(message, _port);
}

void ServerThread::desync()
{
    emit desynced(port);
}

void ServerThread::player_registered(quint32, quint8, quint8)
{
    registered = 1;
}

void ServerThread::getClientNumber(int _port, int size)
{
    if (_port == port)
    {
        QTimer::singleShot(300000, this, &ServerThread::shouldKill);
        emit sendClientNumber(size);
    }
}

void ServerThread::shouldKill()
{
    if (registered == 0)
    {
        emit writeLog(QStringLiteral("Room Killed"), port);
        quit();
    }
}
