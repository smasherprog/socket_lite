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
bool keepgoing = true;
class asioserver : public std::enable_shared_from_this<asioserver> {
  public:
    asioserver(SL::NET::Context &io_context, SL::NET::PortNumber port) : Listener(io_context, port, SL::NET::AddressFamily::IPV4, ec)
    {
        if (ec != SL::NET::StatusCode::SC_SUCCESS) {
            std::cout << "Listener failed to create code:" << ec << std::endl;
        }
    }
    void do_accept()
    {
        auto self(shared_from_this());
        Listener.accept([self](SL::NET::StatusCode code, SL::NET::Socket socket) {
            if (keepgoing) {
                //     std::cout << "Accept" << std::endl;
                self->do_accept();
            }
        });
    }
    void close() { Listener.close(); }
    SL::NET::StatusCode ec;
    SL::NET::Listener Listener;
};

std::vector<SL::NET::sockaddr> addresses;

void connect(SL::NET::Context &iocontext)
{
    auto socket_ = std::make_shared<SL::NET::Socket>(iocontext);
    SL::NET::connect(*socket_, addresses.back(), [&iocontext, socket_](SL::NET::StatusCode connectstatus) {
        connections += 1.0;
        //   std::cout << "connect" << connectstatus << std::endl;
        if (keepgoing) {
            connect(iocontext);
        }
    });
}
void myconnectiontest()
{
    std::cout << "Starting My Connections per Second Test" << std::endl;
    connections = 0.0;
    SL::NET::Context iocontext(SL::NET::ThreadCount(1));
    auto porttouse = static_cast<unsigned short>(std::rand() % 3000 + 10000);
    auto s(std::make_shared<asioserver>(iocontext, SL::NET::PortNumber(porttouse)));
    s->do_accept();
    auto[code, addrs] = SL::NET::getaddrinfo("127.0.0.1", SL::NET::PortNumber(porttouse), SL::NET::AddressFamily::IPV4);
    if (code != SL::NET::StatusCode::SC_SUCCESS) {
        std::cout << "Error code:" << code << std::endl;
    }
    addresses = addrs;
    iocontext.run();
    connect(iocontext);
    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    keepgoing = false;
    std::cout << "My Connections per Second " << connections / 10 << std::endl;
    s->close();
}
} // namespace myconnectiontest
