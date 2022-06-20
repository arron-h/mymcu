#include "UDPSender.h"
#include <Winsock2.h> // before Windows.h, else Winsock 1 conflict
#include <Ws2tcpip.h> // needed for ip_mreq definition for multicast
#include <Windows.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

namespace
{
	const int UDP_PORT = 7778;
    const char* LOOPBACK = "127.0.0.1";
}

class UDPSenderImpl
{
public:
    int fd;
    struct sockaddr_in addr;
};

UDPSender::UDPSender()
{
    impl = new UDPSenderImpl();
}

UDPSender::~UDPSender()
{
    delete impl;
}

int UDPSender::Init()
{
    // Create what looks like an ordinary UDP socket
    impl->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (impl->fd < 0) {
        perror("socket");
        return 1;
    }

    // Set up destination address
    memset(&impl->addr, 0, sizeof(impl->addr));
    impl->addr.sin_family = AF_INET;
    inet_pton(AF_INET, LOOPBACK, &impl->addr.sin_addr.s_addr);
    impl->addr.sin_port = htons(UDP_PORT);

    return 0;
}

int UDPSender::Send(std::vector<unsigned char>& data)
{
    std::vector<char> udpData(data.size());
    udpData.assign(data.begin(), data.end());

    char ch = 0;
    int nbytes = sendto(
        impl->fd,
        udpData.data(),
        udpData.size(),
        0,
        (struct sockaddr*)&impl->addr,
        sizeof(impl->addr)
    );

    if (nbytes < 0)
    {
        perror("sendto");
        return 1;
    }

    return 0;
}
