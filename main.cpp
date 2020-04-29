#include <QCoreApplication>
#include "tcpServer.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    TcpServer server;
    server.listen(QHostAddress::Any, 45000);

    return a.exec();
}
