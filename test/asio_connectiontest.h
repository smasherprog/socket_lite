#pragma once

#define ASIO_STANDALONE
#include "asio.hpp"

#include "Socket_Lite.h"
#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <ctime>

using namespace std::chrono_literals;

namespace asioconnectiontest {

const int MAXRUNTIMES = 10000;
auto connections = 0.0;
auto keepgoing = true;
using asio::ip::tcp;
class asioserver {
  public:
    asioserver(asio::io_context &io_context, SL::NET::PortNumber port) : 
        acceptor_(io_context, tcp::endpoint(tcp::v4(), port.value)) {}

    void do_accept()
    {
        acceptor_.async_accept([&](std::error_code, tcp::socket) {
            if (keepgoing) {
                do_accept();
            }
        });
    }

    tcp::acceptor acceptor_;
};
tcp::resolver::results_type endpoints;
void connect(asio::io_context &io_context)
{
    auto socket = std::make_shared<tcp::socket>(io_context);
    asio::async_connect(*socket, endpoints, [socket, &io_context](std::error_code, tcp::endpoint) {
        connections += 1.0;
        if (keepgoing) {
            connect(io_context);
        }
    });
}
void connectiontest()
{
    std::cout << "Starting Asio Connections per Second Test" << std::endl;
    connections = 0.0;
    asio::io_context iocontext;
    auto porttouse = static_cast<unsigned short>(std::rand() % 3000 + 10000);
    asioserver s(iocontext, SL::NET::PortNumber(porttouse));
    s.do_accept();

    tcp::resolver resolver(iocontext);
    endpoints = resolver.resolve("127.0.0.1", std::to_string(porttouse));

    std::thread t([&]() { iocontext.run(); });
    connect(iocontext);

    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    keepgoing = false;
    std::cout << "Asio Connections per Second " << connections / 10 << std::endl;
    iocontext.stop();
    s.acceptor_.cancel();
    s.acceptor_.close();
    if (t.joinable())
        t.join();
}
} // namespace asioconnectiontest
