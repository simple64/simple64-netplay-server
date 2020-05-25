#include <QCoreApplication>
#include <QCommandLineParser>
#include "socketServer.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QCommandLineParser parser;
    parser.addPositionalArgument("discordbot", QCoreApplication::translate("main", "Discord bot auth token."));
    parser.addPositionalArgument("region", QCoreApplication::translate("main", "Server region."));

    parser.process(a);
    const QStringList args = parser.positionalArguments();

    QString token;
    QString region;
    if (args.size() > 0)
        token = args.at(0);
    if (args.size() > 1)
        region = args.at(1);

    SocketServer socketServer(token, region);

    return a.exec();
}
