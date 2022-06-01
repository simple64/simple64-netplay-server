#ifndef SOCKETSERVER_H
#define SOCKETSERVER_H

#include "serverThread.h"
#include <QWebSocketServer>
#include <QWebSocket>
#include <QJsonObject>
#include <QHash>
#include <QNetworkReply>
#include <QFile>
#include <QTimer>
#include <QUdpSocket>

#define NETPLAY_VER 9

class SocketServer : public QObject
{
    Q_OBJECT

public:
    explicit SocketServer(QString _region, int _timestamp, int _baseport, int _broadcast, QString _discord, QObject *parent = 0);
    ~SocketServer();

signals:
    void closed();
    void setClientNumber(int room_port, int size);
    void inputDelayChanged(int playerNum, int inputDelay);

private slots:
    void onNewConnection();
    void processBinaryMessage(QByteArray message);
    void socketDisconnected();
    void closeUdpServer(int port);
    void desyncMessage(int port);
    void deleteResponse(QNetworkReply *reply);
    void processBroadcast();
    void receiveLog(QString message, int port);
    void createLobbyFinished(QNetworkReply *reply);
private:
    void sendPlayers(int room_port);
    void createDiscord(QString room_name, QString game_name, int port, bool is_public);
    void writeLog(QString message, QString room_name, QString game_name, int port);
    void announceDiscord(QString channel, QString message);
    QWebSocketServer *webSocketServer;
    QHash<int, QPair<QJsonObject, ServerThread*>> rooms;
    QHash<int, QList<QPair<QWebSocket*, QPair<QString, int>>>> clients; //int = udp port, qlist<client socket, <client name, player num>>
    QHash<int, QPair<QString, QString>> discord;
    QString region;
    QString dev_channel;
    int timestamp;
    int baseport;
    int broadcast;
    QFile *log_file;
    QUdpSocket broadcastSocket;
    QString discord_bot;
    QStringList ban_strings;
    QStringList discord_channels;
};

#endif
