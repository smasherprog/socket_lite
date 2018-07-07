#pragma once
#include "Socket_Lite.h"
#include "my_echomodels.h"
#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace mymultithreadedechotest {
void myechotest()
{
    std::cout << "Starting My 4 thread Echos Test" << std::endl;
    myechomodels::writeechos = 0;
    myechomodels::keepgoing = true;
    auto porttouse = static_cast<unsigned short>(std::rand() % 3000 + 10000);
    SL::NET::Context<std::shared_ptr<myechomodels::session>> iocontext(SL::NET::ThreadCount(4));

    auto listencallback([](auto socket) {
        if (myechomodels::keepgoing) {
            auto asock = std::make_shared<myechomodels::session>(std::move(socket));
            asock->do_read();
            asock->do_write();
        }
    });
    SL::NET::Listener listener(iocontext, myechomodels::listengetaddrinfo(nullptr, SL::NET::PortNumber(porttouse), SL::NET::AddressFamily::IPV4),
                               listencallback);

    iocontext.start();
    listener.start(); 
    myechomodels::asioclient c(iocontext, "127.0.0.1", SL::NET::PortNumber(porttouse));
    myechomodels::asioclient c1(iocontext, "127.0.0.1", SL::NET::PortNumber(porttouse));
    myechomodels::asioclient c2(iocontext, "127.0.0.1", SL::NET::PortNumber(porttouse));
    c.do_connect();
    c1.do_connect();
    c2.do_connect();

    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    myechomodels::keepgoing = false;
    std::cout << "My 4 thread Echos per Second " << myechomodels::writeechos / 10 << std::endl;
    listener.stop();
    iocontext.stop();
}

} // namespace mymultithreadedechotest
