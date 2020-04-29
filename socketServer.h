#ifndef SOCKETSERVER_H
#define SOCKETSERVER_H

#include <QWebSocketServer>
#include <QWebSocket>

class SocketServer : public QObject
{
    Q_OBJECT

public:
    explicit SocketServer(QObject *parent = 0);
    ~SocketServer();

signals:
    void closed();

private slots:
    void onNewConnection();
    void processTextMessage(QString message);
    void processBinaryMessage(QByteArray message);
    void socketDisconnected();
private:
    QWebSocketServer *webSocketServer;
    QList<QWebSocket *> clients;
};

#endif
