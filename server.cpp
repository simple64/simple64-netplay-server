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

int Server::hasData(uint32_t count)
{
    for (int i = 0; i < 4; ++i)
    {
        if (!inputs[i].contains(count))
            return 0;
    }
    return 1;
}

void Server::sendInput(uint32_t count, QHostAddress address, int port, int spectator)
{
    uint32_t i, j;
    uint8_t count_number = 4;
    uint32_t sent = 0;
    char buffer[512];
    buffer[0] = 1; // Key info from server
    memcpy(&buffer[1], &count_number, 1);

    for (i = 0; i < count_number; ++i)
    {
        if (spectator == 0 || hasData(count))
        {
            memcpy(&buffer[2 + (sent * 20)], &count, 4);
            for (j = 0; j < 4; ++j)
            {
                checkIfExists(j, count);
                memcpy(&buffer[(j * 4) + (6 + (sent * 20))], &inputs[j][count], 4);
            }
            ++sent;
        }
        ++count;
    }

    if (sent > 0)
        udpSocket->writeDatagram(&buffer[0], 2 + (20 * sent), address, port);
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
            sendInput(count, datagram.senderAddress(), datagram.senderPort(), 0);
        }
        else if (incomingData.at(0) == 2) // request for player input data
        {
            sendInput(count, datagram.senderAddress(), datagram.senderPort(), playerNumber < 4 ? 0 : 1);
        }
        else
        {
            printf("Unknown packet type %d\n", incomingData.at(0));
        }
    }
}
