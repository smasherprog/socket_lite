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
            std::cout << "Listen echo received " << std::endl;
            socket->async_write(sizeof(readecho), (unsigned char *)readecho, [socket](long long bytesread) {
                if (bytesread >= 0) {
                    std::cout << "Listen echo responce sent  " << std::endl;
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
            std::cout << "Listen echo sent " << std::endl;
            socket->async_read(sizeof(writeecho), (unsigned char *)writeecho, [socket](long long bytesread) {
                if (bytesread >= 0) {
                    std::cout << "Listen echo received " << std::endl;
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

void WriteConnectionInfo(const std::shared_ptr<SL::NET::ISocket> &socket)
{
    if (auto peerinfo = socket->get_PeerInfo(); peerinfo.has_value()) {
        std::cout << "Address: '" << peerinfo->Address << "' Port:'" << peerinfo->Port << "' Family:'"
                  << (peerinfo->Family == SL::NET::Address_Family::IPV4 ? "ipv4'\n" : "ipv6'\n");
    }
}
void echolistenertest()
{
    auto listencontext = SL::NET::CreateListener();
    if (!listencontext->bind(SL::NET::PortNumber(3000))) {
        std::cout << "Listen bind failed " << std::endl;
    }
    else {
        std::cout << "Listen bind success 3000" << std::endl;
    }
    if (!listencontext->listen()) {
        std::cout << "Listen listen failed " << std::endl;
    }
    else {
        std::cout << "Listen listen success " << std::endl;
    }

    listencontext->onConnection = [](const std::shared_ptr<SL::NET::ISocket> &socket) {
        std::cout << "Listen Socket Connected " << std::endl;
        WriteConnectionInfo(socket);
        echolistenread(socket);
    };

    listencontext->run(SL::NET::ThreadCount(1));

    auto clientcontext = SL::NET::CreateClient();
    if (!clientcontext->async_connect("::1", SL::NET::PortNumber(3000))) {
        std::cout << "async_connect failed " << std::endl;
    }
    else {
        std::cout << "async_connect success 3000" << std::endl;
    }
    clientcontext->onConnection = [](const std::shared_ptr<SL::NET::ISocket> &socket) {
        if (!socket) {
            std::cout << "Client Connect Failed! " << std::endl;
        }
        else {
            std::cout << "Client Socket Connected " << std::endl;
            WriteConnectionInfo(socket);
            echolistenwrite(socket);
        }
    };
    auto start = std::chrono::high_resolution_clock::now();
    clientcontext->run(SL::NET::ThreadCount(1));
    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    std::cout << "Echo per Second " << writeechos / 10 << std::endl;
}
void echoclienttest() {}
void builderecholistenertest()
{
    auto context = SL::NET::CreateListener(SL::NET::ThreadCount(1))
                       ->onConnection([](const std::shared_ptr<SL::NET::ISocket> &socket) {
                           std::cout << "Listen Socket Connected " << std::endl;
                           WriteConnectionInfo(socket);
                           echolistenread(socket);
                       })
                       ->bind(SL::NET::PortNumber(3001));

    auto start = std::chrono::high_resolution_clock::now();
    context->listen();
    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    std::cout << "Echos per Second " << readechos / 10 << std::endl;
}
void builderechoclienttest()
{
    auto context = SL::NET::CreateClient(SL::NET::ThreadCount(1))->onConnection([](const std::shared_ptr<SL::NET::ISocket> &socket) {
        std::cout << "Client Socket Connected " << std::endl;
        WriteConnectionInfo(socket);
        echolistenwrite(socket);
    });

    auto start = std::chrono::high_resolution_clock::now();
    context->async_connect("localhost", SL::NET::PortNumber(3001));
    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    std::cout << "Echo per Second " << writeechos / 10 << std::endl;
}
int main(int argc, char *argv[])
{
    // the verbode ways to use this library
    echolistenertest();
    echoclienttest();
    // same code as sbove except using the builders. This should be the preferred way to use this library.......
    //   builderecholistenertest();
    //    builderechoclienttest();
    return 0;
}
