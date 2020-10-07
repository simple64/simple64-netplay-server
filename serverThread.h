#ifndef SERVERTHREAD_H
#define SERVERTHREAD_H
#include <QThread>

class ServerThread : public QThread
{
    Q_OBJECT
    void run() Q_DECL_OVERRIDE;
public:
    ServerThread(int _port, QObject *parent = 0);
signals:
    void killServer(int port);
    void desynced(int port);
    void sendClientNumber(int size);
    void writeLog(QString message, int port);
private slots:
    void desync();
    void receiveLog(QString message, int port);
public slots:
    void getClientNumber(int _port, int size);
private:
    int port;
};

#endif
