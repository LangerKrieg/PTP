#ifndef UDPSTREAMER_H
#define UDPSTREAMER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <vector>
#include <string>
#include "Buffer.h"

class UdpStreamer {
public:
    UdpStreamer();
    ~UdpStreamer();

    bool initialize(const std::string& localIp, int localPort,
                   const std::string& remoteIp, int remotePort);
    bool sendBuffer(const Buffer& buffer);
    bool receiveToBuffer(Buffer& buffer);

private:
    int sockfd_;
    struct sockaddr_in localAddr_;
    struct sockaddr_in remoteAddr_;
    bool isInitialized_;
};

#endif // UDPSTREAMER_H
