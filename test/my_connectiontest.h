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

std::vector<SL::NET::sockaddr> addresses;

void connect(SL::NET::Context &iocontext)
{ 
    SL::NET::connect(iocontext, addresses.back(), [&iocontext](SL::NET::StatusCode connectstatus, SL::NET::Socket&& sock) {
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
    SL::NET::StatusCode ec;
    SL::NET::Acceptor acceptor(SL::NET::PortNumber(porttouse), SL::NET::AddressFamily::IPV4, ec);
    if (ec != SL::NET::StatusCode::SC_SUCCESS) {
        std::cout << "Acceptor failed to create code:" << ec << std::endl;
    }
    acceptor.set_handler([](SL::NET::Socket socket) {

    });

    SL::NET::Listener Listener(iocontext, std::move(acceptor));
 
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
 
}
} // namespace myconnectiontest
