#include "udpServer.h"
#include <QNetworkDatagram>
#include <QtEndian>

UdpServer::UdpServer(int _port)
{
    udpSocket = new QUdpSocket(this);
    udpSocket->bind(QHostAddress::Any, _port);

    connect(udpSocket, &QUdpSocket::readyRead,
            this, &UdpServer::readPendingDatagrams);

    timerId = 0;
    for (int i = 0; i < 4; ++i)
    {
        lead_count[i] = 0;
        buffer_size[i] = 4;
        buffer_health[i] = -1;
        inputs[i].setMaxCost(5000);
    }
    port = _port;
    keepalive = 0;
}

UdpServer::~UdpServer()
{
    udpSocket->close();
}

int UdpServer::getPort()
{
    return port;
}

void UdpServer::checkIfExists(uint8_t playerNumber, uint32_t count)
{
    if (!inputs[playerNumber].contains(count)) //They are asking for a value we don't have
    {
        InputState* state = new InputState;
        if (!buttons[playerNumber].isEmpty())
            state->data = buttons[playerNumber].takeFirst();
        else if (inputs[playerNumber].contains(count-1))
            state->data = inputs[playerNumber].object(count-1)->data;
        else
            state->data = qMakePair(0, 0/*Controller not present*/);
        inputs[playerNumber].insert(count, state, 1);
    }
}

void UdpServer::sendInput(uint32_t count, QHostAddress address, int port, uint8_t playerNum, uint8_t spectator)
{
    char buffer[512];
    uint32_t count_lag = lead_count[playerNum] - count;
    buffer[0] = 1; // Key info from server
    buffer[1] = playerNum;
    uint32_t curr = 3;
    uint32_t start = count;
    uint32_t end = start + buffer_size[playerNum];
    while ( (curr < 500) && ( (spectator == 0 && count_lag == 0 && (count < end)) || (inputs[playerNum].contains(count)) ) )
    {
        qToBigEndian(count, &buffer[curr]);
        curr += 4;
        checkIfExists(playerNum, count);
        qToBigEndian(inputs[playerNum].object(count)->data.first, &buffer[curr]);
        curr += 4;
        buffer[curr] = inputs[playerNum].object(count)->data.second;
        curr += 1;
        ++count;
    }

    buffer[2] = count - start; //number of counts in packet

    if (curr > 3)
        udpSocket->writeDatagram(&buffer[0], curr, address, port);
}

void UdpServer::sendRegResponse(uint8_t playerNumber, uint32_t reg_id, QHostAddress address, int port)
{
    char buffer[3];
    buffer[0] = 3;
    buffer[1] = playerNumber;

    if (!reg.contains(playerNumber))
    {
        reg[playerNumber] = reg_id;
        InputState* state = new InputState;
        state->data = qMakePair(0, 1/*PLUGIN_NONE*/);
        inputs[playerNumber].insert(0, state, 1);
        buffer[2] = 1;
    }
    else
    {
        if (reg[playerNumber] == reg_id)
            buffer[2] = 1;
        else
            buffer[2] = 0;
    }

    udpSocket->writeDatagram(&buffer[0], 3, address, port);
}

void UdpServer::readPendingDatagrams()
{
    uint32_t keys, count, reg_id;
    uint8_t playerNum, spectator;
    while (udpSocket->hasPendingDatagrams())
    {
        keepalive = 0;
        QNetworkDatagram datagram = udpSocket->receiveDatagram();
        QByteArray incomingData = datagram.data();
        playerNum = incomingData.at(1);
        switch (incomingData.at(0))
        {
            case 0: // key info from client
                count = qFromBigEndian<uint32_t>(&incomingData.data()[2]);
                keys = qFromBigEndian<uint32_t>(&incomingData.data()[6]);
                if (buttons[playerNum].size() < 2)
                    buttons[playerNum].append(qMakePair(keys, incomingData.at(10)));
                break;
            case 2: // request for player input data
                if (timerId == 0)
                    timerId = startTimer(500);

                count = qFromBigEndian<uint32_t>(&incomingData.data()[2]);
                spectator = incomingData.at(6);
                if (count >= lead_count[playerNum] && spectator == 0)
                {
                    buffer_health[playerNum] = incomingData.data()[7];
                    lead_count[playerNum] = count;
                }
                sendInput(count, datagram.senderAddress(), datagram.senderPort(), playerNum, spectator);
                break;
            case 4: // registration request
                reg_id = qFromBigEndian<uint32_t>(&incomingData.data()[2]);
                if (playerNum < 4)
                    sendRegResponse(playerNum, reg_id, datagram.senderAddress(), datagram.senderPort());
                break;
            default:
                printf("Unknown packet type %d\n", incomingData.at(0));
                break;
        }
    }
}

void UdpServer::timerEvent(QTimerEvent *)
{
    for (int i = 0; i < 4; ++i)
    {
        if (buffer_health[i] != -1)
        {
            if (buffer_health[i] > 3 && buffer_size[i] > 3)
                --buffer_size[i];
            else if (buffer_health[i] < 3)
                ++buffer_size[i];
        }
    }
   ++keepalive;
   if (keepalive > 20)
       emit killMe(port);
}
