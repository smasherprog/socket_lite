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
    myechomodels::keepgoing = true;
    myechomodels::writeechos = 0;
    myechomodels::writeecho.resize(buffersize);
    myechomodels::readecho.resize(buffersize);

    auto porttouse = static_cast<unsigned short>(std::rand() % 3000 + 10000);
    SL::NET::Context iocontext(SL::NET::ThreadCount(1));
    myechomodels::asioserver s(iocontext, porttouse);
    s.do_accept();
    iocontext.start();

    myechomodels::asioclient c(iocontext, "127.0.0.1", SL::NET::PortNumber(porttouse));
    c.do_connect();
    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    myechomodels::keepgoing = false;
    std::cout << "My Echo per Second " << myechomodels::writeechos / 10 << std::endl;
    iocontext.stop();
}
} // namespace myechotest
