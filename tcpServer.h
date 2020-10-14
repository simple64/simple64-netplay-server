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
    TcpServer(char _buffer_target, QObject *parent = 0);
    void setPort(int port);
    QHash<QString, QByteArray> files;
    QByteArray settings;
    QByteArray gliden64_settings;
    QHash<quint8, QPair<quint32, QPair<quint8, quint8>>> reg; //player number, <reg_id, <plugin, raw>>
    int client_number;
private slots:
    void reg_player(quint32 reg_id, quint8 playerNum, quint8 plugin);
    void playerDisconnect(quint32 reg_id);
    void onNewConnection();
public slots:
    void getClientNumber(int size);
signals:
    void register_player(quint32 reg_id, quint8 playerNum, quint8 plugin);
    void disconnect_player(quint32 reg_id);
private:
    char buffer_target;
};

class ClientHandler : public QObject
{
    Q_OBJECT
public:
    ClientHandler(char _buffer_target, QTcpSocket *_socket, QObject *parent = 0);
private slots:
    void readData();
    void sendFile();
    void sendSettings();
    void sendGliden64Settings();
    void sendReg();
signals:
    void reg_player(quint32 reg_id, quint8 playerNum, quint8 plugin);
    void playerDisconnect(quint32 reg_id);
private:
    QTcpSocket *socket;
    QByteArray data;
    QString filename;
    quint8 request;
    qint32 filesize;
    TcpServer *server;
    QTimer fileTimer;
    QTimer settingTimer;
    QTimer gliden64_settingTimer;
    QTimer regTimer;
    char buffer_target;
};

#endif
