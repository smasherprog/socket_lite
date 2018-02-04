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
        socket_->recv(sizeof(writeecho), (unsigned char *)writeecho, [self, this](SL::NET::StatusCode code, size_t bytesread) {
            if (bytesread == sizeof(writeecho) && code == SL::NET::StatusCode::SC_SUCCESS) {
                do_write();
            }
        });
    }

    void do_write()
    {
        auto self(shared_from_this());
        socket_->send(sizeof(writeecho), (unsigned char *)writeecho, [self, this](SL::NET::StatusCode code, size_t bytesread) {
            if (bytesread == sizeof(writeecho) && code == SL::NET::StatusCode::SC_SUCCESS) {
                do_read();
            }
        });
    }
    std::shared_ptr<SL::NET::ISocket> socket_;
};

class asioserver {
  public:
    asioserver(std::shared_ptr<SL::NET::IContext> &io_context, SL::NET::PortNumber port) : IOContext(io_context)
    {

        std::shared_ptr<SL::NET::ISocket> listensocket;
        auto[code, addresses] = SL::NET::getaddrinfo(nullptr, port, SL::NET::AddressFamily::IPV4);
        if (code != SL::NET::StatusCode::SC_SUCCESS) {
            std::cout << "Error code:" << code << std::endl;
        }
        for (auto &address : addresses) {
            auto lsock = IOContext->CreateSocket();
            if (lsock->bind(address) == SL::NET::StatusCode::SC_SUCCESS) {
                if (lsock->listen(5) == SL::NET::StatusCode::SC_SUCCESS) {
                    listensocket = lsock;
                }
            }
        }
        listensocket->setsockopt<SL::NET::SocketOptions::O_REUSEADDR>(SL::NET::SockOptStatus::ENABLED);
        Listener = io_context->CreateListener(std::move(listensocket));
        do_accept();
    }
    ~asioserver() { close(); }
    void do_accept()
    {

        Listener->async_accept([this](SL::NET::StatusCode code, const std::shared_ptr<SL::NET::ISocket> &socket) {
            if (socket && SL::NET::StatusCode::SC_SUCCESS == code) {
                std::make_shared<session>(socket)->start();
                do_accept();
            }
        });
    }
    void close() { Listener->close(); }
    std::shared_ptr<SL::NET::IContext> &IOContext;
    std::shared_ptr<SL::NET::IListener> Listener;
};

class asioclient : public std::enable_shared_from_this<asioclient> {
  public:
    asioclient(std::shared_ptr<SL::NET::IContext> &io_context, const std::vector<SL::NET::sockaddr> &endpoints)
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
        socket_->connect(Addresses.back(), [this](SL::NET::StatusCode connectstatus) {
            if (connectstatus == SL::NET::StatusCode::SC_SUCCESS) {

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
        socket_->recv(sizeof(writeecho), (unsigned char *)writeecho, [self, this](SL::NET::StatusCode code, size_t bytesread) {
            if (bytesread == sizeof(writeecho) && code == SL::NET::StatusCode::SC_SUCCESS) {
                do_write();
            }
        });
    }

    void do_write()
    {
        auto self(shared_from_this());
        socket_->send(sizeof(writeecho), (unsigned char *)writeecho, [self, this](SL::NET::StatusCode code, size_t bytesread) {
            if (bytesread == sizeof(writeecho) && code == SL::NET::StatusCode::SC_SUCCESS) {
                writeechos += 1.0;
                do_read();
            }
        });
    }
    std::shared_ptr<SL::NET::IContext> &IOcontext;
    std::vector<SL::NET::sockaddr> Addresses;
    std::shared_ptr<SL::NET::ISocket> socket_;
};

void myechotest()
{
    auto porttouse = static_cast<unsigned short>(std::rand() % 3000 + 10000);
    auto iocontext = SL::NET::CreateContext();
    asioserver s(iocontext, SL::NET::PortNumber(porttouse));
    auto[code, addresses] = SL::NET::getaddrinfo("127.0.0.1", SL::NET::PortNumber(porttouse), SL::NET::AddressFamily::IPV4);
    if (code != SL::NET::StatusCode::SC_SUCCESS) {
        std::cout << "Error code:" << code << std::endl;
    }
    auto c = std::make_shared<asioclient>(iocontext, addresses);

    iocontext->run(SL::NET::ThreadCount(2));
    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    c->socket_->close();
    s.close();
    std::cout << "My Echo per Second " << writeechos / 10 << std::endl;
}

} // namespace myechotest
