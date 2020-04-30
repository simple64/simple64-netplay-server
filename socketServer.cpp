#include "socketServer.h"
#include <QJsonObject>
#include <QJsonDocument>

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

    connect(socket, &QWebSocket::binaryMessageReceived, this, &SocketServer::processBinaryMessage);
    connect(socket, &QWebSocket::disconnected, this, &SocketServer::socketDisconnected);

    clients << socket;
}

void SocketServer::processBinaryMessage(QByteArray message)
{
    QJsonDocument json_doc = QJsonDocument::fromBinaryData(message);
    QJsonObject json = json_doc.object();
    if (json.value("type").toString() == "create_room")
    {
        int port = 45001;
        for (int i = 0; i < servers.size(); ++i)
        {
            if (servers.at(i)->getPort() == port)
            {
                ++port;
                i = -1;
            }
        }
        if (port < 45011)
        {
            UdpServer *server = new UdpServer(port);
            connect(server, &UdpServer::killMe, this, &SocketServer::closeUdpServer);
            servers << server;
        }
    }
//    QWebSocket *client = qobject_cast<QWebSocket *>(sender());
//    if (client)
//    {
//        client->sendBinaryMessage(message);
//    }
}

void SocketServer::closeUdpServer(int port)
{
    for (int i = 0; i < servers.size(); ++i)
    {
        if (servers.at(i)->getPort() == port)
        {
            delete servers.at(i);
            servers.removeAt(i);
        }
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
