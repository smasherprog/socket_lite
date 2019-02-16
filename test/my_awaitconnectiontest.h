#pragma once
#include "Socket_Lite.h"
#include "my_echomodels.h"
#include <chrono>
#include <iostream>
#include <vector>
#include <future>

using namespace std::chrono_literals;
namespace myawaitconnectiontest {
    auto connections = 0.0;
    bool keepgoing = true; 
    std::vector<SL::NET::SocketAddress> endpoints;

    std::future<void> connect()
    {
        while (keepgoing) {
            auto[status, socket] = co_await SL::NET::async_connect(endpoints.back());
            connections += 1.0;
        }
    }
    std::future<void> accept(SL::NET::AwaitablePlatformSocket& acceptsocket)
    {
        while (keepgoing) {
            auto[status, socket] = co_await acceptsocket.async_accept();
        }
    }
    void myconnectiontest()
    {

        std::cout << "Starting My Await Connections per Second Test" << std::endl;
        connections = 0.0;
        auto porttouse = static_cast<unsigned short>(std::rand() % 3000 + 10000);

        endpoints = SL::NET::getaddrinfo("127.0.0.1", SL::NET::PortNumber(porttouse));

        std::thread([=]() { 
            auto socket(myechomodels::listengetaddrinfo(nullptr, SL::NET::PortNumber(porttouse), SL::NET::AddressFamily::IPV4));
            accept(socket).wait(); 
        }).detach();
        std::thread([]() {  connect().wait(); }).detach();
     
        std::this_thread::sleep_for(10s); // sleep for 10 seconds
        keepgoing = false;
        std::cout << "My Await Connections per Second " << connections / 10 << std::endl;
    }
} // namespace myconnectiontest
