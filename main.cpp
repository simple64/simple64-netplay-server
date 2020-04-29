#include <QCoreApplication>
#include "udpServer.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    UdpServer* myServer = new UdpServer();
    myServer->initSocket();

    return a.exec();
}
