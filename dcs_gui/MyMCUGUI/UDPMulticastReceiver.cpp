#include "UDPMulticastReceiver.h"
#include <Winsock2.h> // before Windows.h, else Winsock 1 conflict
#include <Ws2tcpip.h> // needed for ip_mreq definition for multicast
#include <Windows.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

namespace 
{
	const char* MULTICAST_IP = "239.255.50.10";
	const int MULTICAST_PORT = 5010;
    const int MSGBUFSIZE = 128;
}

class UDPMulticastReceiverImpl
{
public:
    struct sockaddr_in addr;
    int fd;
};

UDPMulticastReceiver::UDPMulticastReceiver()
{
    impl = new UDPMulticastReceiverImpl();
}

UDPMulticastReceiver::~UDPMulticastReceiver()
{
    delete impl;
}

int UDPMulticastReceiver::Init()
{
    // Create what looks like an ordinary UDP socket
    impl->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (impl->fd < 0)
    {
        perror("socket");
        return 1;
    }

    // Allow multiple sockets to use the same PORT number
    u_int yes = 1;
    if (setsockopt(impl->fd, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes)) < 0)
    {
        perror("Reusing ADDR failed");
        return 1;
    }

    // Set up destination address
    memset(&impl->addr, 0, sizeof(impl->addr));
    impl->addr.sin_family = AF_INET;
    impl->addr.sin_addr.s_addr = htonl(INADDR_ANY); // differs from sender
    impl->addr.sin_port = htons(MULTICAST_PORT);

    // Bind to receive address
    if (bind(impl->fd, (struct sockaddr*)&impl->addr, sizeof(impl->addr)) < 0)
    {
        perror("bind");
        return 1;
    }

    // Use setsockopt() to request that the kernel join a multicast group
    struct ip_mreq mreq;
    inet_pton(AF_INET, MULTICAST_IP, &mreq.imr_multiaddr.s_addr);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(impl->fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq)) < 0)
    {
        perror("setsockopt");
        return 1;
    }
}

int UDPMulticastReceiver::Recv(std::function<void(std::vector<char>&)> dataFunc)
{
    std::vector<char> data;
    data.resize(MSGBUFSIZE);

    char msgbuf[MSGBUFSIZE];
    int addrlen = sizeof(impl->addr);
    int nbytes = recvfrom(
        impl->fd,
        data.data(),
        data.size(),
        0,
        (struct sockaddr*)&impl->addr,
        &addrlen
    );

    if (nbytes < 0) {
        perror("recvfrom");
        return 1;
    }

    data[nbytes] = '\0';

    dataFunc(data);

    return 0;
}
