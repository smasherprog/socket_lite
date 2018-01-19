#pragma once
#include "Socket_Lite.h"
#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace myconnectiontest {

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
    void close() { Listener->close(); }
    std::shared_ptr<SL::NET::IIO_Context> &IOContext;
    std::shared_ptr<SL::NET::IListener> Listener;
};

void myconnectiontest()
{
    auto iocontext = SL::NET::CreateIO_Context();
    asioserver s(iocontext, SL::NET::PortNumber(3000));
    auto addresses = SL::NET::getaddrinfo("127.0.0.1", SL::NET::PortNumber(3000), SL::NET::Address_Family::IPV4);

    iocontext->run(SL::NET::ThreadCount(2));
    auto start = std::chrono::high_resolution_clock::now();
    for (auto i = 0; i < 10000; i++) {
        auto socket_ = SL::NET::CreateSocket(iocontext, SL::NET::Address_Family::IPV4);
        socket_->connect(iocontext, addresses.back(), [socket_](SL::NET::ConnectionAttemptStatus connectstatus) {
            if (connectstatus == SL::NET::ConnectionAttemptStatus::SuccessfullConnect) {
                connections += 1.0;
            }
            socket_->close();
        });
    }
    s.close();
    auto seconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();

    std::cout << "Connections per Second " << 10000 * 1000 / seconds << std::endl;
}

} // namespace myconnectiontest
