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
    QHash<uint8_t, QPair<uint32_t, QPair<uint8_t, uint8_t>>> reg; //player number, <reg_id, <plugin, raw>>
    int client_number;
private slots:
    void reg_player(uint32_t reg_id, uint8_t playerNum, uint8_t plugin);
    void playerDisconnect(uint32_t reg_id);
    void onNewConnection();
public slots:
    void getClientNumber(int size);
signals:
    void register_player(uint32_t reg_id, uint8_t playerNum, uint8_t plugin);
    void disconnect_player(uint32_t reg_id);
};

class ClientHandler : public QObject
{
    Q_OBJECT
public:
    ClientHandler(QTcpSocket *_socket, QObject *parent = 0);
private slots:
    void readData();
    void sendFile();
    void sendSettings();
    void sendReg();
signals:
    void reg_player(uint32_t reg_id, uint8_t playerNum, uint8_t plugin);
    void playerDisconnect(uint32_t reg_id);
private:
    QTcpSocket *socket;
    QByteArray data;
    QString filename;
    uint8_t request;
    int32_t filesize;
    TcpServer *server;
    QTimer fileTimer;
    QTimer settingTimer;
    QTimer regTimer;
};

#endif
