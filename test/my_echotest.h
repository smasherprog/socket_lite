#pragma once
#include "Socket_Lite.h"
#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace myechotest {

char writeecho[] = "echo test";
char readecho[] = "echo test";
auto readechos = 0.0;
auto writeechos = 0.0;

class session : public std::enable_shared_from_this<session> {
  public:
    session(const std::shared_ptr<SL::NET::ISocket> &socket) : socket_(socket) {}

    void start() { do_read(); }
    void do_read()
    {
        auto self(shared_from_this());
        socket_->recv(sizeof(writeecho), (unsigned char *)writeecho, [self, this](long long bytesread) {
            if (bytesread == sizeof(writeecho)) {
                do_write();
            }
        });
    }

    void do_write()
    {
        auto self(shared_from_this());
        socket_->async_write(sizeof(writeecho), (unsigned char *)writeecho, [self, this](long long bytesread) {
            if (bytesread == sizeof(writeecho)) {
                do_read();
            }
        });
    }
    std::shared_ptr<SL::NET::ISocket> socket_;
};

class asioserver {
  public:
    asioserver(std::shared_ptr<SL::NET::IIO_Context> &io_context, SL::NET::PortNumber port) : IOContext(io_context)
    {

        std::shared_ptr<SL::NET::ISocket> listensocket;
        for (auto &address : SL::NET::getaddrinfo(nullptr, SL::NET::PortNumber(3000), SL::NET::Address_Family::IPV4)) {
            auto lsock = SL::NET::CreateSocket(IOContext, SL::NET::Address_Family::IPV4);
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
        Listener = SL::NET::CreateListener(io_context, std::move(listensocket));

        do_accept();
    }
    ~asioserver()
    {
        std::cout << "~asioserver " << std::endl;
        close();
    }
    void do_accept()
    {
        std::cout << "do_accept() " << std::endl;
        auto newsocket = SL::NET::CreateSocket(IOContext, SL::NET::Address_Family::IPV4);
        Listener->async_accept(newsocket, [newsocket, this](bool connectsuccess) {
            if (connectsuccess) {
                std::cout << "Listen Socket Accept Success " << std::endl;
                if (auto peerinfo = newsocket->getsockname(); peerinfo.has_value()) {
                    std::cout << "Address: '" << peerinfo->get_Host() << "' Port:'" << peerinfo->get_Port() << "' Family:'"
                              << (peerinfo->get_Family() == SL::NET::Address_Family::IPV4 ? "ipv4'\n" : "ipv6'\n");
                }
                Sockets.push_back(std::make_shared<session>(newsocket));
                Sockets.back()->start();
                do_accept();
            }
            else {
                std::cout << "Listen Socket Acceot Failed " << std::endl;
            }
        });
    }
    void close()
    {
        for (auto &s : Sockets) {
            s->socket_->close();
            Listener->close();
        }
    }
    std::vector<std::shared_ptr<session>> Sockets;
    std::shared_ptr<SL::NET::IIO_Context> &IOContext;
    std::shared_ptr<SL::NET::IListener> Listener;
};

class asioclient : public std::enable_shared_from_this<asioclient> {
  public:
    asioclient(std::shared_ptr<SL::NET::IIO_Context> &io_context, const std::vector<SL::NET::sockaddr> &endpoints)
        : IOcontext(io_context), Addresses(endpoints)
    {
        socket_ = SL::NET::CreateSocket(io_context, SL::NET::Address_Family::IPV4);
        do_connect();
    }
    ~asioclient() { std::cout << "~asioclient " << std::endl; }
    void do_connect()
    {

        socket_->connect(IOcontext, Addresses.back(), [this](SL::NET::ConnectionAttemptStatus connectstatus) {
            if (connectstatus == SL::NET::ConnectionAttemptStatus::SuccessfullConnect) {
                std::cout << "Client Socket Connected " << std::endl;
                if (auto peerinfo = socket_->getpeername(); peerinfo.has_value()) {
                    std::cout << "Address: '" << peerinfo->get_Host() << "' Port:'" << peerinfo->get_Port() << "' Family:'"
                              << (peerinfo->get_Family() == SL::NET::Address_Family::IPV4 ? "ipv4'\n" : "ipv6'\n");
                }
                do_write();
            }
            else {
                do_connect();
            }

        });
    }

    void do_read()
    {
        auto self(shared_from_this());
        socket_->recv(sizeof(writeecho), (unsigned char *)writeecho, [self, this](long long bytesread) {
            if (bytesread == sizeof(writeecho)) {
                do_write();
            }
        });
    }

    void do_write()
    {
        auto self(shared_from_this());
        socket_->async_write(sizeof(writeecho), (unsigned char *)writeecho, [self, this](long long bytesread) {
            if (bytesread == sizeof(writeecho)) {
                writeechos += 1.0;
                do_read();
            }
        });
    }
    std::shared_ptr<SL::NET::IIO_Context> &IOcontext;
    std::vector<SL::NET::sockaddr> Addresses;
    std::shared_ptr<SL::NET::ISocket> socket_;
};

void myechotest()
{
    auto iocontext = SL::NET::CreateIO_Context();
    asioserver s(iocontext, SL::NET::PortNumber(3000));
    auto addresses = SL::NET::getaddrinfo("127.0.0.1", SL::NET::PortNumber(3000), SL::NET::Address_Family::IPV4);
    auto c = std::make_shared<asioclient>(iocontext, addresses);

    iocontext->run(SL::NET::ThreadCount(2));
    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    c.reset();
    s.close();
    std::cout << "My Echo per Second " << writeechos / 10 << std::endl;
}

} // namespace myechotest
