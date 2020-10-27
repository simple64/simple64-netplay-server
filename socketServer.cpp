#include "socketServer.h"
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkDatagram>
#include <QNetworkAddressEntry>
#include <QCoreApplication>
#include <QDir>

SocketServer::SocketServer(QString _region, int _timestamp, int _baseport, QObject *parent)
    : QObject(parent)
{
    webSocketServer = new QWebSocketServer(QStringLiteral("m64p Netplay Server"), QWebSocketServer::NonSecureMode, this);
    broadcastSocket.bind(45000, QUdpSocket::ShareAddress);
    connect(&broadcastSocket, &QUdpSocket::readyRead, this, &SocketServer::processBroadcast);

    if (webSocketServer->listen(QHostAddress::Any, _baseport))
    {
        connect(webSocketServer, &QWebSocketServer::newConnection, this, &SocketServer::onNewConnection);
        connect(webSocketServer, &QWebSocketServer::closed, this, &SocketServer::closed);
    }

    baseport = _baseport;
    region = _region;
    timestamp = _timestamp;
    QDir AppPath(QCoreApplication::applicationDirPath());
    log_file = new QFile(AppPath.absoluteFilePath("m64p_server_log.txt"), this);
    log_file->open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append);
    writeLog("Server started", "None", "None", baseport);
}

SocketServer::~SocketServer()
{
    log_file->close();
    webSocketServer->close();
    broadcastSocket.close();
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
            json.insert(region, QStringLiteral("ws://") + ip.toString() + QStringLiteral(":") + QString::number(baseport));
            QJsonDocument json_doc(json);
            broadcastSocket.writeDatagram(json_doc.toJson(), datagram.senderAddress(), datagram.senderPort());
        }
    }
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
            for (port = (baseport + 1); port < (baseport + 11); ++port)
            {
                if (!rooms.contains(port))
                {
                    writeLog("creating room", json.value("room_name").toString(), json.value("game_name").toString(), port);

                    ServerThread *serverThread = new ServerThread(port, this);
                    connect(serverThread, &ServerThread::writeLog, this, &SocketServer::receiveLog);
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

            if (port == (baseport + 11))
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
        room = rooms.value(room_port).first;
        int duplicate_name = 0;
        for (i = 0; i < clients[room_port].size(); ++i)
        {
            if (json.value("player_name").toString() == clients.value(room_port).value(i).second.first)
                duplicate_name = 1;
        }
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
        else if (duplicate_name)
        {
            accepted = 4; //duplicate player name
        }
        else //correct password
        {
            int player_num = 1;
            for (i = 0; i < clients[room_port].size(); ++i)
            {
                if (clients.value(room_port).value(i).second.second == player_num)
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
            clients.value(room_port).value(i).first->sendBinaryMessage(json_doc.toJson());
    }
    else if (json.value("type").toString() == "start_game")
    {
        room.insert("type", "begin_game");
        room_port = json.value("port").toInt();
        emit setClientNumber(room_port, clients[room_port].size());
        rooms[room_port].first.insert("running", "true");
        writeLog("starting game", rooms.value(room_port).first.value("room_name").toString(), rooms.value(room_port).first.value("game_name").toString(), room_port);
        json_doc = QJsonDocument(room);
        for (i = 0; i < clients[room_port].size(); ++i)
            clients.value(room_port).value(i).first->sendBinaryMessage(json_doc.toJson());
    }
    else if (json.value("type").toString() == "get_motd")
    {
        room.insert("type", "send_motd");
        room.insert("message", QStringLiteral("Join <a href=\"https://discord.gg/B5svWhR\">The Discord Server</a> for voice chat!"));
        json_doc = QJsonDocument(room);
        client->sendBinaryMessage(json_doc.toJson());
    }
}

void SocketServer::createDiscord(QString room_name, QString game_name, bool is_public)
{
    QString type = is_public ? QStringLiteral("public") : QStringLiteral("private");
    QString message = "New " + type + " netplay room running in " + region + ": **" + room_name + "** has been created! Come play " + game_name;
    QString path;
    //Annouce room
    if (is_public)
    {
        char env_var[80] = "M64P_CHANNEL_";
        for (int i = 0; i < 10; ++i)
        {
            env_var[13] = '0' + i;
            env_var[14] = '\0';
            path = qEnvironmentVariable(env_var);
            if (!path.isEmpty())
                announceDiscord(path, message);
        }
    }
    path = qEnvironmentVariable("M64P_DEV_CHANNEL");
    if (!path.isEmpty())
        announceDiscord(path, message); //m64p discord dev channel
}

void SocketServer::announceDiscord(QString channel, QString message)
{
    QUrl path = QUrl(channel);
    QNetworkRequest request(path);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject json;
    json.insert("content", message);
    QNetworkAccessManager *nam = new QNetworkAccessManager(this);
    connect(nam, &QNetworkAccessManager::finished, this, &SocketServer::deleteResponse);
    nam->post(request, QJsonDocument(json).toJson());
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
        room.insert(QString::number(clients.value(room_port).value(i).second.second - 1), clients.value(room_port).value(i).second.first);
    }
    json_doc = QJsonDocument(room);
    for (i = 0; i < clients[room_port].size(); ++i)
    {
        clients.value(room_port).value(i).first->sendBinaryMessage(json_doc.toJson());
    }

}

void SocketServer::closeUdpServer(int port)
{
    writeLog("deleting room", rooms.value(port).first.value("room_name").toString(), rooms.value(port).first.value("game_name").toString(), port);

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
            if (iter.value().value(j).first == client)
            {
                iter.value().removeAt(j);
                if (!rooms.value(iter.key()).first.contains("running"))
                    sendPlayers(iter.key());
            }

            if (iter.value().isEmpty() && !rooms.value(iter.key()).first.contains("running")) //no more clients connected to room
            {
                should_delete = iter.key();
            }
        }
    }

    if (should_delete)
        rooms.value(should_delete).second->quit();

    if (client)
        client->deleteLater();
}

void SocketServer::writeLog(QString message, QString room_name, QString game_name, int port)
{
    QTextStream out(log_file);
    if (timestamp)
    {
        QString currentDateTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
        out << currentDateTime << ": ";
    }
    out << QStringLiteral("room: ");
    out << room_name;
    out << QStringLiteral(", game: ");
    out << game_name;
    out << QStringLiteral(", port: ");
    out << QString::number(port);
    out << QStringLiteral(", ");
    out << message;
    out << endl;
    log_file->flush();
}

void SocketServer::desyncMessage(int port)
{
    QString room_name = rooms.value(port).first.value("room_name").toString();
    QString game_name = rooms.value(port).first.value("game_name").toString();
    writeLog("game desynced", room_name, game_name, port);
    QString message = "Desync in netplay room running in " + region + ": **" + room_name + "** game: " + game_name;
    QString path = qEnvironmentVariable("M64P_DEV_CHANNEL");
    if (!path.isEmpty())
        announceDiscord(path, message); //m64p discord dev channel
}

void SocketServer::receiveLog(QString message, int port)
{
    QString room_name = rooms.value(port).first.value("room_name").toString();
    QString game_name = rooms.value(port).first.value("game_name").toString();
    writeLog(message, room_name, game_name, port);
}
