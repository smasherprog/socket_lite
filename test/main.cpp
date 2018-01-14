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

void echolistenertest()
{
    auto iocontext = SL::NET::CreateIO_Context();
    std::shared_ptr<SL::NET::ISocket> listensocket;
    for (auto &address : SL::NET::getaddrinfo(nullptr, SL::NET::PortNumber(3000), SL::NET::Address_Family::IPV6)) {
        auto lsock = SL::NET::CreateSocket(iocontext);
        if (lsock->bind(address)) {
            std::cout << "Listener bind success " << std::endl;
            if (lsock->listen(5)) {
                std::cout << "listen success " << std::endl;
                listensocket = lsock;
            }
            else {
                std::cout << "Listen failed " << std::endl;
            }
        }
    }
    auto listener = SL::NET::CreateListener(iocontext, std::move(listensocket));
    auto newsocket = SL::NET::CreateSocket(iocontext);
    listener->async_accept(newsocket, [newsocket](bool connectsuccess) {
        if (connectsuccess) {
            std::cout << "Listen Socket Acceot Success " << std::endl;
            if (auto peerinfo = newsocket->getsockname(); peerinfo.has_value()) {
                std::cout << "Address: '" << peerinfo->get_Host() << "' Port:'" << peerinfo->get_Port() << "' Family:'"
                          << (peerinfo->get_Family() == SL::NET::Address_Family::IPV4 ? "ipv4'\n" : "ipv6'\n");
            }
            echolistenread(newsocket);
        }
        else {
            std::cout << "Listen Socket Acceot Failed " << std::endl;
        }
    });
    auto clientsocket = SL::NET::CreateSocket(iocontext);
    auto addresses = SL::NET::getaddrinfo("::1", SL::NET::PortNumber(3000), SL::NET::Address_Family::IPV6);

    clientsocket->async_connect(iocontext, addresses, [clientsocket](SL::NET::ConnectionAttemptStatus connectstatus, SL::NET::sockaddr &addr) {
        if (connectstatus == SL::NET::ConnectionAttemptStatus::SuccessfullConnect) {
            std::cout << "Client Socket Connected " << std::endl;
            if (auto peerinfo = clientsocket->getpeername(); peerinfo.has_value()) {
                std::cout << "Address: '" << peerinfo->get_Host() << "' Port:'" << peerinfo->get_Port() << "' Family:'"
                          << (peerinfo->get_Family() == SL::NET::Address_Family::IPV4 ? "ipv4'\n" : "ipv6'\n");
            }
            echolistenwrite(clientsocket);
            return SL::NET::ConnectSelection::Selected;
        }
        return SL::NET::ConnectSelection::NotSelected;
    });

    iocontext->run(SL::NET::ThreadCount(1));
    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    std::cout << "Echo per Second " << writeechos / 10 << std::endl;
}
void echoclienttest() {}

int main(int argc, char *argv[])
{
    // the verbode ways to use this library
    echolistenertest();
    echoclienttest();

    return 0;
}
