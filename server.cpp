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
        if (inputs[playerNumber].contains(count-1))
            state->Buttons = inputs[playerNumber].object(count-1)->Buttons;
        else
            state->Buttons.Value = 0;
        inputs[playerNumber].insert(count, state, 1);
        playerInfo[playerNumber].count++;
    }
}

void Server::sendInput(int playerNumber, uint8_t count)
{
    int i;
    char buffer[7];
    buffer[0] = 1; // Key info from server
    buffer[1] = playerNumber;
    buffer[2] = count;

    memcpy(&buffer[3], &inputs[playerNumber].object(count)->Buttons.Value, 4);

    for (i = 0; i < 4; ++i)
    {
        if (playerInfo[i].port)
            udpSocket->writeDatagram(&buffer[0], sizeof(buffer), playerInfo[i].address, playerInfo[i].port);
    }
}

void Server::readPendingDatagrams()
{
    int i, playerNumber;
    while (udpSocket->hasPendingDatagrams())
    {
        QNetworkDatagram datagram = udpSocket->receiveDatagram();
        QByteArray incomingData = datagram.data();
        if (incomingData.at(0) == 0) // key info from client
        {
            InputState* state = new InputState;
            playerNumber = incomingData.at(1);
            memcpy(&state->Buttons.Value, &incomingData.data()[2], 4);
            playerInfo[playerNumber].address = datagram.senderAddress();
            playerInfo[playerNumber].port = datagram.senderPort();
            inputs[playerNumber].insert(playerInfo[playerNumber].count, state, 1);
            sendInput(playerNumber, playerInfo[playerNumber].count);
            playerInfo[playerNumber].count++;
        }
        else if (incomingData.at(0) == 2) // request for player input data
        {
            uint8_t count = incomingData.at(2);
            checkIfExists(incomingData.at(1), count);
            for (i = 0; i < 4; ++i)
            {
                if (inputs[i].contains(count))
                    sendInput(i, count);
            }
        }
        else
        {
            printf("Unknown packet type %d\n", incomingData.at(0));
        }
    }
}
