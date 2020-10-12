#include <QCoreApplication>
#include <QCommandLineParser>
#include "socketServer.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption name_opt("name", "Server name (required).");
    QCommandLineOption port_opt("baseport", "Base port. Defaults to 45000.");
    QCommandLineOption timestamp_opt("timestamps", "Whether log should include timestamps.");
    name_opt.setValueName("name");
    port_opt.setValueName("baseport");
    parser.addOption(name_opt);
    parser.addOption(port_opt);
    parser.addOption(timestamp_opt);

    parser.process(a);

    QString region;
    if (parser.isSet(name_opt))
        region = parser.value("name");
    else
    {
        printf("must set server name\n");
        return 0;
    }

    int port = 45000;
    if (parser.isSet(port_opt))
        port = parser.value("baseport").toInt();

    int timestamp = 0;
    if (parser.isSet(timestamp_opt))
        timestamp = 1;

    SocketServer socketServer(region, timestamp, port);

    return a.exec();
}
