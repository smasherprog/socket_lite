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
int connectnumber =0;
int acceptnumber=0;
std::vector<SL::NET::sockaddr> addresses;

void connect(SL::NET::Context &iocontext)
{
    auto s = std::make_shared<SL::NET::Socket>(iocontext);
    SL::NET::connect_async(*s, addresses.back(), [&iocontext, s](SL::NET::StatusCode code) {
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
    SL::NET::Context iocontext(SL::NET::ThreadCount(1));
    SL::NET::Acceptor a;
    a.AcceptSocket = myechomodels::listengetaddrinfo(nullptr, SL::NET::PortNumber(porttouse), SL::NET::AddressFamily::IPV4);
    a.AcceptHandler = ([](SL::NET::Socket) {  });
    a.Family = SL::NET::AddressFamily::IPV4;
    SL::NET::Listener Listener(iocontext, std::move(a));
     [[maybe_unused]] auto gerro = SL::NET::getaddrinfo("127.0.0.1", SL::NET::PortNumber(porttouse), SL::NET::AddressFamily::IPV4, [&](const SL::NET::sockaddr &s) {
        addresses.push_back(s);
        return SL::NET::GetAddrInfoCBStatus::CONTINUE;
    });
      
    connect(iocontext);
    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    keepgoing = false;
    std::cout << "My Connections per Second " << connections / 10 << std::endl;

}
} // namespace myconnectiontest
