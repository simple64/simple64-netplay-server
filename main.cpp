#include <QCoreApplication>
#include "socketServer.h"
#include "udpServer.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    SocketServer socketServer;

    UdpServer udpServer;
    udpServer.initSocket();

    return a.exec();
}
