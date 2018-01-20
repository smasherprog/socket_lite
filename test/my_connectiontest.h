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

const int MAXRUNTIMES = 10000;
auto connections = 0.0;

class asioserver {
  public:
    asioserver(std::shared_ptr<SL::NET::IIO_Context> &io_context, SL::NET::PortNumber port) : IOContext(io_context)
    {

        std::shared_ptr<SL::NET::ISocket> listensocket;
        for (auto &address : SL::NET::getaddrinfo(nullptr, port, SL::NET::Address_Family::IPV4)) {
            auto lsock = SL::NET::CreateSocket(IOContext, SL::NET::Address_Family::IPV4);
            if (lsock->bind(address)) {
                if (lsock->listen(5)) {
                    listensocket = lsock;
                }
            }
        }
        listensocket->setsockopt<SL::NET::Socket_Options::O_REUSEADDR>(true);
        Listener = SL::NET::CreateListener(io_context, std::move(listensocket));
        do_accept();
    }
    ~asioserver() { close(); }
    void do_accept()
    {
        auto newsocket = SL::NET::CreateSocket(IOContext, SL::NET::Address_Family::IPV4);
        Listener->async_accept(newsocket, [newsocket, this](bool connectsuccess) {
            if (connectsuccess) {
                do_accept();
            }
        });
    }
    std::chrono::time_point<std::chrono::steady_clock> start = std::chrono::high_resolution_clock::now();
    void close() { Listener->close(); }
    std::shared_ptr<SL::NET::IIO_Context> &IOContext;
    std::shared_ptr<SL::NET::IListener> Listener;
};
bool keepgoing = true;
std::vector<SL::NET::sockaddr> addresses;
void connect(std::shared_ptr<SL::NET::IIO_Context> iocontext)
{
    auto socket_ = SL::NET::CreateSocket(iocontext, SL::NET::Address_Family::IPV4);
    socket_->connect(iocontext, addresses.back(), [iocontext, socket_](SL::NET::ConnectionAttemptStatus connectstatus) {
        connections += 1.0;
        if (keepgoing) {
            connect(iocontext);
        }
    });
}
void myconnectiontest()
{
    connections = 0.0;
    auto iocontext = SL::NET::CreateIO_Context();
    auto porttouse = std::rand() % 3000 + 10000;
    asioserver s(iocontext, SL::NET::PortNumber(porttouse));
    addresses = SL::NET::getaddrinfo("127.0.0.1", SL::NET::PortNumber(porttouse), SL::NET::Address_Family::IPV4);

    iocontext->run(SL::NET::ThreadCount(2));
    connect(iocontext);

    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    keepgoing = false;
    std::cout << "My Connections per Second " << connections / 10 << std::endl;
}

} // namespace myconnectiontest