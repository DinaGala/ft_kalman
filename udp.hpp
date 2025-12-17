// udp.hpp - simple UDP client for handshake and message exchange
#pragma once

#include <string>
#include <optional>

// POSIX socket types used in the class (sockaddr_storage, socklen_t)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


class UDPClient {
    public:
        // host: remote host (e.g. 127.0.0.1), port: remote port (e.g. "4242")
        UDPClient(const std::string &host, const std::string &port);
        ~UDPClient();

        // send a message to the remote endpoint; returns true on success
        bool sendMessage(const std::string &msg) const;

        // receive a message into out (string). timeout_ms is milliseconds.
        // returns true if a message was received, false on timeout or error.
        bool receiveMessage(std::string &out, int timeout_ms) const;

    private:
        int sockfd_;
        std::string host_;
        std::string port_;
        struct sockaddr_storage remote_addr_;
        socklen_t remote_addr_len_;

        // non-copyable
        UDPClient(const UDPClient &) = delete;
        UDPClient &operator=(const UDPClient &) = delete;
};
