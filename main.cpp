#include <QCoreApplication>
#include <QCommandLineParser>
#include "socketServer.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption discord_opt("discord", "Discord bot auth token (optional).");
    discord_opt.setValueName("bot token");
    QCommandLineOption name_opt("name", "Server name (required).");
    name_opt.setValueName("name");
    parser.addOption(discord_opt);
    parser.addOption(name_opt);

    parser.process(a);

    QString token;
    QString region;
    if (parser.isSet(discord_opt))
        token = parser.value("discord");
    if (parser.isSet(name_opt))
        region = parser.value("name");
    else
    {
        printf("must set server name\n");
        return 0;
    }

    SocketServer socketServer(token, region);

    return a.exec();
}
