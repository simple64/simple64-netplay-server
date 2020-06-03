#include "udpServer.h"
#include "xxh3.h"
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
        buffer_size[i] = 3;
        buffer_health[i] = -1;
    }
    port = _port;
    status = 0;
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
        if (!buttons[playerNumber].isEmpty())
            inputs[playerNumber].insert(count, buttons[playerNumber].takeFirst());
        else if (inputs[playerNumber].contains(count-1))
            inputs[playerNumber].insert(count, inputs[playerNumber].value(count-1));
        else
            inputs[playerNumber].insert(count, qMakePair(0, 0/*Controller not present*/));

        if (inputs[playerNumber].size() == 5000)
            inputs[playerNumber].remove(count - 4999);
    }
}

void UdpServer::sendInput(uint32_t count, QHostAddress address, int port, uint8_t playerNum, uint8_t spectator)
{
    char buffer[512];
    uint32_t count_lag = lead_count[playerNum] - count;
    buffer[0] = 1; // Key info from server
    buffer[1] = playerNum;
    buffer[2] = status;
    buffer[3] = (uint8_t) count_lag;
    uint32_t curr = 5;
    uint32_t start = count;
    uint32_t end = start + buffer_size[playerNum];
    while ( (curr < 500) && ( (spectator == 0 && count_lag == 0 && (count - end) > (UINT32_MAX / 2)) || inputs[playerNum].contains(count) ) )
    {
        qToBigEndian(count, &buffer[curr]);
        curr += 4;
        checkIfExists(playerNum, count);
        qToBigEndian(inputs[playerNum].value(count).first, &buffer[curr]);
        curr += 4;
        buffer[curr] = inputs[playerNum].value(count).second;
        curr += 1;
        ++count;
    }

    buffer[4] = count - start; //number of counts in packet

    if (curr > 5)
        udpSocket->writeDatagram(&buffer[0], curr, address, port);
}

void UdpServer::register_player(uint32_t reg_id, uint8_t playerNum, uint8_t plugin)
{
    player_keepalive[reg_id].first = 0;
    player_keepalive[reg_id].second = playerNum;
    inputs[playerNum].insert(0, qMakePair(0, plugin));
}

void UdpServer::readPendingDatagrams()
{
    uint32_t keys, count, vi_count;
    uint8_t playerNum, spectator;
    while (udpSocket->hasPendingDatagrams())
    {
        QNetworkDatagram datagram = udpSocket->receiveDatagram();
        QByteArray incomingData = datagram.data();
        playerNum = incomingData.at(1);
        switch (incomingData.at(0))
        {
            case 0: // key info from client
                count = qFromBigEndian<uint32_t>(&incomingData.data()[2]);
                keys = qFromBigEndian<uint32_t>(&incomingData.data()[6]);
                if (buttons[playerNum].size() == 0)
                    buttons[playerNum].append(qMakePair(keys, incomingData.at(10)));
                break;
            case 2: // request for player input data
                player_keepalive[qFromBigEndian<uint32_t>(&incomingData.data()[2])].first = 0;
                if (timerId == 0)
                    timerId = startTimer(500);

                count = qFromBigEndian<uint32_t>(&incomingData.data()[6]);
                spectator = incomingData.at(10);
                if (((count - lead_count[playerNum]) < (UINT32_MAX / 2)) && spectator == 0)
                {
                    buffer_health[playerNum] = incomingData.at(11);
                    lead_count[playerNum] = count;
                }
                sendInput(count, datagram.senderAddress(), datagram.senderPort(), playerNum, spectator);
                break;
            case 4: // cp0 info from client
                if ((status & 1) == 0)
                {
                    vi_count = qFromBigEndian<uint32_t>(&incomingData.data()[1]);
                    if (!sync_hash.contains(vi_count))
                    {
                        sync_hash.insert(vi_count, XXH3_64bits(&incomingData.data()[5], 128));
                        if (sync_hash.size() == 5000)
                            sync_hash.remove(vi_count - 4999);
                    }
                    else if (sync_hash.value(vi_count) != XXH3_64bits(&incomingData.data()[5], 128))
                    {
                        status |= 1;
                        emit desynced();
                    }
                }
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
            if (buffer_health[i] > 2 && buffer_size[i] > 0)
                --buffer_size[i];
            else if (buffer_health[i] < 2)
                ++buffer_size[i];
        }
    }

    uint32_t should_delete = 0;
    QHash<uint32_t, QPair<uint8_t, uint8_t>>::iterator iter;
    for (iter = player_keepalive.begin(); iter != player_keepalive.end(); ++iter)
    {
        ++iter.value().first;
        if (iter.value().first > 40)
            should_delete = iter.key();
    }

    if (should_delete)
        disconnect_player(should_delete);
}

void UdpServer::disconnect_player(uint32_t reg_id)
{
    status |= (0x1 << (player_keepalive[reg_id].second + 1));
    player_keepalive.remove(reg_id);

    if (player_keepalive.isEmpty())
        emit killMe(port);
}
