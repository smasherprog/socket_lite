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
void connect();
SL::NET::Context *context;
std::vector<SL::NET::SocketAddress> addresses;
void myconnectiontest()
{

    std::cout << "Starting My Connections per Second Test" << std::endl;
    connections = 0.0;
    auto porttouse = static_cast<unsigned short>(std::rand() % 3000 + 10000);
    auto listencallback(([](auto) {})); // empty function does nothing
    SL::NET::Context iocontext(SL::NET::ThreadCount(1));
    context = &iocontext;
    SL::NET::Listener listener(iocontext, myechomodels::listengetaddrinfo(nullptr, SL::NET::PortNumber(porttouse), SL::NET::AddressFamily::IPV4),
                               listencallback);
    iocontext.start();
    listener.start();
    addresses = SL::NET::getaddrinfo("127.0.0.1", SL::NET::PortNumber(porttouse));
    connect();
    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    keepgoing = false;
    listener.stop();
    iocontext.stop();
    std::cout << "My Connections per Second " << connections / 10 << std::endl;
}
void connect()
{
    auto socket(std::make_shared<SL::NET::Socket>(*context));
    SL::NET::connect_async(*socket, addresses.back(), [socket](SL::NET::StatusCode) {
        connections += 1.0;
        if (keepgoing) {
            connect();
        }
    });
}
} // namespace myconnectiontest
