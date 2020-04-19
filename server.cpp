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
    while (udpSocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = udpSocket->receiveDatagram();
	printf("received at server\n");
        char hello[] = "Hello from server";
        int success = udpSocket->writeDatagram(hello, strlen(hello), datagram.senderAddress(), datagram.senderPort());
        printf("%d\n", success);
//        processTheDatagram(datagram);
    }
}
