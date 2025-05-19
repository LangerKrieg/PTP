#include "udpstreamer.h"
#include <iostream>

UdpStreamer::UdpStreamer()
    : sockfd_(-1), isInitialized_(false) {
    memset(&localAddr_, 0, sizeof(localAddr_));
    memset(&remoteAddr_, 0, sizeof(remoteAddr_));
}

UdpStreamer::~UdpStreamer() {
    if (sockfd_ >= 0) {
        close(sockfd_);
    }
}

bool UdpStreamer::initialize(const std::string& localIp, int localPort,
                           const std::string& remoteIp, int remotePort) {
    if (sockfd_ >= 0) {
        close(sockfd_);
    }

    sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_ < 0) {
        std::cerr << "Socket creation failed: " << strerror(errno) << std::endl;
        return false;
    }

    localAddr_.sin_family = AF_INET;
    localAddr_.sin_port = htons(localPort);
    if (inet_pton(AF_INET, localIp.c_str(), &localAddr_.sin_addr) <= 0) {
        std::cerr << "Invalid local IP: " << localIp << std::endl;
        close(sockfd_);
        return false;
    }

    remoteAddr_.sin_family = AF_INET;
    remoteAddr_.sin_port = htons(remotePort);
    if (inet_pton(AF_INET, remoteIp.c_str(), &remoteAddr_.sin_addr) <= 0) {
        std::cerr << "Invalid remote IP: " << remoteIp << std::endl;
        close(sockfd_);
        return false;
    }

    if (bind(sockfd_, (struct sockaddr*)&localAddr_, sizeof(localAddr_))) {
        std::cerr << "Bind failed: " << strerror(errno) << std::endl;
        close(sockfd_);
        return false;
    }

    isInitialized_ = true;
    std::cout << "UDP initialized: " << localIp << ":" << localPort
              << " -> " << remoteIp << ":" << remotePort << std::endl;
    return true;
}

bool UdpStreamer::sendBuffer(const Buffer& buffer) {
    if (!isInitialized_) return false;

    const auto& rawBuffer = buffer.getRawBuffer();
    ssize_t sent = sendto(sockfd_, rawBuffer.data(), rawBuffer.size(), 0,
                         (struct sockaddr*)&remoteAddr_, sizeof(remoteAddr_));
    if (sent < 0) {
        std::cerr << "Send failed: " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

bool UdpStreamer::receiveToBuffer(Buffer& buffer) {
    if (!isInitialized_) return false;

    auto& rawBuffer = const_cast<std::vector<char>&>(buffer.getRawBuffer());
    socklen_t addrLen = sizeof(remoteAddr_);

    ssize_t received = recvfrom(sockfd_, rawBuffer.data(), rawBuffer.size(), 0,
                              (struct sockaddr*)&remoteAddr_, &addrLen);
    if (received < 0) {
        std::cerr << "Receive failed: " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}
