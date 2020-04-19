#include "server.h"
#include <QNetworkDatagram>

void Server::initSocket()
{
    int i;
    for (i = 0; i < 4; ++i)
    {
        playerInfo[i].port = 0;
    }

    udpSocket = new QUdpSocket(this);
    udpSocket->bind(QHostAddress::Any, 45467);

    connect(udpSocket, &QUdpSocket::readyRead,
            this, &Server::readPendingDatagrams);
}

void Server::readPendingDatagrams()
{
    BUTTONS keys;
    int i, playerNumber;
    while (udpSocket->hasPendingDatagrams())
    {
        QNetworkDatagram datagram = udpSocket->receiveDatagram();
        QByteArray incomingData = datagram.data();
        if (incomingData.at(0) == 0) // key info from client
        {
            playerNumber = incomingData.at(1);
            memcpy(&keys.Value, &incomingData.data()[2], 4);
            playerInfo[playerNumber].address = datagram.senderAddress();
            playerInfo[playerNumber].port = datagram.senderPort();
            for (i = 0; i < 4; ++i)
            {
                if (playerInfo[i].port)
                {
                    char buffer[7];
                    buffer[0] = 1; // Key info from server
                    buffer[1] = playerNumber;
                    buffer[2] = playerInfo[playerNumber].count;
                    memcpy(&buffer[3], &keys.Value, 4);
                    udpSocket->writeDatagram(&buffer[0], sizeof(buffer), playerInfo[i].address, playerInfo[i].port);
                }
            }
            inputs[playerInfo[playerNumber].count].Buttons[playerNumber] = keys;
            playerInfo[playerNumber].count++;
        }
        else
        {
            printf("Unknown packet type %d\n", incomingData.at(0));
        }
    }
}
