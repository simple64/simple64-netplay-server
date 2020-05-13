#include <QCoreApplication>
#include <QCommandLineParser>
#include "socketServer.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QCommandLineParser parser;
    parser.addPositionalArgument("discordbot", QCoreApplication::translate("main", "Discord bot auth token."));

    parser.process(a);
    const QStringList args = parser.positionalArguments();

    QString token;
    if (args.size() > 0)
        token = args.at(0);

    SocketServer socketServer(token);

    return a.exec();
}
