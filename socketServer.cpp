#include "socketServer.h"

SocketServer::SocketServer(QObject *parent)
    : QObject(parent)
{
    webSocketServer = new QWebSocketServer(QStringLiteral("m64p Netplay Server"), QWebSocketServer::NonSecureMode, this);

    if (webSocketServer->listen(QHostAddress::Any, 45000))
    {
        connect(webSocketServer, &QWebSocketServer::newConnection, this, &SocketServer::onNewConnection);
        connect(webSocketServer, &QWebSocketServer::closed, this, &SocketServer::closed);
    }
}

SocketServer::~SocketServer()
{
    webSocketServer->close();
    qDeleteAll(clients.begin(), clients.end());
}

void SocketServer::onNewConnection()
{
    QWebSocket *socket = webSocketServer->nextPendingConnection();

    connect(socket, &QWebSocket::textMessageReceived, this, &SocketServer::processTextMessage);
    connect(socket, &QWebSocket::binaryMessageReceived, this, &SocketServer::processBinaryMessage);
    connect(socket, &QWebSocket::disconnected, this, &SocketServer::socketDisconnected);

    clients << socket;
}

void SocketServer::processTextMessage(QString message)
{
    QWebSocket *client = qobject_cast<QWebSocket *>(sender());
    if (client)
    {
        client->sendTextMessage(message);
    }
}

void SocketServer::processBinaryMessage(QByteArray message)
{
    QWebSocket *client = qobject_cast<QWebSocket *>(sender());
    if (client)
    {
        client->sendBinaryMessage(message);
    }
}

void SocketServer::socketDisconnected()
{
    QWebSocket *client = qobject_cast<QWebSocket *>(sender());
    if (client)
    {
        clients.removeAll(client);
        client->deleteLater();
    }
}
