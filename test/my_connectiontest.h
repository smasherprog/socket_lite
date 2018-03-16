#pragma once
#include "Socket_Lite.h"
#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <ctime>

using namespace std::chrono_literals;

namespace myconnectiontest {

auto connections = 0.0;
bool keepgoing = true;
class asioserver : public std::enable_shared_from_this<asioserver> {
  public:
    asioserver(std::shared_ptr<SL::NET::Context> &io_context, SL::NET::PortNumber port)
    {

        std::shared_ptr<SL::NET::Socket> listensocket;
        auto[code, addresses] = SL::NET::getaddrinfo(nullptr, port, SL::NET::AddressFamily::IPV4);
        if (code != SL::NET::StatusCode::SC_SUCCESS) {
            std::cout << "Error code:" << code << std::endl;
        }
        for (auto &address : addresses) {
            auto lsock = std::make_shared<SL::NET::Socket>(io_context.get());
            if (lsock->bind(address) == SL::NET::StatusCode::SC_SUCCESS) {
                if (lsock->listen(5) == SL::NET::StatusCode::SC_SUCCESS) {
                    listensocket = lsock;
                }
            }
        }
        listensocket->setsockopt<SL::NET::SocketOptions::O_REUSEADDR>(SL::NET::SockOptStatus::ENABLED);
        Listener = io_context->CreateListener(std::move(listensocket));
    }
    ~asioserver() { close(); }
    void do_accept()
    {
        auto self(shared_from_this());
        Listener->accept([self](SL::NET::StatusCode code, const std::shared_ptr<SL::NET::Socket> &socket) {
            if (keepgoing) {
                self->do_accept();
            }
        });
    }
    void close() { Listener->close(); }

    std::shared_ptr<SL::NET::IListener> Listener;
};

std::vector<SL::NET::sockaddr> addresses;

void connect(std::shared_ptr<SL::NET::Context> iocontext)
{
    auto socket_ = std::make_shared<SL::NET::Socket>(iocontext.get());
    socket_->connect(addresses.back(), [iocontext, socket_](SL::NET::StatusCode connectstatus) {
        connections += 1.0;
        if (keepgoing) {
            connect(iocontext);
        }
    });
}
void myconnectiontest()
{
    std::cout << "Starting My Connections per Second Test" << std::endl;
    connections = 0.0;
    auto iocontext = std::make_shared<SL::NET::Context>(SL::NET::ThreadCount(1));
    auto porttouse = static_cast<unsigned short>(std::rand() % 3000 + 10000);
    auto s(std::make_shared<asioserver>(iocontext, SL::NET::PortNumber(porttouse)));
    s->do_accept();
    auto[code, addrs] = SL::NET::getaddrinfo("127.0.0.1", SL::NET::PortNumber(porttouse), SL::NET::AddressFamily::IPV4);
    if (code != SL::NET::StatusCode::SC_SUCCESS) {
        std::cout << "Error code:" << code << std::endl;
    }
    addresses = addrs;
    iocontext->run();
    connect(iocontext);
    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    keepgoing = false;
    std::cout << "My Connections per Second " << connections / 10 << std::endl;
    s->close();
}
} // namespace myconnectiontest
