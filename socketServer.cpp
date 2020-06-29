#include "socketServer.h"
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkDatagram>
#include <QNetworkAddressEntry>
#include <QCoreApplication>
#include <QDir>

SocketServer::SocketServer(QString _token, QString _region, QObject *parent)
    : QObject(parent)
{
    webSocketServer = new QWebSocketServer(QStringLiteral("m64p Netplay Server"), QWebSocketServer::NonSecureMode, this);
    broadcastSocket.bind(45000, QUdpSocket::ShareAddress);
    connect(&broadcastSocket, &QUdpSocket::readyRead, this, &SocketServer::processBroadcast);

    if (webSocketServer->listen(QHostAddress::Any, 45000))
    {
        connect(webSocketServer, &QWebSocketServer::newConnection, this, &SocketServer::onNewConnection);
        connect(webSocketServer, &QWebSocketServer::closed, this, &SocketServer::closed);
    }

    token = _token;
    region = _region;
    QDir AppPath(QCoreApplication::applicationDirPath());
    log_file = new QFile(AppPath.absoluteFilePath("m64p_server_log.txt"), this);
    log_file->open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append);
    writeLog("Server started", "None", "None");

    if (!token.isEmpty())
    {
        discordCounter = -1;
        connect(&discordClient, &QWebSocket::textMessageReceived, this, &SocketServer::discordConnected);
        connect(&discordClient, &QWebSocket::disconnected, this, &SocketServer::discordReconnect);
        discordClient.open(QUrl("wss://gateway.discord.gg/?v=6&encoding=json"));
        connect(&discordTimer, &QTimer::timeout, this, &SocketServer::discordHeartbeat);
    }
}

SocketServer::~SocketServer()
{
    log_file->close();
    webSocketServer->close();
    broadcastSocket.close();
    if (!token.isEmpty())
    {
        discordClient.close();
        discordTimer.stop();
    }
}

void SocketServer::processBroadcast()
{
    while (broadcastSocket.hasPendingDatagrams())
    {
        QNetworkDatagram datagram = broadcastSocket.receiveDatagram();
        QByteArray incomingData = datagram.data();
        if (incomingData.at(0) == 1)
        {
            QNetworkInterface inter = QNetworkInterface::interfaceFromIndex(datagram.interfaceIndex());
            QList<QNetworkAddressEntry> addresses = inter.addressEntries();
            QHostAddress ip = addresses.at(0).ip();
            QJsonObject json;
            json.insert(region, QStringLiteral("ws://") + ip.toString() + QStringLiteral(":45000"));
            QJsonDocument json_doc(json);
            broadcastSocket.writeDatagram(json_doc.toJson(), datagram.senderAddress(), datagram.senderPort());
        }
    }
}

void SocketServer::discordReconnect()
{
    discordTimer.stop();
    discordCounter = -1;
    discordClient.open(QUrl("wss://gateway.discord.gg/?v=6&encoding=json"));
}

void SocketServer::discordConnected(QString message)
{
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    QJsonObject json = doc.object();
    if (json.contains("s"))
    {
        if (json.value("s").isNull())
            discordCounter = -1;
        else
            discordCounter = json.value("s").toInt();
    }
    if (json.value("op").toInt() == 10) //hello
    {
        int heartbeat = json.value("d").toObject().value("heartbeat_interval").toInt();
        discordTimer.start(heartbeat);

        QJsonObject ident;
        ident.insert("op", 2);
        QJsonObject d;
        QJsonObject prop;
        d.insert("token", token);
        prop.insert("$os", "linux");
        prop.insert("$browser", "qt");
        prop.insert("$device", "aws");
        d.insert("properties", prop);
        ident.insert("d", d);
        QJsonDocument ident_doc(ident);
        discordClient.sendTextMessage(ident_doc.toJson());
    }
}

void SocketServer::discordHeartbeat()
{
    QJsonObject json;
    json.insert("op", 1);
    if (discordCounter == -1)
    {
        json.insert("d", QJsonValue());
    }
    else
        json.insert("d", discordCounter);
    QJsonDocument json_doc(json);
    discordClient.sendTextMessage(json_doc.toJson());
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
    QJsonDocument json_doc = QJsonDocument::fromJson(message);
    QJsonObject json = json_doc.object();
    QJsonObject room;
    if (json.value("type").toString() == "create_room")
    {
        if (json.value("netplay_version").toInt() != NETPLAY_VER)
        {
            room.insert("type", "message");
            room.insert("message", "client and server not at same version. Visit <a href=\"https://m64p.github.io\">here</a> to update");
            json_doc = QJsonDocument(room);
            client->sendBinaryMessage(json_doc.toJson());
        }
        else
        {
            int port;
            for (port = 45001; port < 45011; ++port)
            {
                if (!rooms.contains(port) && !discord.contains(json.value("room_name").toString()))
                {
                    writeLog("creating room", json.value("room_name").toString(), json.value("game_name").toString());

                    ServerThread *serverThread = new ServerThread(port, this);
                    connect(serverThread, &ServerThread::killServer, this, &SocketServer::closeUdpServer);
                    connect(serverThread, &ServerThread::desynced, this, &SocketServer::desyncMessage);
                    connect(serverThread, &ServerThread::finished, serverThread, &ServerThread::deleteLater);
                    connect(this, &SocketServer::setClientNumber, serverThread, &ServerThread::getClientNumber);
                    serverThread->start();
                    room = json;
                    room.remove("type");
                    room.remove("player_name");
                    room.insert("port", port);
                    rooms[port] = qMakePair(room, serverThread);
                    room.insert("type", "send_room_create");
                    room.insert("player_name", json.value("player_name").toString());
                    clients[port].append(qMakePair(client, qMakePair(json.value("player_name").toString(), 1)));

                    createDiscord(json.value("room_name").toString(), json.value("game_name").toString(), json.value("password").toString().isEmpty());
                    break;
                }
            }

            if (port == 45011)
            {
                room.insert("type", "message");
                room.insert("message", "Failed to create room");
            }

            json_doc = QJsonDocument(room);
            client->sendBinaryMessage(json_doc.toJson());
        }
    }
    else if (json.value("type").toString() == "get_rooms")
    {
        if (json.value("netplay_version").toInt() != NETPLAY_VER)
        {
            room.insert("type", "message");
            room.insert("message", "client and server not at same version. Visit <a href=\"https://m64p.github.io\">here</a> to update");
            json_doc = QJsonDocument(room);
            client->sendBinaryMessage(json_doc.toJson());
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
                client->sendBinaryMessage(json_doc.toJson());
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
        else if (clients[room_port].size() >= 4)
        {
            accepted = 3; //room is full
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
        client->sendBinaryMessage(json_doc.toJson());
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
            clients[room_port][i].first->sendBinaryMessage(json_doc.toJson());
    }
    else if (json.value("type").toString() == "start_game")
    {
        room.insert("type", "begin_game");
        room_port = json.value("port").toInt();
        emit setClientNumber(room_port, clients[room_port].size());
        rooms[room_port].first.insert("running", "true");
        writeLog("starting game", rooms[room_port].first.value("room_name").toString(), rooms[room_port].first.value("game_name").toString());
        json_doc = QJsonDocument(room);
        for (i = 0; i < clients[room_port].size(); ++i)
            clients[room_port][i].first->sendBinaryMessage(json_doc.toJson());
    }
    else if (json.value("type").toString() == "get_discord")
    {
        if (discord.contains(json.value("room_name").toString()))
        {
            room.insert("type", "discord_link");
            room.insert("link", QStringLiteral("discord.gg/") + discord[json.value("room_name").toString()].second);
            json_doc = QJsonDocument(room);
            client->sendBinaryMessage(json_doc.toJson());
        }
    }
}

void SocketServer::createDiscord(QString room_name, QString game_name, bool is_public)
{
    if (token.isEmpty())
        return;

    //Create voice channel
    QNetworkRequest request(QUrl("https://discord.com/api/guilds/709975510981279765/channels"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QString auth = "Bot " + token;
    request.setRawHeader("Authorization", auth.toLocal8Bit());
    QJsonObject json;
    json.insert("name", room_name);
    json.insert("type", 2);
    QNetworkAccessManager *nam = new QNetworkAccessManager(this);
    connect(nam, &QNetworkAccessManager::finished, this, &SocketServer::createResponse);
    nam->post(request, QJsonDocument(json).toJson());

    QString type = is_public ? QStringLiteral("public") : QStringLiteral("private");
    QString message = "New " + type + " netplay room running in " + region + ": **" + room_name + "** has been created! Come play " + game_name;
    //Annouce room
    if (is_public)
    {
        announceDiscord("714342667814830111", message); //Discord64
        announceDiscord("709975511484334083", message); //m64p discord
    }
    announceDiscord("716049124188749845", message); //m64p discord dev channel
}

void SocketServer::announceDiscord(QString channel, QString message)
{
    QNetworkRequest request(QUrl("https://discord.com/api/channels/" + channel + "/messages"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QString auth = "Bot " + token;
    request.setRawHeader("Authorization", auth.toLocal8Bit());
    QJsonObject json;
    json.insert("content", message);
    QNetworkAccessManager *nam = new QNetworkAccessManager(this);
    connect(nam, &QNetworkAccessManager::finished, this, &SocketServer::deleteResponse);
    nam->post(request, QJsonDocument(json).toJson());
}

void SocketServer::createResponse(QNetworkReply *reply)
{
    QJsonDocument json_doc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject json = json_doc.object();

    if (json.contains("name") && json.contains("id"))
    {
        discord[json.value("name").toString()].first = json.value("id").toString();

        QNetworkRequest request(QUrl("https://discord.com/api/channels/" + json.value("id").toString() + "/invites"));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        QString auth = "Bot " + token;
        request.setRawHeader("Authorization", auth.toLocal8Bit());
        QJsonObject json_invite;
        QNetworkAccessManager *nam = new QNetworkAccessManager(this);
        connect(nam, &QNetworkAccessManager::finished, this, &SocketServer::inviteResponse);
        nam->post(request, QJsonDocument(json_invite).toJson());
    }

    reply->deleteLater();
}

void SocketServer::inviteResponse(QNetworkReply *reply)
{
    QJsonDocument json_doc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject json = json_doc.object();

    if (json.contains("channel") && json.contains("code"))
    {
        QString room_name = json.value("channel").toObject().value("name").toString();
        discord[room_name].second = json.value("code").toString();
    }

    reply->deleteLater();
}

void SocketServer::deleteDiscord(QString room_name)
{
    if (token.isEmpty())
        return;

    if (discord.contains(room_name))
    {
        QNetworkRequest request(QUrl("https://discord.com/api/channels/" + discord[room_name].first));
        QString auth = "Bot " + token;
        request.setRawHeader("Authorization", auth.toLocal8Bit());
        QNetworkAccessManager *nam = new QNetworkAccessManager(this);
        connect(nam, &QNetworkAccessManager::finished, this, &SocketServer::deleteResponse);
        nam->deleteResource(request);
        discord.remove(room_name);
    }
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
        clients[room_port][i].first->sendBinaryMessage(json_doc.toJson());
    }

}

void SocketServer::closeUdpServer(int port)
{
    writeLog("deleting room", rooms[port].first.value("room_name").toString(), rooms[port].first.value("game_name").toString());

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
                if (!rooms[iter.key()].first.contains("running"))
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

void SocketServer::writeLog(QString message, QString room_name, QString game_name)
{
    QTextStream out(log_file);
    QString currentDateTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    out << currentDateTime;
    out << QStringLiteral(": room: ");
    out << room_name;
    out << QStringLiteral(", game: ");
    out << game_name;
    out << QStringLiteral(", ");
    out << message;
    out << endl;
    log_file->flush();
}

void SocketServer::desyncMessage(int port)
{
    QString room_name = rooms[port].first.value("room_name").toString();
    QString game_name = rooms[port].first.value("game_name").toString();
    writeLog("game desynced", room_name, game_name);
    QString message = "Desync in netplay room running in " + region + ": **" + room_name + "** game: " + game_name;
    announceDiscord("716049124188749845", message); //m64p discord dev channel
}
