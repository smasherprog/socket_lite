#include "Socket_Lite.h"
#include <assert.h>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
using namespace std::chrono_literals;
auto writeecho = "echo test";
auto readecho = "echo test";
auto readechos = 0.0;
auto writeechos = 0.0;
void echolistenread(const std::shared_ptr<SL::NET::ISocket> &socket)
{
    socket->async_read(sizeof(readecho), (unsigned char *)readecho, [socket](long long bytesread) {
        if (bytesread >= 0) {
            socket->async_write(sizeof(readecho), (unsigned char *)readecho, [socket](long long bytesread) {
                if (bytesread >= 0) {
                    readechos += 1.0;
                    echolistenread(socket);
                }
                else {
                    // disconnected
                }
            });
        }
        else {
            // disconnected
        }
    });
}

void echolistenwrite(const std::shared_ptr<SL::NET::ISocket> &socket)
{
    socket->async_write(sizeof(writeecho), (unsigned char *)writeecho, [socket](long long bytesread) {
        if (bytesread >= 0) {
            socket->async_read(sizeof(writeecho), (unsigned char *)writeecho, [socket](long long bytesread) {
                if (bytesread >= 0) {
                    writeechos += 1.0;
                    echolistenwrite(socket);
                }
                else {
                    // disconnected
                }
            });
        }
        else {
            // disconnected
        }
    });
}

void echolistenertest()
{

    auto context = SL::NET::CreateListener(SL::NET::ThreadCount(1))
                       ->onConnection([&](const std::shared_ptr<SL::NET::ISocket> &socket) { echolistenread(socket); })
                       ->onDisconnection([&](const std::shared_ptr<SL::NET::ISocket> &socket) {});
    auto start = std::chrono::high_resolution_clock::now();
    auto listener = context->bind(SL::NET::PortNumber(3000))->listen();
    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    std::cout << "Echos per Second " << readechos / 10 << std::endl;
}
void echoclienttest()
{

    auto context = SL::NET::CreateClient(SL::NET::ThreadCount(1))
                       ->onConnection([&](const std::shared_ptr<SL::NET::ISocket> &socket) { echolistenwrite(socket); })
                       ->onDisconnection([&](const std::shared_ptr<SL::NET::ISocket> &socket) {});
    auto start = std::chrono::high_resolution_clock::now();
    auto client = context->async_connect("localhost", SL::NET::PortNumber(3000));
    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    std::cout << "Echo per Second " << writeechos / 10 << std::endl;
}
int main(int argc, char *argv[])
{
    echolistenertest();
    echoclienttest();
    return 0;
}
