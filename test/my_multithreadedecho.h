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
    myechomodels::writeechob.resize(buffersize);
    myechomodels::readechob.resize(buffersize);

    auto porttouse = static_cast<unsigned short>(std::rand() % 3000 + 10000);
    SL::Network::io_thread_service context(4);
     
    auto acceptsocket(myechomodels::Create_and_Bind_Listen_Socket(nullptr, porttouse, SL::Network::SocketType::TCP, SL::Network::AddressFamily::IPV4, context.getio_service()));
    auto t1(std::thread([&]() { myechomodels::asioserver s(acceptsocket.value()); s.do_accept().wait(); }));
 
    myechomodels::awaitclient c("127.0.0.1", porttouse, context.getio_service());
    myechomodels::awaitclient c1("127.0.0.1", porttouse, context.getio_service());
    myechomodels::awaitclient c2("127.0.0.1", porttouse, context.getio_service());
    c.do_connect();
    c1.do_connect();
    c2.do_connect();

    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    myechomodels::keepgoing = false;
    t1.join();
    std::cout << "My 4 thread Echos per Second " << myechomodels::writeechos / 10 << std::endl;

}

} // namespace mymultithreadedechotest
