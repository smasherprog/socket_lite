#pragma once

#define ASIO_STANDALONE
#include "asio.hpp"
#include "asio_echomodels.h"
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
using namespace std::chrono_literals;

namespace asiotest {

using asio::ip::tcp;
void asioechotest()
{
    std::cout << "Starting ASIO Echo Test" << std::endl;
    asiomodels::writeechos = 0;
    asiomodels::keepgoing = true;

    auto porttouse = static_cast<unsigned short>(std::rand() % 3000 + 10000);
    asio::io_context iocontext;
    auto s(std::make_shared<asiomodels::asioserver>(iocontext, porttouse));
    s->do_accept();

    tcp::resolver resolver(iocontext);
    auto endpoints = resolver.resolve("127.0.0.1", std::to_string(porttouse));
    asiomodels::asioclient c(iocontext);
    c.do_connect(endpoints);
    std::thread t([&iocontext]() { iocontext.run(); });

    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    asiomodels::keepgoing = false;
    std::cout << "ASIO Echo per Second " << asiomodels::writeechos / 10 << std::endl;
    iocontext.stop();
    s->acceptor_.cancel();
    s->acceptor_.close();
    c.close();
    t.join();
}

} // namespace asiotest
