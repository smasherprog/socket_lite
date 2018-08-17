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
void myechotest(int buffersize = 128)
{
    std::cout << "Starting My 4 thread Echos Test with buffer size of " << buffersize << " bytes" << std::endl;
    myechomodels::writeechos = 0;
    myechomodels::keepgoing = true;
    myechomodels::writeecho.resize(buffersize);
    myechomodels::readecho.resize(buffersize);

    auto porttouse = static_cast<unsigned short>(std::rand() % 3000 + 10000);
    SL::NET::Context iocontext(SL::NET::ThreadCount(4));

    SL::NET::AsyncAcceptor s(myechomodels::listengetaddrinfo(nullptr, SL::NET::PortNumber(porttouse), SL::NET::AddressFamily::IPV4, iocontext),
                             [](auto socket) {
                                 if (myechomodels::keepgoing) {
                                     auto s = std::make_shared<myechomodels::session>(std::move(socket));
                                     s->do_read();
                                     s->do_write();
                                 }
                             });

    myechomodels::asioclient c(iocontext, "127.0.0.1", SL::NET::PortNumber(porttouse));
    myechomodels::asioclient c1(iocontext, "127.0.0.1", SL::NET::PortNumber(porttouse));
    myechomodels::asioclient c2(iocontext, "127.0.0.1", SL::NET::PortNumber(porttouse));
    c.do_connect();
    c1.do_connect();
    c2.do_connect();

    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    myechomodels::keepgoing = false;
    std::cout << "My 4 thread Echos per Second " << myechomodels::writeechos / 10 << std::endl;

}

} // namespace mymultithreadedechotest
