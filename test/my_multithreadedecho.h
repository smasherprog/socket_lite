#pragma once
#include "Socket_Lite.h"
#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace mymultithreadedechotest {

char writeecho[] = "echo test";
char readecho[] = "echo test";

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
        socket_->send(sizeof(writeecho), (unsigned char *)writeecho, [self, this](long long bytesread) {
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
        for (auto &address : SL::NET::getaddrinfo(nullptr, port, SL::NET::Address_Family::IPV4)) {
            auto lsock = IOContext->CreateSocket();
            if (lsock->bind(address)) {
                if (lsock->listen(5)) {
                    listensocket = lsock;
                }
            }
        }
        listensocket->setsockopt<SL::NET::Socket_Options::O_REUSEADDR>(SL::NET::SockOptStatus::ENABLED);
        Listener = SL::NET::CreateListener(io_context, std::move(listensocket));
        do_accept();
    }
    ~asioserver() { close(); }
    void do_accept()
    {
        Listener->async_accept([this](const std::shared_ptr<SL::NET::ISocket> &socket) {
            if (socket) {
                std::make_shared<session>(socket)->start();
                do_accept();
            }
        });
    }
    void close() { Listener->close(); }
    std::shared_ptr<SL::NET::IIO_Context> &IOContext;
    std::shared_ptr<SL::NET::IListener> Listener;
};

class asioclient : public std::enable_shared_from_this<asioclient> {
  public:
    asioclient(std::shared_ptr<SL::NET::IIO_Context> &io_context, const std::vector<SL::NET::sockaddr> &endpoints)
        : IOcontext(io_context), Addresses(endpoints)
    {
        socket_ = io_context->CreateSocket();
        do_connect();
    }
    ~asioclient() {}
    void do_connect()
    {
        if (Addresses.empty())
            return;
        socket_->connect(IOcontext, Addresses.back(), [this](SL::NET::ConnectionAttemptStatus connectstatus) {
            if (connectstatus == SL::NET::ConnectionAttemptStatus::SuccessfullConnect) {

                do_write();
            }
            else {
                Addresses.pop_back();
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
        socket_->send(sizeof(writeecho), (unsigned char *)writeecho, [self, this](long long bytesread) {
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
    auto porttouse = static_cast<unsigned short>(std::rand() % 3000 + 10000);
    auto iocontext = SL::NET::CreateIO_Context();
    asioserver s(iocontext, SL::NET::PortNumber(porttouse));
    auto addresses = SL::NET::getaddrinfo("127.0.0.1", SL::NET::PortNumber(porttouse), SL::NET::Address_Family::IPV4);
    auto c = std::make_shared<asioclient>(iocontext, addresses);
    auto c1 = std::make_shared<asioclient>(iocontext, addresses);
    auto c2 = std::make_shared<asioclient>(iocontext, addresses);

    iocontext->run(SL::NET::ThreadCount(4));
    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    c->socket_->close();
    c1->socket_->close();
    c2->socket_->close();
    s.close();
    std::cout << "My 4 thread Echos per Second " << writeechos / 10 << std::endl;
}

} // namespace mymultithreadedechotest
