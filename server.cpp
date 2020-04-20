#include "server.h"
#include <QNetworkDatagram>

void Server::initSocket()
{
    int i;
    for (i = 0; i < 4; ++i)
    {
        playerInfo[i].port = 0;
        inputs[i].setMaxCost(50);
    }

    udpSocket = new QUdpSocket(this);
    udpSocket->bind(QHostAddress::Any, 45467);

    connect(udpSocket, &QUdpSocket::readyRead,
            this, &Server::readPendingDatagrams);
}

void Server::checkIfExists(int playerNumber, uint8_t count)
{
    if (!inputs[playerNumber].contains(count)) //They are asking for a value we don't have
    {
        InputState* state = new InputState;
        if (!buttons[playerNumber].isEmpty())
            state->Buttons = buttons[playerNumber].takeFirst();
        else if (inputs[playerNumber].contains(count-1))
            state->Buttons = inputs[playerNumber].object(count-1)->Buttons;
        else
            state->Buttons.Value = 0;
        inputs[playerNumber].insert(count, state, 1);
    }
}

void Server::sendInput(int playerNumber, uint8_t count)
{
    char buffer[18];
    buffer[0] = 1; // Key info from server
    buffer[1] = count;

    for (int i = 0; i < 4; ++i)
        memcpy(&buffer[(i * 4) + 2], &inputs[i].object(count)->Buttons.Value, 4);

    udpSocket->writeDatagram(&buffer[0], sizeof(buffer), playerInfo[playerNumber].address, playerInfo[playerNumber].port);
}

void Server::readPendingDatagrams()
{
    int i, playerNumber;
    uint8_t count;
    while (udpSocket->hasPendingDatagrams())
    {
        QNetworkDatagram datagram = udpSocket->receiveDatagram();
        QByteArray incomingData = datagram.data();
        playerNumber = incomingData.at(1);
        playerInfo[playerNumber].address = datagram.senderAddress();
        playerInfo[playerNumber].port = datagram.senderPort();
        if (incomingData.at(0) == 0) // key info from client
        {
            BUTTONS keys;
            memcpy(&keys.Value, &incomingData.data()[2], 4);
            buttons[playerNumber].append(keys);
        }
        else if (incomingData.at(0) == 2) // request for player input data
        {
            count = incomingData.at(2);
            for (i = 0; i < 4; ++i)
                checkIfExists(i, count);
            sendInput(playerNumber, count);
        }
        else
        {
            printf("Unknown packet type %d\n", incomingData.at(0));
        }
    }
}
