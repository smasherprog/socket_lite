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
    SL::NET::getaddrinfo(nodename, port, family, [&](const SL::NET::sockaddr &s) {
        SL::NET::PlatformSocket h(SL::NET::AddressFamily::IPV4);
        if (h.bind(s) == SL::NET::StatusCode::SC_SUCCESS) {
            if (h.listen(5) == SL::NET::StatusCode::SC_SUCCESS) {
                h.setsockopt(SL::NET::REUSEADDRTag{}, SL::NET::SockOptStatus::ENABLED);
                handle = std::move(h);
                return SL::NET::GetAddrInfoCBStatus::FINISHED;
            }
        }
        return SL::NET::GetAddrInfoCBStatus::CONTINUE;
    });
    return handle;
}

char writeecho[] = "echo test";
char readecho[] = "echo test";
auto readechos = 0.0;
auto writeechos = 0.0;
auto skipped = 0;
bool keepgoing = true;

class session : public std::enable_shared_from_this<session> {
  public:
    session(SL::NET::Socket &&socket) : socket_(std::move(socket)) {}
    void do_read()
    {
        auto self(shared_from_this());
        socket_.recv_async(sizeof(readecho), (unsigned char *)readecho, [self](SL::NET::StatusCode code, size_t bytesread) {
            if (code == SL::NET::StatusCode::SC_SUCCESS) {
                self->do_read();
            }
        });
    }

    void do_write()
    {
        auto self(shared_from_this());
        socket_.send_async(sizeof(writeecho), (unsigned char *)writeecho, [self](SL::NET::StatusCode code, size_t bytesread) {
            if (code == SL::NET::StatusCode::SC_SUCCESS) {
                writeechos += 1.0;
                self->do_write();
            }
            else {
                skipped += 1;
            }
        });
    }
    SL::NET::Socket socket_;
};
class asioclient {
  public:
    std::shared_ptr<session> socket_;
    SL::NET::Context &Context_;
    std::vector<SL::NET::sockaddr> addrs;
    asioclient(SL::NET::Context &io_context, const char *nodename, SL::NET::PortNumber port, SL::NET::AddressFamily family) : Context_(io_context)
    {
        SL::NET::getaddrinfo(nodename, port, family, [&](const SL::NET::sockaddr &s) {
            addrs.push_back(s);
            return SL::NET::GetAddrInfoCBStatus::CONTINUE;
        });
    }
    void close() { socket_->socket_.close(); }
    SL::NET::StatusCode handleconnect(SL::NET::StatusCode connectstatus, SL::NET::Socket &socket) { return connectstatus; }
    void do_connect()
    {
        SL::NET::connect_async(Context_, addrs.back(), [this](SL::NET::StatusCode connectstatus, SL::NET::Socket socket) {
            if (connectstatus == SL::NET::StatusCode::SC_SUCCESS) {
                socket_ = std::make_shared<session>(std::move(socket));
                socket_->do_write();
                socket_->do_read();
            }
        });
    }
};

} // namespace myechomodels
