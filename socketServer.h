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

#define NETPLAY_VER 11

class SocketServer : public QObject
{
    Q_OBJECT

public:
    explicit SocketServer(QString _region, int _timestamp, int _baseport, int _broadcast, QObject *parent = 0);
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
private:
    void sendPlayers(int room_port);
    void createDiscord(QString room_name, QString game_name, int port, bool is_public);
    void writeLog(QString message, QString room_name, QString game_name, int port);
    void announceDiscord(QString channel, QString message);
    QWebSocketServer *webSocketServer;
    QHash<int, QPair<QJsonObject, ServerThread*>> rooms;
    QHash<int, QList<QPair<QWebSocket*, QPair<QString, int>>>> clients; //int = udp port, qlist<client socket, <client name, player num>>
    QString region;
    QString dev_channel;
    int timestamp;
    int baseport;
    int broadcast;
    QFile *log_file;
    QUdpSocket broadcastSocket;
    QStringList ban_strings;
    QStringList discord_channels;
};

#endif
