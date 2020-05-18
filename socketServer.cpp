#include "socketServer.h"
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QCoreApplication>
#include <QDir>

SocketServer::SocketServer(QString _token, QObject *parent)
    : QObject(parent)
{
    webSocketServer = new QWebSocketServer(QStringLiteral("m64p Netplay Server"), QWebSocketServer::NonSecureMode, this);

    if (webSocketServer->listen(QHostAddress::Any, 45000))
    {
        connect(webSocketServer, &QWebSocketServer::newConnection, this, &SocketServer::onNewConnection);
        connect(webSocketServer, &QWebSocketServer::closed, this, &SocketServer::closed);
    }

    token = _token;
    QDir AppPath(QCoreApplication::applicationDirPath());
    log_file = new QFile(AppPath.absoluteFilePath("m64p_server_log.txt"), this);
    log_file->open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append);
}

SocketServer::~SocketServer()
{
    log_file->close();
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
        if (json.value("netplay_version").toInt() != NETPLAY_VER)
        {
            room.insert("type", "message");
            room.insert("message", "client and server not at same version. Visit <a href=\"https://m64p.github.io\">here</a> to update");
            json_doc = QJsonDocument(room);
            client->sendBinaryMessage(json_doc.toBinaryData());
        }
        else
        {
            int port;
            for (port = 45001; port < 45011; ++port)
            {
                if (!rooms.contains(port) && !discord.contains(json.value("room_name").toString()))
                {
                    QTextStream out(log_file);
                    QString currentDateTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
                    out << currentDateTime;
                    out << QStringLiteral(": creating room: ");
                    out << json.value("room_name").toString();
                    out << QStringLiteral(", game: ");
                    out << json.value("game_name").toString();
                    out << endl;
                    log_file->flush();
                    ServerThread *serverThread = new ServerThread(port, this);
                    connect(serverThread, SIGNAL(killServer(int)), this, SLOT(closeUdpServer(int)));
                    connect(serverThread, &QThread::finished, serverThread, &QObject::deleteLater);
                    serverThread->start();
                    room = json;
                    room.remove("type");
                    room.remove("player_name");
                    room.insert("port", port);
                    rooms[port] = qMakePair(room, serverThread);
                    room.insert("type", "send_room_create");
                    room.insert("player_name", json.value("player_name").toString());
                    clients[port].append(qMakePair(client, qMakePair(json.value("player_name").toString(), 1)));

                    createDiscord(json.value("room_name").toString());
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
    }
    else if (json.value("type").toString() == "get_rooms")
    {
        if (json.value("netplay_version").toInt() != NETPLAY_VER)
        {
            room.insert("type", "message");
            room.insert("message", "client and server not at same version. Visit <a href=\"https://m64p.github.io\">here</a> to update");
            json_doc = QJsonDocument(room);
            client->sendBinaryMessage(json_doc.toBinaryData());
        }
        else
        {
            QHash<int, QPair<QJsonObject, ServerThread*>>::iterator iter;
            for (iter = rooms.begin(); iter != rooms.end(); ++iter)
            {
                room = iter.value().first;
                if (room.contains("running"))
                    continue;

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
    }
    else if (json.value("type").toString() == "join_room")
    {
        int accepted = 0;
        room_port = json.value("port").toInt();
        room = rooms[room_port].first;
        if (!room.value("password").toString().isEmpty() &&
           (room.value("password").toString() != json.value("password").toString()))
        {
            accepted = 1; //bad password
        }
        else if (room.value("client_sha").toString() != json.value("client_sha").toString())
        {
            accepted = 2; //client versions do not match
        }
        else //correct password
        {
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
    else if (json.value("type").toString() == "get_discord")
    {
        if (discord.contains(json.value("room_name").toString()))
        {
            room.insert("type", "discord_link");
            room.insert("link", QStringLiteral("discord.gg/") + discord[json.value("room_name").toString()].second);
            json_doc = QJsonDocument(room);
            client->sendBinaryMessage(json_doc.toBinaryData());
        }
    }
}

void SocketServer::createDiscord(QString room_name)
{
    if (token.isEmpty())
        return;

    QNetworkRequest request(QUrl("https://discord.com/api/guilds/709975510981279765/channels"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QString auth = "Bot " + token;
    request.setRawHeader("Authorization", auth.toLocal8Bit());
    QJsonObject json;
    json.insert("name", room_name);
    json.insert("type", 2);
    QNetworkAccessManager *nam = new QNetworkAccessManager(this);
    connect(nam, SIGNAL(finished(QNetworkReply*)),
            SLOT(createResponse(QNetworkReply*)));
    nam->post(request, QJsonDocument(json).toJson());
}

void SocketServer::createResponse(QNetworkReply *reply)
{
    QJsonDocument json_doc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject json = json_doc.object();
    discord[json.value("name").toString()].first = json.value("id").toString();
    reply->deleteLater();

    QNetworkRequest request(QUrl("https://discord.com/api/channels/" + json.value("id").toString() + "/invites"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QString auth = "Bot " + token;
    request.setRawHeader("Authorization", auth.toLocal8Bit());
    QJsonObject json_invite;
    json.insert("temporary", true);
    QNetworkAccessManager *nam = new QNetworkAccessManager(this);
    connect(nam, SIGNAL(finished(QNetworkReply*)),
            SLOT(inviteResponse(QNetworkReply*)));
    nam->post(request, QJsonDocument(json_invite).toJson());
}

void SocketServer::inviteResponse(QNetworkReply *reply)
{
    QJsonDocument json_doc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject json = json_doc.object();

    QString room_name = json.value("channel").toObject().value("name").toString();
    discord[room_name].second = json.value("code").toString();

    reply->deleteLater();
}

void SocketServer::deleteDiscord(QString room_name)
{
    if (token.isEmpty())
        return;

    QNetworkRequest request(QUrl("https://discord.com/api/channels/" + discord[room_name].first));
    QString auth = "Bot " + token;
    request.setRawHeader("Authorization", auth.toLocal8Bit());
    QNetworkAccessManager *nam = new QNetworkAccessManager(this);
    connect(nam, SIGNAL(finished(QNetworkReply*)),
            SLOT(deleteResponse(QNetworkReply*)));
    nam->deleteResource(request);
    discord.remove(room_name);
}

void SocketServer::deleteResponse(QNetworkReply *reply)
{
    reply->deleteLater();
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
    QTextStream out(log_file);
    QString currentDateTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    out << currentDateTime;
    out << QStringLiteral(": deleting room: ");
    out << rooms[port].first.value("room_name").toString();
    out << QStringLiteral(", game: ");
    out << rooms[port].first.value("game_name").toString();
    out << endl;
    log_file->flush();

    deleteDiscord(rooms[port].first.value("room_name").toString());
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
        rooms[should_delete].second->quit();

    if (client)
        client->deleteLater();
}
