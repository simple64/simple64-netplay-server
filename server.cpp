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

void Server::sendInput(uint32_t count, QHostAddress address, int port)
{
    uint32_t i, j;
    uint8_t count_number = 3;
    char buffer[512];
    buffer[0] = 1; // Key info from server
    memcpy(&buffer[1], &count_number, 1);

    for (i = 0; i < count_number; ++i)
    {
        memcpy(&buffer[2 + (i * 20)], &count, 4);
        for (j = 0; j < 4; ++j)
        {
            checkIfExists(j, count);
            memcpy(&buffer[(j * 4) + (6 + (i * 20))], &inputs[j][count], 4);
        }
        ++count;
    }

    udpSocket->writeDatagram(&buffer[0], 2 + (20 * count_number), address, port);
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

        if (incomingData.at(0) == 0) // key info from client
        {
            memcpy(&keys, &incomingData.data()[6], 4);
            buttons[playerNumber].append(keys);
            sendInput(count, datagram.senderAddress(), datagram.senderPort());
        }
        else if (incomingData.at(0) == 2) // request for player input data
        {
            sendInput(count, datagram.senderAddress(), datagram.senderPort());
        }
        else
        {
            printf("Unknown packet type %d\n", incomingData.at(0));
        }
    }
}
