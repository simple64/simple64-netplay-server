#include <QCoreApplication>
#include <QCommandLineParser>
#include "socketServer.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption name_opt("name", "Server name (required).");
    name_opt.setValueName("name");
    parser.addOption(name_opt);

    parser.process(a);

    QString region;
    if (parser.isSet(name_opt))
        region = parser.value("name");
    else
    {
        printf("must set server name\n");
        return 0;
    }

    SocketServer socketServer(region);

    return a.exec();
}
