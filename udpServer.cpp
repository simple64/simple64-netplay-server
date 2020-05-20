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
        buffer_size[i] = 4;
        buffer_health[i] = -1;
        inputs[i].setMaxCost(5000);
    }
    sync_hash.setMaxCost(5000);
    port = _port;
    status = 0;
    desync = 0;
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
    buffer[2] = status;
    uint32_t curr = 4;
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

    buffer[3] = count - start; //number of counts in packet

    if (curr > 4)
        udpSocket->writeDatagram(&buffer[0], curr, address, port);
}

void UdpServer::register_player(uint32_t reg_id, uint8_t playerNum, uint8_t plugin)
{
    player_keepalive[reg_id].first = 0;
    player_keepalive[reg_id].second = playerNum;
    InputState* state = new InputState;
    state->data = qMakePair(0, plugin);
    inputs[playerNum].insert(0, state, 1);
}

void UdpServer::readPendingDatagrams()
{
    uint32_t keys, count, vi_count;
    uint8_t playerNum, spectator;
    QSet<uint64_t> set;
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
                if (buttons[playerNum].size() < 2)
                    buttons[playerNum].append(qMakePair(keys, incomingData.at(10)));
                break;
            case 2: // request for player input data
                player_keepalive[qFromBigEndian<uint32_t>(&incomingData.data()[2])].first = 0;
                if (timerId == 0)
                    timerId = startTimer(500);

                count = qFromBigEndian<uint32_t>(&incomingData.data()[6]);
                spectator = incomingData.at(10);
                if (count >= lead_count[playerNum] && spectator == 0)
                {
                    buffer_health[playerNum] = incomingData.data()[11];
                    lead_count[playerNum] = count;
                }
                sendInput(count, datagram.senderAddress(), datagram.senderPort(), playerNum, spectator);
                break;
            case 4: // cp0 info from client
                if (desync == 0)
                {
                    vi_count = qFromBigEndian<uint32_t>(&incomingData.data()[1]);
                    if (!sync_hash.contains(vi_count))
                    {
                        HashState* state = new HashState;
                        sync_hash.insert(vi_count, state, 1);
                    }
                    sync_hash[vi_count]->data.append(XXH3_64bits(&incomingData.data()[5], 128));
                    set = QSet<uint64_t>::fromList(sync_hash[vi_count]->data);
                    if (set.size() > 1)
                    {
                        desync = 1;
                        status |= 1;
                        emit desynced(port);
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
            if (buffer_health[i] > 3 && buffer_size[i] > 3)
                --buffer_size[i];
            else if (buffer_health[i] < 3)
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
