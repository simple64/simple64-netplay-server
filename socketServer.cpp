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
    int i, room_port;
    QWebSocket *client = qobject_cast<QWebSocket *>(sender());
    QJsonDocument json_doc = QJsonDocument::fromBinaryData(message);
    QJsonObject json = json_doc.object();
    QJsonObject room;
    if (json.value("type").toString() == "create_room")
    {
        int port;
        for (port = 45001; port < 45011; ++port)
        {
            if (!rooms.contains(port))
            {
                UdpServer *server = new UdpServer(port);
                connect(server, &UdpServer::killMe, this, &SocketServer::closeUdpServer);
                room = json;
                room.remove("type");
                room.remove("player_name");
                room.insert("port", port);
                rooms[port] = qMakePair(room, server);
                room.insert("type", "send_room_create");
                room.insert("player_name", json.value("player_name").toString());
                clients[port].append(qMakePair(client, qMakePair(json.value("player_name").toString(), 1)));
                break;
            }
        }

        if (port == 45011)
        {
            room.insert("type", "message");
            room.insert("message", "Failed to create room");
        }

        json_doc = QJsonDocument(room);
        client->sendBinaryMessage(json_doc.toBinaryData());
    }
    else if (json.value("type").toString() == "get_rooms")
    {
        QHash<int, QPair<QJsonObject, UdpServer*>>::iterator iter;
        for (iter = rooms.begin(); iter != rooms.end(); ++iter)
        {
            room = iter.value().first;
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
        room_port = json.value("port").toInt();
        room = rooms[room_port].first;
        if (!room.value("password").toString().isEmpty() &&
           (room.value("password").toString() != json.value("password").toString()))
        {
            accepted = -1; //bad password
        }
        else //correct password
        {
            accepted = 1;
            int player_num = 1;
            for (i = 0; i < clients[room_port].size(); ++i)
            {
                if (clients[room_port][i].second.second == player_num)
                {
                    ++player_num;
                    i = -1;
                }
            }
            clients[room_port].append(qMakePair(client, qMakePair(json.value("player_name").toString(), player_num)));
        }
        room.remove("password");
        room.insert("player_name", json.value("player_name").toString());
        room.insert("type", "accept_join");
        room.insert("accept", accepted);
        json_doc = QJsonDocument(room);
        client->sendBinaryMessage(json_doc.toBinaryData());
    }
    else if (json.value("type").toString() == "request_players")
    {
        sendPlayers(json.value("port").toInt());
    }
    else if (json.value("type").toString() == "chat_message")
    {
        room.insert("type", "chat_update");
        room_port = json.value("port").toInt();
        QString message = json.value("player_name").toString() + ": " + json.value("message").toString();
        room.insert("message", message);
        json_doc = QJsonDocument(room);
        for (i = 0; i < clients[room_port].size(); ++i)
            clients[room_port][i].first->sendBinaryMessage(json_doc.toBinaryData());
    }
    else if (json.value("type").toString() == "start_game")
    {
        room.insert("type", "begin_game");
        room_port = json.value("port").toInt();
        rooms[room_port].first.insert("running", "true");
        json_doc = QJsonDocument(room);
        for (i = 0; i < clients[room_port].size(); ++i)
            clients[room_port][i].first->sendBinaryMessage(json_doc.toBinaryData());
    }
}

void SocketServer::sendPlayers(int room_port)
{
    QJsonObject room;
    QJsonDocument json_doc;
    int i;
    room.insert("type", "room_players");
    for (i = 0; i < clients[room_port].size(); ++i)
    {
        room.insert(QString::number(clients[room_port][i].second.second - 1), clients[room_port][i].second.first);
    }
    json_doc = QJsonDocument(room);
    for (i = 0; i < clients[room_port].size(); ++i)
    {
        clients[room_port][i].first->sendBinaryMessage(json_doc.toBinaryData());
    }

}

void SocketServer::closeUdpServer(int port)
{
    delete rooms[port].second;
    rooms.remove(port);
    clients.remove(port);
}

void SocketServer::socketDisconnected()
{
    int j;
    int should_delete = 0;
    QWebSocket *client = qobject_cast<QWebSocket *>(sender());
    QHash<int, QList<QPair<QWebSocket*, QPair<QString, int>>>> ::iterator iter;
    for (iter = clients.begin(); iter != clients.end(); ++iter)
    {
        for (j = 0; j < iter.value().size(); ++j)
        {
            if (iter.value()[j].first == client)
            {
                iter.value().removeAt(j);
                sendPlayers(iter.key());
            }

            if (iter.value().isEmpty() && !rooms[iter.key()].first.contains("running")) //no more clients connected to room
            {
                should_delete = iter.key();
            }
        }
    }

    if (should_delete)
        closeUdpServer(should_delete);

    if (client)
        client->deleteLater();
}
