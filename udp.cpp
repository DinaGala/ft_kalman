#include "udp.hpp"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <vector>
#include <arpa/inet.h>

UDPClient::UDPClient(const std::string &host, const std::string &port)
    : sockfd_(-1), host_(host), port_(port), remote_addr_len_(0) {
    struct addrinfo hints;
    struct addrinfo *res = nullptr;

    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // IPv4 or IPv6
    hints.ai_socktype = SOCK_DGRAM;

    int rv = getaddrinfo(host.c_str(), port.c_str(), &hints, &res);
    if (rv != 0) {
        std::cerr << "getaddrinfo: " << gai_strerror(rv) << '\n';
        return;
    }

    // Create socket using the first resolved addr
    for (struct addrinfo *p = res; p != nullptr; p = p->ai_next) {
        sockfd_ = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd_ == -1)
            continue;

        // bind to an ephemeral local port so replies reliably arrive on this socket
        if (p->ai_family == AF_INET) {
            struct sockaddr_in local4;
            std::memset(&local4, 0, sizeof(local4));
            local4.sin_family = AF_INET;
            local4.sin_addr.s_addr = INADDR_ANY;
            local4.sin_port = 0; // ephemeral
            if (bind(sockfd_, reinterpret_cast<struct sockaddr *>(&local4), sizeof(local4)) == -1) {
                // non-fatal: continue to next candidate
                close(sockfd_);
                sockfd_ = -1;
                continue;
            }
        } else if (p->ai_family == AF_INET6) {
            struct sockaddr_in6 local6;
            std::memset(&local6, 0, sizeof(local6));
            local6.sin6_family = AF_INET6;
            local6.sin6_addr = in6addr_any;
            local6.sin6_port = 0;
            if (bind(sockfd_, reinterpret_cast<struct sockaddr *>(&local6), sizeof(local6)) == -1) {
                close(sockfd_);
                sockfd_ = -1;
                continue;
            }
        }

        // store remote address
        remote_addr_len_ = static_cast<socklen_t>(p->ai_addrlen);
        std::memcpy(&remote_addr_, p->ai_addr, p->ai_addrlen);

        // connect the UDP socket so we can use send/recv and the kernel will
        // filter incoming datagrams from that peer; this also simplifies
        // address-family selection.
        if (connect(sockfd_, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd_);
            sockfd_ = -1;
            continue;
        }

        break;
    }

    freeaddrinfo(res);

    if (sockfd_ == -1) {
        std::cerr << "failed to create or connect UDP socket\n";
    }
}

UDPClient::~UDPClient() {
    if (sockfd_ != -1) close(sockfd_);
}

bool UDPClient::sendMessage(const std::string &msg) const {
    if (sockfd_ == -1) return false;
    ssize_t n = send(sockfd_, msg.data(), msg.size(), 0);
    if (n == -1) {
        perror("send");
        return false;
    }
    return n == static_cast<ssize_t>(msg.size());
}

bool UDPClient::receiveMessage(std::string &out, int timeout_ms) const {
    out.clear();
    if (sockfd_ == -1) return false;

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sockfd_, &readfds);

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int rv = select(sockfd_ + 1, &readfds, nullptr, nullptr, &tv);
    if (rv == -1) {
        perror("select");
        return false;
    }
    if (rv == 0) {
        // timeout
        return false;
    }

    // data available
    const size_t BUF_SZ = 4096;
    std::vector<char> buf(BUF_SZ);
    ssize_t n = recv(sockfd_, buf.data(), buf.size() - 1, 0);
    if (n <= 0) return false;
    buf[n] = '\0';
    out.assign(buf.data(), static_cast<size_t>(n));
    return true;
}
