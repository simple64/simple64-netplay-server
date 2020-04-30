#include "socketServer.h"
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
    int i;
    QWebSocket *client = qobject_cast<QWebSocket *>(sender());
    QJsonDocument json_doc = QJsonDocument::fromBinaryData(message);
    QJsonObject json = json_doc.object();
    QJsonObject room;
    if (json.value("type").toString() == "create_room")
    {
        int port = 45001;
        for (i = 0; i < servers.size(); ++i)
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
            room = json;
            room.remove("type");
            room.remove("player_name");
            room.insert("port", port);
            rooms << room;

            json_doc = QJsonDocument(room);
            client->sendBinaryMessage(json_doc.toBinaryData());
        }
    }
    else if (json.value("type").toString() == "get_rooms")
    {
        for (i = 0; i < rooms.size(); ++i)
        {
            room = rooms.at(i);
            room.remove("password");
            json_doc = QJsonDocument(room);
            client->sendBinaryMessage(json_doc.toBinaryData());
        }
    }
}

void SocketServer::closeUdpServer(int port)
{
    int i;

    for (i = 0; i < servers.size(); ++i)
    {
        if (servers.at(i)->getPort() == port)
        {
            delete servers.at(i);
            servers.removeAt(i);
        }
    }

    for (i = 0; i < rooms.size(); ++i)
    {
        if (rooms.at(i).value("port").toInt() == port)
        {
            rooms.removeAt(i);
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
