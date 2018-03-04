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

namespace myechotest
{

void myechotest()
{
    std::cout << "Starting My Echo Test" << std::endl;
    myechomodels::writeechos=0;
    auto porttouse = static_cast<unsigned short>(std::rand() % 3000 + 10000);
    auto iocontext = SL::NET::CreateContext();
    auto s(std::make_shared<myechomodels::asioserver>(iocontext, SL::NET::PortNumber(porttouse)));
    s->do_accept();
    auto[code, addresses] = SL::NET::getaddrinfo("127.0.0.1", SL::NET::PortNumber(porttouse), SL::NET::AddressFamily::IPV4);
    if (code != SL::NET::StatusCode::SC_SUCCESS) {
        std::cout << "Error code:" << code << std::endl;
    }
    myechomodels::asioclient c(iocontext, addresses);
    c.do_connect();
    iocontext->run(SL::NET::ThreadCount(2));
    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    c.close();
    s->close();
    std::cout << "My Echo per Second " << myechomodels::writeechos / 20 << std::endl;
}

} // namespace myechotest
