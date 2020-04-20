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
            inputs[playerNumber][count] = 0;
    }
}

void Server::sendInput(uint8_t playerNumber, uint32_t count)
{
    char buffer[21];
    buffer[0] = 1; // Key info from server
    memcpy(&buffer[1], &count, 4);

    for (int i = 0; i < 4; ++i)
    {
        checkIfExists(i, count);
        memcpy(&buffer[(i * 4) + 5], &inputs[i][count], 4);
    }

    udpSocket->writeDatagram(&buffer[0], sizeof(buffer), playerInfo[playerNumber].address, playerInfo[playerNumber].port);
}

void Server::readPendingDatagrams()
{
    uint8_t playerNumber;
    uint32_t keys, count;
    while (udpSocket->hasPendingDatagrams())
    {
        QNetworkDatagram datagram = udpSocket->receiveDatagram();
        QByteArray incomingData = datagram.data();
        playerNumber = incomingData.at(1);
        memcpy(&count, &incomingData.data()[2], 4);
        playerInfo[playerNumber].address = datagram.senderAddress();
        playerInfo[playerNumber].port = datagram.senderPort();
        if (incomingData.at(0) == 0) // key info from client
        {
            memcpy(&keys, &incomingData.data()[6], 4);
            buttons[playerNumber].append(keys);
            sendInput(playerNumber, count + 2);
            sendInput(playerNumber, count + 3);
        }
        else if (incomingData.at(0) == 2) // request for player input data
        {
            sendInput(playerNumber, count);
        }
        else
        {
            printf("Unknown packet type %d\n", incomingData.at(0));
        }
    }
}
