#include "tcpServer.h"
#include <QtEndian>

TcpServer::TcpServer(QObject *parent)
    : QTcpServer(parent)
{
}

TcpServer::~TcpServer()
{
    close();
}

void TcpServer::setPort(int port)
{
    listen(QHostAddress::Any, port);
}

void TcpServer::incomingConnection(qintptr socketDescriptor)
{
    new ClientHandler(socketDescriptor, this);
}

ClientHandler::ClientHandler(qintptr socketDescriptor, QObject *parent)
    : QObject(parent)
{
    server = (TcpServer*)parent;
    socket.setSocketDescriptor(socketDescriptor);
    connect(&socket, &QAbstractSocket::disconnected, this, &QObject::deleteLater);
    connect(&socket, SIGNAL(readyRead()), this, SLOT(readData()));
    filename.clear();
    request = 255;
    filesize = 0;
    data.clear();
    connect(&fileTimer, SIGNAL(timeout()), this, SLOT(sendFile()));
    connect(&settingTimer, SIGNAL(timeout()), this, SLOT(sendSettings()));
}

void ClientHandler::readData()
{
    data.append(socket.readAll());
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
        if (request == 3) //get settings
        {
            if (data.size() >= 20)
            {
                server->settings = data.mid(0, 20);
                data = data.mid(20);
                request = 255;
                process = 1;
            }
        }
        if (request == 4) //send settings
        {
            if (server->settings.isEmpty())
                settingTimer.start(5);
            else
            {
                sendSettings();
                process = 1;
            }
        }
    }
}

void ClientHandler::sendSettings()
{
    if (!server->settings.isEmpty())
    {
        socket.write(server->settings);
        request = 255;
        settingTimer.stop();
    }
}

void ClientHandler::sendFile()
{
    if (server->files.contains(filename))
    {
        socket.write(server->files[filename]);
        filename.clear();
        filesize = 0;
        request = 255;
        fileTimer.stop();
    }
}
