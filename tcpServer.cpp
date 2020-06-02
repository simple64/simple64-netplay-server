#include "tcpServer.h"
#include <QtEndian>

TcpServer::TcpServer(QObject *parent)
    : QTcpServer(parent)
{
    client_number = 0;
    settings.clear();
    gliden64_settings.clear();
    connect(this, &QTcpServer::newConnection, this, &TcpServer::onNewConnection);
}

TcpServer::~TcpServer()
{
    close();
}

void TcpServer::getClientNumber(int size)
{
    client_number = size;
}

void TcpServer::setPort(int port)
{
    listen(QHostAddress::Any, port);
}

void TcpServer::onNewConnection()
{
    ClientHandler *clientH = new ClientHandler(nextPendingConnection(), this);
    connect(clientH, &ClientHandler::reg_player, this, &TcpServer::reg_player);
    connect(clientH, &ClientHandler::playerDisconnect, this, &TcpServer::playerDisconnect);
}

void TcpServer::reg_player(uint32_t reg_id, uint8_t playerNum, uint8_t plugin)
{
    emit register_player(reg_id, playerNum, plugin);
}

void TcpServer::playerDisconnect(uint32_t reg_id)
{
    emit disconnect_player(reg_id);
}

ClientHandler::ClientHandler(QTcpSocket *_socket, QObject *parent)
    : QObject(parent)
{
    server = (TcpServer*)parent;
    socket = _socket;
    connect(socket, &QTcpSocket::disconnected, this, &ClientHandler::deleteLater);
    connect(socket, &QTcpSocket::readyRead, this, &ClientHandler::readData);
    filename.clear();
    request = 255;
    filesize = 0;
    data.clear();
    connect(&fileTimer, &QTimer::timeout, this, &ClientHandler::sendFile);
    connect(&settingTimer, &QTimer::timeout, this, &ClientHandler::sendSettings);
    connect(&gliden64_settingTimer, &QTimer::timeout, this, &ClientHandler::sendGliden64Settings);
    connect(&regTimer, &QTimer::timeout, this, &ClientHandler::sendReg);
}

void ClientHandler::readData()
{
    data.append(socket->readAll());
    int process = 1;
    while (process)
    {
        process = 0;
        if (request == 255) //get request type
        {
            if (!data.isEmpty())
            {
                request = data.at(0);
                data = data.mid(1);
                process = 1;
            }
        }
        int null_index = data.indexOf('\0');
        if ((request == 1 || request == 2) && (null_index != -1 && filename.isEmpty())) //get file name
        {
            QByteArray name = data.mid(0, null_index + 1);
            filename = QString::fromStdString(name.toStdString());
            data = data.mid(null_index + 1);
            process  = 1;
        }
        if (!filename.isEmpty() && (request == 1) && (filesize == 0)) //get file size from sender
        {
            if (data.size() >= 4)
            {
                filesize = qFromBigEndian<int32_t>(data.mid(0,4));
                data = data.mid(4);
                process = 1;
            }
        }
        if (!filename.isEmpty() && (request == 1) && (filesize != 0)) //read in file from sender
        {
            if (data.size() >= filesize)
            {
                server->files[filename] = data.mid(0, filesize);
                data = data.mid(filesize);
                filename.clear();
                filesize = 0;
                request = 255;
                process = 1;
            }
        }
        if (!filename.isEmpty() && (request == 2)) //send requested file
        {
            if (!server->files.contains(filename))
                fileTimer.start(5);
            else
            {
                sendFile();
                process = 1;
            }
        }
        if (request == 3) //get settings from P1
        {
            if (data.size() >= 20)
            {
                server->settings = data.mid(0, 20);
                data = data.mid(20);
                request = 255;
                process = 1;
            }
        }
        if (request == 4) //send settings to P2-4
        {
            if (server->settings.isEmpty())
                settingTimer.start(5);
            else
            {
                sendSettings();
                process = 1;
            }
        }
        if (request == 5) //register player
        {
            if (data.size() >= 7)
            {
                uint8_t playerNum = data.mid(0, 1).at(0);
                uint8_t plugin = data.mid(1, 1).at(0);
                uint8_t raw = data.mid(2, 1).at(0);
                uint32_t reg_id = qFromBigEndian<uint32_t>(data.mid(3,4));
                data = data.mid(7);
                char response[2];
                char buffer_target = 2;
                if (!server->reg.contains(playerNum))
                {
                    if (playerNum > 0 && plugin == 2) //Only P1 can use mempak
                        plugin = 1;
                    server->reg[playerNum].first = reg_id;
                    server->reg[playerNum].second.first = plugin;
                    server->reg[playerNum].second.second = raw;
                    response[0] = 1;
                    response[1] = buffer_target;
                    emit reg_player(reg_id, playerNum, plugin);
                }
                else
                {
                    if (server->reg[playerNum].first == reg_id)
                        response[0] = 1;
                    else
                        response[0] = 0;

                    response[1] = buffer_target;
                }
                QByteArray output;
                output.append(&response[0], 2);
                socket->write(output);
                request = 255;
                process = 1;
            }
        }
        if (request == 6) //send registration
        {
            if (server->reg.size() == server->client_number)
            {
                sendReg();
                process = 1;
            }
            else
                regTimer.start(5);
        }
        if (request == 7) //disconnect notice
        {
            if (data.size() >= 4)
            {
                uint32_t reg_id = qFromBigEndian<uint32_t>(data.mid(0,4));
                emit playerDisconnect(reg_id);
                data = data.mid(4);
                request = 255;
                process = 1;
            }
        }
        if (request == 8) //get GLideN64 settings from P1
        {
            if (data.size() >= 92)
            {
                server->gliden64_settings = data.mid(0, 92);
                data = data.mid(92);
                request = 255;
                process = 1;
            }
        }
        if (request == 9) //send GLideN64 settings to P2-4
        {
            if (server->gliden64_settings.isEmpty())
                gliden64_settingTimer.start(5);
            else
            {
                sendGliden64Settings();
                process = 1;
            }
        }
    }
}

void ClientHandler::sendSettings()
{
    if (!server->settings.isEmpty())
    {
        socket->write(server->settings);
        request = 255;
        settingTimer.stop();
    }
}

void ClientHandler::sendGliden64Settings()
{
    if (!server->gliden64_settings.isEmpty())
    {
        socket->write(server->gliden64_settings);
        request = 255;
        gliden64_settingTimer.stop();
    }
}

void ClientHandler::sendReg()
{
    if (server->reg.size() == server->client_number)
    {
        QByteArray output;
        char player_data[6];
        for (int i = 0; i < 4; ++i)
        {
           if (server->reg.contains(i))
           {
               qToBigEndian(server->reg[i].first, &player_data[0]);
               player_data[4] = server->reg[i].second.first; //plugin
               player_data[5] = server->reg[i].second.second; //raw
           }
           else
           {
               memset(&player_data[0], 0, 6);
           }
           output.append(&player_data[0], 6);
        }
        socket->write(output);

        request = 255;
        regTimer.stop();
    }
}

void ClientHandler::sendFile()
{
    if (server->files.contains(filename))
    {
        socket->write(server->files[filename]);
        filename.clear();
        filesize = 0;
        request = 255;
        fileTimer.stop();
    }
}
