#ifndef SOCKETSERVER_H
#define SOCKETSERVER_H

#include "udpServer.h"
#include <QWebSocketServer>
#include <QWebSocket>
#include <QJsonObject>

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
    QList<QWebSocket *> clients;
    QList<UdpServer *> servers;
    QList<QJsonObject> rooms;
};

#endif
