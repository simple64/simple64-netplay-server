#ifndef SERVERTHREAD_H
#define SERVERTHREAD_H
#include <QThread>

class ServerThread : public QThread
{
    Q_OBJECT
    void run() Q_DECL_OVERRIDE;
public:
    ServerThread(int _port, QObject *parent = 0, int p1InputDelay = -1);
signals:
    void killServer(int port);
    void desynced(int port);
    void sendClientNumber(int size);
    void writeLog(QString message, int port);
    void inputDelayChanged(int playerNum, int inputDelay);
private slots:
    void desync();
    void receiveLog(QString message, int _port);
    void player_registered(quint32 reg_id, quint8 playerNum, quint8 plugin);
    void shouldKill();
public slots:
    void setInputDelay(int playerNum, int delay);
    void getClientNumber(int _port, int size);
private:
    int port;
    int registered;
    int p1InputDelay;
};

#endif
