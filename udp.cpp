#include "udp.hpp"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <vector>
#include <arpa/inet.h>
#include <poll.h>
#include <errno.h>

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
    // Use poll() to wait for readability, then read all available datagrams
    struct pollfd pfd;
    pfd.fd = sockfd_;
    pfd.events = POLLIN;
    pfd.revents = 0;

    int rv = poll(&pfd, 1, timeout_ms);
    if (rv == -1) {
        perror("poll");
        return false;
    }
    if (rv == 0) {
        // timeout
        return false;
    }

    // One or more datagrams available. Read in a loop using MSG_DONTWAIT to
    // drain the socket so we don't leave pending messages unread.
    const size_t BUF_SZ = 8192;
    std::vector<char> buf(BUF_SZ);
    bool got = false;
    while (true) {
        ssize_t n = recv(sockfd_, buf.data(), buf.size() - 1, MSG_DONTWAIT);
        if (n > 0) {
            buf[n] = '\0';
            std::string msg(buf.data(), static_cast<size_t>(n));
            // print each received message so caller can inspect raw data
            std::cout << "udp recv: " << msg << std::endl;
            // store the last received message in out
            out = msg;
            got = true;
            // continue reading until EAGAIN/EWOULDBLOCK
            continue;
        }
        if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            // no more data
            break;
        }
        // other errors or socket closed
        if (n == 0) {
            // socket closed
            break;
        }
        if (n == -1) {
            perror("recv");
            break;
        }
    }
    return got;
}
