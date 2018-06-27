#pragma once
#include "Socket_Lite.h"
#include "my_echomodels.h"
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
std::vector<SL::NET::SocketAddress> addresses;

void connect(SL::NET::Context &iocontext)
{
    auto socket = std::make_shared<SL::NET::Socket>(iocontext);
    SL::NET::connect_async(*socket, addresses.back(), [&iocontext, socket](SL::NET::StatusCode) {
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
    auto porttouse = static_cast<unsigned short>(std::rand() % 3000 + 10000);
    auto iocontext = SL::NET::createContext(SL::NET::ThreadCount(1))
                         ->AddListener(myechomodels::listengetaddrinfo(nullptr, SL::NET::PortNumber(porttouse), SL::NET::AddressFamily::IPV4),
                                       ([](SL::NET::Socket) {}))
                         ->run();

    addresses = SL::NET::getaddrinfo("127.0.0.1", SL::NET::PortNumber(porttouse));
    connect(*iocontext);
    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    keepgoing = false;
    std::cout << "My Connections per Second " << connections / 10 << std::endl;
}
} // namespace myconnectiontest
