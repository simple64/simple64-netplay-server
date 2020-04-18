#ifndef SERVER_H
#define SERVER_H
#include <QObject>
#include <QUdpSocket>
#include <QHash>
#include "m64p_plugin.h"

struct frame_state {
    BUTTONS Player1_Buttons;
    BUTTONS Player2_Buttons;
    BUTTONS Player3_Buttons;
    BUTTONS Player4_Buttons;
};

class Server : public QObject
{
    Q_OBJECT
public:
    void initSocket();
    void readPendingDatagrams();
private:
    QUdpSocket* udpSocket;
    QHash<uint8_t, frame_state> frames;
};

#endif
