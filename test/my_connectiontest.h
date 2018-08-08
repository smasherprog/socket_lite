#pragma once
#include "Socket_Lite.h"
#include "my_echomodels.h"
#include <chrono>
#include <iostream>
#include <vector>

using namespace std::chrono_literals;
namespace myconnectiontest {
auto connections = 0.0;
bool keepgoing = true;

class asioserver {
  public:
    asioserver(SL::NET::Context &io_context, unsigned short port)
        : acceptor_(myechomodels::listengetaddrinfo(nullptr, SL::NET::PortNumber(port), SL::NET::AddressFamily::IPV4, io_context))
    {
    }

    void do_accept()
    {
        acceptor_.accept([&](auto, auto) {
            if (keepgoing) {
                do_accept();
            }
        });
    }

    SL::NET::AsyncAcceptor acceptor_;
};
std::vector<SL::NET::SocketAddress> endpoints;
void connect(SL::NET::Context &io_context)
{
    auto socket(std::make_shared<SL::NET::AsyncSocket>(io_context));
    SL::NET::AsyncSocket::connect(*socket, endpoints.back(), [socket, &io_context](SL::NET::StatusCode) {
        connections += 1.0;
        if (keepgoing) {
            connect(io_context);
        }
    });
}

void myconnectiontest()
{

    std::cout << "Starting My Connections per Second Test" << std::endl;
    connections = 0.0;
    auto porttouse = static_cast<unsigned short>(std::rand() % 3000 + 10000);
    SL::NET::Context iocontext(SL::NET::ThreadCount(1));

    asioserver s(iocontext, porttouse);
    s.do_accept();
    iocontext.start();

    endpoints = SL::NET::getaddrinfo("127.0.0.1", SL::NET::PortNumber(porttouse));
    connect(iocontext);
    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    keepgoing = false;
    iocontext.stop();
    std::cout << "My Connections per Second " << connections / 10 << std::endl;
}
} // namespace myconnectiontest
