#include "Socket_Lite.h"
#include <assert.h>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
using namespace std::chrono_literals;

int main(int argc, char *argv[])
{
    auto context = SL::NET::CreateContext(SL::NET::ThreadCount(1))
                       ->CreateListener(SL::NET::PortNumber(3000))
                       ->onConnection([](const std::shared_ptr<SL::NET::ISocket> &socket) {

                       })
                       ->onMessage([](const std::shared_ptr<SL::NET::ISocket> &socket, const SL::NET::Message &msg) {

                       })
                       ->onDisconnection([](const std::shared_ptr<SL::NET::ISocket> &socket) {

                       });

    return 0;
}
