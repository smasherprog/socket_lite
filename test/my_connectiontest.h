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

const int MAXRUNTIMES = 10000;
auto connections = 0.0;
bool keepgoing = true;

std::vector<SL::NET::sockaddr> addresses;
void connect(SL::NET::Context &iocontext);
void continue_connect(SL::NET::Context &iocontext)
{
    connections += 1.0;
    if (keepgoing) {
        connect(iocontext);
    }
}
void connect(SL::NET::Context &iocontext)
{
    while (keepgoing) {
        auto[ec, sock] =
            SL::NET::connect(iocontext, addresses.back(), [&iocontext](SL::NET::StatusCode, SL::NET::Socket sock) { continue_connect(iocontext); });
        if (ec == SL::NET::StatusCode::SC_SUCCESS) {
            // connection completed immediatly, and the callback will not be call, must handle it here
            connections += 1.0;
        }
        else if (ec == SL::NET::StatusCode::SC_PENDINGIO) {
            // the callback will be executed some time later
            break;
        }
    }
}
void myconnectiontest()
{
    std::cout << "Starting My Connections per Second Test" << std::endl;
    connections = 0.0;
    SL::NET::Context iocontext(SL::NET::ThreadCount(1));
    auto porttouse = static_cast<unsigned short>(std::rand() % 3000 + 10000);

    SL::NET::Acceptor a;
    a.AcceptSocket = myechomodels::listengetaddrinfo(nullptr, SL::NET::PortNumber(porttouse), SL::NET::AddressFamily::IPV4);
    a.AcceptHandler = ([](SL::NET::Socket socket) {});
    a.Family = SL::NET::AddressFamily::IPV4;
    SL::NET::Listener Listener(iocontext, std::move(a));
    Listener.start();
    SL::NET::getaddrinfo("127.0.0.1", SL::NET::PortNumber(porttouse), SL::NET::AddressFamily::IPV4, [&](const SL::NET::sockaddr &s) {
        addresses.push_back(s);
        return SL::NET::GetAddrInfoCBStatus::CONTINUE;
    });
    iocontext.run();

    connect(iocontext);
    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    keepgoing = false;
    std::cout << "My Connections per Second " << connections / 10 << std::endl;
}
} // namespace myconnectiontest
