#include "server.h"
#include <QNetworkDatagram>

void Server::initSocket()
{
    udpSocket = new QUdpSocket(this);
    udpSocket->bind(QHostAddress::Any, 45467);

    connect(udpSocket, &QUdpSocket::readyRead,
            this, &Server::readPendingDatagrams);
}

void Server::checkIfExists(uint8_t playerNumber, uint32_t count)
{
    if (!inputs[playerNumber].contains(count)) //They are asking for a value we don't have
    {
        if (!buttons[playerNumber].isEmpty())
            inputs[playerNumber][count] = buttons[playerNumber].takeFirst();
        else if (inputs[playerNumber].contains(count-1))
            inputs[playerNumber][count] = inputs[playerNumber][count-1];
        else
            inputs[playerNumber][count] = qMakePair(0, 1/*PLUGIN_NONE*/);
    }
}

void Server::sendInput(uint32_t count, QHostAddress address, int port, uint8_t playerNum, uint8_t spectator)
{
    char buffer[512];
    buffer[0] = 1; // Key info from server
    buffer[1] = playerNum;
    buffer[2] = 4; //count number
    uint32_t curr = 3;
    for (uint8_t i = 0; i < buffer[2]; ++i)
    {
        if (spectator == 0 || inputs[playerNum].contains(count))
        {
            memcpy(&buffer[curr], &count, 4);
            curr += 4;
            checkIfExists(playerNum, count);
            memcpy(&buffer[curr], &inputs[playerNum][count].first, 4);
            curr += 4;
            memcpy(&buffer[curr], &inputs[playerNum][count].second, 1);
            curr += 1;
        }
        ++count;
    }

    if (curr > 3)
        udpSocket->writeDatagram(&buffer[0], curr, address, port);
}

void Server::readPendingDatagrams()
{
    uint32_t keys, count;
    while (udpSocket->hasPendingDatagrams())
    {
        QNetworkDatagram datagram = udpSocket->receiveDatagram();
        QByteArray incomingData = datagram.data();
        memcpy(&count, &incomingData.data()[2], 4);

        if (incomingData.at(0) == 0) // key info from client
        {
            memcpy(&keys, &incomingData.data()[6], 4);
            buttons[(uint8_t)incomingData.at(1)].append(qMakePair(keys, incomingData.at(10)));
        }
        else if (incomingData.at(0) == 2) // request for player input data
        {
            sendInput(count, datagram.senderAddress(), datagram.senderPort(), incomingData.at(1), incomingData.at(6));
        }
        else
        {
            printf("Unknown packet type %d\n", incomingData.at(0));
        }
    }
}
