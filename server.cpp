#include "server.h"
#include <QNetworkDatagram>

void Server::initSocket()
{
    udpSocket = new QUdpSocket(this);
    udpSocket->bind(QHostAddress::Any, 45467);

    connect(udpSocket, &QUdpSocket::readyRead,
            this, &Server::readPendingDatagrams);
}

void Server::readPendingDatagrams()
{
    BUTTONS keys;
    int playerNumber;
    while (udpSocket->hasPendingDatagrams())
    {
        QNetworkDatagram datagram = udpSocket->receiveDatagram();
        QByteArray incomingData = datagram.data();
        if (incomingData.at(0) == 0) // key info
        {
            playerNumber = incomingData.at(1);
            memcpy(&keys.Value, &incomingData.data()[2], 4);
            printf("player %u keys %x\n", playerNumber, keys.Value);
        }
        else
        {
            printf("Unknown packet type %d\n", incomingData.at(0));
        }
       // int success = udpSocket->writeDatagram(hello, strlen(hello), datagram.senderAddress(), datagram.senderPort());
    }
}
