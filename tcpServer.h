#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <QThread>
#include <QTcpSocket>
#include <QTcpServer>

class TcpThread : public QThread
{
    Q_OBJECT

public:
    TcpThread(int socketDescriptor, QObject *parent);

    void run() override;

signals:
    void error(QTcpSocket::SocketError socketError);

private slots:
    void readData();

private:
    QTcpSocket tcpSocket;
    int socketDescriptor;
};

class TcpServer : public QTcpServer
{
    Q_OBJECT

public:
    TcpServer(QObject *parent = 0);

protected:
    void incomingConnection(qintptr socketDescriptor) override;
};

#endif
