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
void myechotest()
{
    std::cout << "Starting My Echo Test" << std::endl;
    myechomodels::writeechos = 0;
    myechomodels::keepgoing = true;
    auto porttouse = static_cast<unsigned short>(std::rand() % 3000 + 10000);
    auto iocontext = SL::NET::createContext(SL::NET::ThreadCount(1))
                         ->AddListener(myechomodels::listengetaddrinfo(nullptr, SL::NET::PortNumber(porttouse), SL::NET::AddressFamily::IPV4),
                                       [](SL::NET::Socket socket) {
                                           if (myechomodels::keepgoing) {
                                               auto s = std::make_shared<myechomodels::session>(std::move(socket));
                                               s->do_read();
                                               s->do_write();
                                           }
                                       })
                         ->run();

    myechomodels::asioclient c(*iocontext, "127.0.0.1", SL::NET::PortNumber(porttouse));
    c.do_connect();

    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    myechomodels::keepgoing = false;
    std::cout << "My Echo per Second " << myechomodels::writeechos / 10 << std::endl;
    c.close();
}

} // namespace myechotest
