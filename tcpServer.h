#ifndef TCPSERVER_H
#define TCPSERVER_H
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHash>
#include <QTimer>

class TcpServer : public QTcpServer
{
    Q_OBJECT
public:
    TcpServer(QObject *parent = 0);
    ~TcpServer();
    void setPort(int port);
    QHash<QString, QByteArray> files;
    QByteArray settings;
protected:
    void incomingConnection(qintptr socketDescriptor) override;
};

class ClientHandler : public QObject
{
    Q_OBJECT
public:
    ClientHandler(qintptr socketDescriptor, QObject *parent = 0);
private slots:
    void readData();
    void sendFile();
    void sendSettings();
private:
    QTcpSocket socket;
    QByteArray data;
    QString filename;
    uint8_t request;
    int32_t filesize;
    TcpServer *server;
    QTimer fileTimer;
    QTimer settingTimer;
};

#endif
