#include <QCoreApplication>
#include "socketServer.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    SocketServer server;

    return a.exec();
}
