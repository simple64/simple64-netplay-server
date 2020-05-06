#ifndef SOCKETSERVER_H
#define SOCKETSERVER_H

#include "udpServer.h"
#include <QWebSocketServer>
#include <QWebSocket>
#include <QJsonObject>
#include <QHash>

class SocketServer : public QObject
{
    Q_OBJECT

public:
    explicit SocketServer(QObject *parent = 0);
    ~SocketServer();

signals:
    void closed();

private slots:
    void onNewConnection();
    void processBinaryMessage(QByteArray message);
    void socketDisconnected();
    void closeUdpServer(int port);
private:
    QWebSocketServer *webSocketServer;
    QHash<int, QPair<QJsonObject, UdpServer*>> rooms;
    QHash<int, QList<QPair<QWebSocket*, QPair<QString, int>>>> clients; //int = udp port, qlist<client socket, <client name, player num>>
};

#endif
