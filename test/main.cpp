#include "Socket_Lite.h"
#include <assert.h>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
using namespace std::chrono_literals;
void testconnectins()
{
    double connects = 0.0;
    double disconnects = 0.0;
    auto context = SL::NET::CreateContext(SL::NET::ThreadCount(1))
                       ->CreateListener(SL::NET::PortNumber(3000))
                       ->onConnection([&](const std::shared_ptr<SL::NET::ISocket> &socket) { connects += 1.0; })
                       ->onDisconnection([&](const std::shared_ptr<SL::NET::ISocket> &socket) { connects -= 1.0; });
    auto start = std::chrono::high_resolution_clock::now();
    context->listen();
    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    std::cout << "Connections per Second " << disconnects / 10 << std::endl;
}

int main(int argc, char *argv[])
{
    testconnectins();
    return 0;
}
