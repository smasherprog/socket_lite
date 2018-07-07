#pragma once
#include "Socket_Lite.h"
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace myechomodels {

SL::NET::PlatformSocket listengetaddrinfo(const char *nodename, SL::NET::PortNumber port, SL::NET::AddressFamily family)
{
    auto addrs = SL::NET::getaddrinfo(nodename, port, family);
    for (auto &a : addrs) {
        SL::NET::PlatformSocket h(a.getFamily(), SL::NET::Blocking_Options::BLOCKING);
        if (h.bind(a) == SL::NET::StatusCode::SC_SUCCESS) {
            if (h.listen(INT_MAX) == SL::NET::StatusCode::SC_SUCCESS) {
                [[maybe_unused]] auto reter = h.setsockopt(SL::NET::REUSEADDRTag{}, SL::NET::SockOptStatus::ENABLED);
                return h;
            }
        }
    }
    return SL::NET::PlatformSocket(); // empty socket
}

char writeecho[] = "echo test";
char readecho[] = "echo test";
auto writeechos = 0.0;
bool keepgoing = true;

class session : public std::enable_shared_from_this<session> {

  public:
    session(SL::NET::Socket<std::shared_ptr<session>> socket) : socket_(std::move(socket)) {}
    ~session() { close(); }
    void do_read()
    {
        auto self(shared_from_this());
        socket_.recv_async(sizeof(readecho), (unsigned char *)readecho,
                           [](SL::NET::StatusCode code, std::shared_ptr<session> &userdata) {
                               if (code == SL::NET::StatusCode::SC_SUCCESS) {
                                   userdata->do_read();
                               }
                           },
                           self);
    }

    void do_write()
    {
        auto self(shared_from_this());
        socket_.send_async(sizeof(writeecho), (unsigned char *)writeecho,
                           [](SL::NET::StatusCode code, std::shared_ptr<session> &userdata) {
                               if (code == SL::NET::StatusCode::SC_SUCCESS) {
                                   writeechos += 1.0;
                                   userdata->do_write();
                               }
                           },
                           self);
    }
    SL::NET::Socket<std::shared_ptr<session>> socket_;
    void close() { socket_.close(); }
};
template <class CONTEXTYPE> class asioclient {
  public:
    std::shared_ptr<session> socket_;
    std::vector<SL::NET::SocketAddress> addrs;
    asioclient(CONTEXTYPE &io_context, const char *nodename, SL::NET::PortNumber port) : socket_(std::make_shared<session>(io_context))
    {
        addrs = SL::NET::getaddrinfo(nodename, port);
    }
    void close() { socket_->close(); }
    void do_connect()
    {
        auto self(socket_);
        SL::NET::connect_async(socket_->socket_, addrs.back(),
                               [](SL::NET::StatusCode connectstatus, std::shared_ptr<session> &userdata) {
                                   if (connectstatus == SL::NET::StatusCode::SC_SUCCESS) {
                                       userdata->do_write();
                                       userdata->do_read();
                                   }
                               },
                               self);
    }
};

} // namespace myechomodels
