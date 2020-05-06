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
}

void SocketServer::onNewConnection()
{
    QWebSocket *socket = webSocketServer->nextPendingConnection();

    connect(socket, &QWebSocket::binaryMessageReceived, this, &SocketServer::processBinaryMessage);
    connect(socket, &QWebSocket::disconnected, this, &SocketServer::socketDisconnected);
}

void SocketServer::processBinaryMessage(QByteArray message)
{
    int i, j;
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
            room.insert("type", "send_room_create");
            clients[port].append(qMakePair(client, qMakePair(json.value("player_name").toString(), 1)));
        }
        else
        {
            room.insert("type", "message");
            room.insert("message", "Failed to create room");
        }
        json_doc = QJsonDocument(room);
        client->sendBinaryMessage(json_doc.toBinaryData());
    }
    else if (json.value("type").toString() == "get_rooms")
    {
        for (i = 0; i < rooms.size(); ++i)
        {
            room = rooms.at(i);
            if (room.value("password").toString().isEmpty())
                room.insert("protected", "No");
            else
                room.insert("protected", "Yes");
            room.remove("password");
            room.insert("type", "send_room");
            json_doc = QJsonDocument(room);
            client->sendBinaryMessage(json_doc.toBinaryData());
        }
    }
    else if (json.value("type").toString() == "join_room")
    {
        int accepted = 0;
        for (i = 0; i < rooms.size(); ++i)
        {
            if (json.value("port").toInt() == rooms[i].value("port").toInt())
            {
                if (!rooms[i].value("password").toString().isEmpty() &&
                   (rooms[i].value("password").toString() != json.value("password").toString()))
                {
                    accepted = -1; //bad password
                }
                else //no password
                {
                    room = rooms[i];
                    accepted = 1;
                    int room_port = room.value("port").toInt();
                    int player_num = 1;
                    for (j = 0; j < clients[room_port].size(); ++j)
                    {
                        if (clients[room_port][j].second.second == player_num)
                        {
                            ++player_num;
                            j = -1;
                        }
                    }
                    clients[room_port].append(qMakePair(client, qMakePair(json.value("player_name").toString(), player_num)));
                }
            }
        }
        room.insert("type", "accept_join");
        room.insert("accept", accepted);
        json_doc = QJsonDocument(room);
        client->sendBinaryMessage(json_doc.toBinaryData());
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

    clients.remove(port);
}

void SocketServer::socketDisconnected()
{
    int i, j;
    QWebSocket *client = qobject_cast<QWebSocket *>(sender());
    for (i = 0; i < clients.size(); ++i)
    {
        for (j = 0; j < clients[i].size(); ++j)
        {
            if (clients[i][j].first == client)
                clients[i].removeAt(j);
        }
    }
    if (client)
        client->deleteLater();
}
