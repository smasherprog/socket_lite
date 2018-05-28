#pragma once
#include "Socket_Lite.h"
#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace myechomodels {
SL::NET::PlatformSocket listengetaddrinfo(const char *nodename, SL::NET::PortNumber port, SL::NET::AddressFamily family)
{
    SL::NET::PlatformSocket handle;
    auto getaddret = SL::NET::getaddrinfo(nodename, port, family, [&](const SL::NET::sockaddr &s) {
        SL::NET::PlatformSocket h(SL::NET::AddressFamily::IPV4, SL::NET::Blocking_Options::BLOCKING);
        if (h.bind(s) == SL::NET::StatusCode::SC_SUCCESS) {
            if (h.listen(INT_MAX) == SL::NET::StatusCode::SC_SUCCESS) {
                [[maybe_unused]] auto reter = h.setsockopt(SL::NET::REUSEADDRTag{}, SL::NET::SockOptStatus::ENABLED);
                handle = std::move(h);
                return SL::NET::GetAddrInfoCBStatus::FINISHED;
            }
        }
        return SL::NET::GetAddrInfoCBStatus::CONTINUE;
    });
    if (getaddret != SL::NET::StatusCode::SC_SUCCESS) {
        std::cout << "getaddrError" << std::endl;
    }
    return handle;
}

char writeecho[] = "echo test";
char readecho[] = "echo test";
auto readechos = 0.0;
auto writeechos = 0.0;
bool keepgoing = true;

inline void do_read(std::shared_ptr<SL::NET::ISocket> socket)
{
    socket->recv_async(sizeof(readecho), (unsigned char *)readecho, [socket](SL::NET::StatusCode code, size_t) {
        if (code == SL::NET::StatusCode::SC_SUCCESS) {
            do_read(socket);
        }
    });
}

inline void do_write(std::shared_ptr<SL::NET::ISocket> socket)
{
    socket->send_async(sizeof(writeecho), (unsigned char *)writeecho, [socket](SL::NET::StatusCode code, size_t) {
        if (code == SL::NET::StatusCode::SC_SUCCESS) {
            writeechos += 1.0;
            do_write(socket);
        }
    });
}

class asioclient {
  public:
    std::shared_ptr<SL::NET::ISocket> socket_;
    std::vector<SL::NET::sockaddr> addrs;
    asioclient(SL::NET::Context &io_context, const char *nodename, SL::NET::PortNumber port, SL::NET::AddressFamily family)
    {
        socket_ = SL::NET::ISocket::CreateSocket(io_context);
        [[maybe_unused]] auto getaddret = SL::NET::getaddrinfo(nodename, port, family, [&](const SL::NET::sockaddr &s) {
            addrs.push_back(s);
            return SL::NET::GetAddrInfoCBStatus::CONTINUE;
        });
    }
    void close() { socket_->close(); }
    void do_connect()
    {
        SL::NET::connect_async(socket_, addrs.back(), [&](SL::NET::StatusCode connectstatus) {
            if (connectstatus == SL::NET::StatusCode::SC_SUCCESS) {

                do_write(socket_);
                do_read(socket_);
            }
        });
    }
};

} // namespace myechomodels
