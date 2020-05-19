#ifndef SOCKETSERVER_H
#define SOCKETSERVER_H

#include "serverThread.h"
#include <QWebSocketServer>
#include <QWebSocket>
#include <QJsonObject>
#include <QHash>
#include <QNetworkReply>
#include <QFile>

#define NETPLAY_VER 4

class SocketServer : public QObject
{
    Q_OBJECT

public:
    explicit SocketServer(QString _token, QObject *parent = 0);
    ~SocketServer();

signals:
    void closed();

private slots:
    void onNewConnection();
    void processBinaryMessage(QByteArray message);
    void socketDisconnected();
    void closeUdpServer(int port);
    void desyncMessage(int port);
    void createResponse(QNetworkReply *reply);
    void deleteResponse(QNetworkReply *reply);
    void inviteResponse(QNetworkReply *reply);
private:
    void sendPlayers(int room_port);
    void createDiscord(QString room_name);
    void deleteDiscord(QString room_name);
    void writeLog(QString message, QString room_name, QString game_name);
    QWebSocketServer *webSocketServer;
    QHash<int, QPair<QJsonObject, ServerThread*>> rooms;
    QHash<int, QList<QPair<QWebSocket*, QPair<QString, int>>>> clients; //int = udp port, qlist<client socket, <client name, player num>>
    QHash<QString, QPair<QString, QString>> discord; // room name, <channel id, invite id>
    QString token;
    QFile *log_file;
};

#endif
