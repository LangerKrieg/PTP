#include "udpstreamer.h"

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
#include <cerrno>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <vector>

#include "Buffer.h"


UdpStreamer::UdpStreamer(const std::string& local_ip, const int local_port,
                         const std::string& remote_ip, const int remote_port)
    : sockfd_(-1), isInitialized_(false) {
    memset(&localAddr_, 0, sizeof(localAddr_));
    memset(&remoteAddr_, 0, sizeof(remoteAddr_));
    initialize(local_ip, local_port, remote_ip, remote_port);
}


UdpStreamer::~UdpStreamer() {
    if (sockfd_ >= 0) {
        close(sockfd_);
    }
}


void UdpStreamer::initialize(const std::string& local_ip, const int local_port,
                             const std::string& remote_ip, const int remote_port) {
    if (sockfd_ >= 0) {
        close(sockfd_);
    }

    sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_ < 0) {
        throw std::runtime_error(std::string("Socket creation failed: ") + strerror(errno));
    }

    int flags = fcntl(sockfd_, F_GETFL, 0);
    fcntl(sockfd_, F_SETFL, flags | O_NONBLOCK);

    localAddr_.sin_family = AF_INET;
    localAddr_.sin_port = htons(local_port);
    if (inet_pton(AF_INET, local_ip.c_str(), &localAddr_.sin_addr) <= 0) {
        close(sockfd_);
        throw std::runtime_error(std::string("Invalid local IP: ") + local_ip);
    }

    remoteAddr_.sin_family = AF_INET;
    remoteAddr_.sin_port = htons(remote_port);
    if (inet_pton(AF_INET, remote_ip.c_str(), &remoteAddr_.sin_addr) <= 0) {
        close(sockfd_);
        throw std::runtime_error(std::string("Invalid remote IP: ") + remote_ip);
    }

    if (bind(sockfd_, reinterpret_cast<struct sockaddr*>(&localAddr_), sizeof(localAddr_)) < 0) {
        close(sockfd_);
        throw std::runtime_error(std::string("Bind failed: ") + strerror(errno));
    }

    isInitialized_ = true;
    std::cout << "UDP initialized: " << local_ip << ":" << local_port
              << " -> " << remote_ip << ":" << remote_port << std::endl;
}


void UdpStreamer::Send(const char* data, const size_t size) {
    if (!isInitialized_) {
        throw std::runtime_error("UDP socket not initialized");
    }

    const ssize_t sent = sendto(sockfd_, data, size, 0,
                          reinterpret_cast<struct sockaddr*>(&remoteAddr_), sizeof(remoteAddr_));
    if (sent < 0) {
        throw std::runtime_error(std::string("Send failed: ") + strerror(errno));
    }
    if (static_cast<size_t>(sent) != size) {
        std::cerr << "Warning: Sent " << sent << " bytes instead of " << size << std::endl;
    }
}


size_t UdpStreamer::Receive(char* data, const size_t max_size) {
    if (!isInitialized_) {
        throw std::runtime_error("UDP socket not initialized");
    }

    socklen_t addr_len = sizeof(remoteAddr_);
    const ssize_t received = recvfrom(sockfd_, data, max_size, 0,
                                reinterpret_cast<struct sockaddr*>(&remoteAddr_), &addr_len);
    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        throw std::runtime_error(std::string("Receive failed: ") + strerror(errno));
    }
    return static_cast<size_t>(received);
}
