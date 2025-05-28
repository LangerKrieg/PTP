//
// Created by kuki on 5/19/25.
//

#ifndef UDPSTREAMER_H
#define UDPSTREAMER_H


#include <string>
#include <netinet/in.h>

class UdpStreamer {
public:
    UdpStreamer(const std::string& local_ip, int local_port,
                const std::string& remote_ip, int remote_port);
    ~UdpStreamer();

    void Send(const char* data, size_t size);
    size_t Receive(char* data, size_t max_size);

private:
    void initialize(const std::string& local_ip, int local_port,
                    const std::string& remote_ip, int remote_port);

    int sockfd_;
    bool isInitialized_;

    struct sockaddr_in localAddr_;
    struct sockaddr_in remoteAddr_;
};




#endif //UDPSTREAMER_H
