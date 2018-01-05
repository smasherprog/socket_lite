#include "Socket_Lite.h"
#include <assert.h>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
using namespace std::chrono_literals;
auto echo = "echo test";
void echolistenertest()
{
    double echos = 0.0;
    auto context = SL::NET::CreateContext(SL::NET::ThreadCount(1))
                       ->CreateListener(SL::NET::PortNumber(3000))
                       ->onConnection([&](const std::shared_ptr<SL::NET::ISocket> &socket) {
                           socket->async_read(sizeof(echo), (unsigned char *)echo, [&](size_t bytesread) { echos += 1.0; });
                       })
                       ->onDisconnection([&](const std::shared_ptr<SL::NET::ISocket> &socket) {});
    auto start = std::chrono::high_resolution_clock::now();
    auto listener = context->listen();
    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    std::cout << "Echos per Second " << echos / 10 << std::endl;
}
void echoclienttest()
{
    double echos = 0.0;
    auto context = SL::NET::CreateContext(SL::NET::ThreadCount(1))
                       ->CreateClient()
                       ->onConnection([&](const std::shared_ptr<SL::NET::ISocket> &socket) {
                           socket->async_write(sizeof(echo), (unsigned char *)echo, [&](size_t bytessent) { echos += 1.0; });
                       })
                       ->onDisconnection([&](const std::shared_ptr<SL::NET::ISocket> &socket) {});
    auto start = std::chrono::high_resolution_clock::now();
    auto client = context->connect("localhost", SL::NET::PortNumber(3000));
    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    std::cout << "Echo per Second " << echos / 10 << std::endl;
}
int main(int argc, char *argv[])
{
    echolistenertest();
    echoclienttest();
    return 0;
}
