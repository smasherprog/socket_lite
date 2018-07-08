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

namespace myechotest {
void myechotest(int buffersize = 128)
{
    std::cout << "Starting My Echo Test with buffer size of " << buffersize << " bytes" << std::endl;
    myechomodels::writeechos = 0;
    myechomodels::keepgoing = true;
    myechomodels::writeecho.resize(buffersize);
    myechomodels::readecho.resize(buffersize); 

    auto porttouse = static_cast<unsigned short>(std::rand() % 3000 + 10000);
    SL::NET::Context iocontext(SL::NET::ThreadCount(1));
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
    c.do_connect();
    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    myechomodels::keepgoing = false;
    std::cout << "My Echo per Second " << myechomodels::writeechos / 10 << std::endl;
    listener.stop();
    iocontext.stop();
}

} // namespace myechotest
