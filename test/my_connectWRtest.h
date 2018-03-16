#pragma once
#include "Socket_Lite.h"
#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <ctime>

using namespace std::chrono_literals;

namespace myconnectwrtest {

char writeecho[] = "echo test";
char readecho[] = "echo test";
auto writeechos = 0.0;
bool keepgoing = true;
class session : public std::enable_shared_from_this<session> {
  public:
    session(const std::shared_ptr<SL::NET::Socket> &socket) : socket_(socket) {}
    void do_read()
    {
        auto self(shared_from_this());
        socket_->recv(sizeof(writeecho), (unsigned char *)writeecho, [self](SL::NET::StatusCode code, size_t bytesread) {
            if (code == SL::NET::StatusCode::SC_SUCCESS) {
                self->do_write();
            }
        });
    }

    void do_write()
    {
        auto self(shared_from_this());
        socket_->send(sizeof(writeecho), (unsigned char *)writeecho, [self](SL::NET::StatusCode code, size_t bytesread) {
            if (code == SL::NET::StatusCode::SC_SUCCESS) {
            }
        });
    }
    std::shared_ptr<SL::NET::Socket> socket_;
};

class asioserver : public std::enable_shared_from_this<asioserver> {
  public:
    asioserver(std::shared_ptr<SL::NET::Context> &io_context, SL::NET::PortNumber port)
    {

        std::shared_ptr<SL::NET::Socket> listensocket;
        auto[code, addresses] = SL::NET::getaddrinfo(nullptr, port, SL::NET::AddressFamily::IPV4);
        if (code != SL::NET::StatusCode::SC_SUCCESS) {
            std::cout << "Error code:" << code << std::endl;
        }
        for (auto &address : addresses) {
            auto lsock = std::make_shared<SL::NET::Socket>(io_context.get());
            if (lsock->bind(address) == SL::NET::StatusCode::SC_SUCCESS) {
                if (lsock->listen(5) == SL::NET::StatusCode::SC_SUCCESS) {
                    listensocket = lsock;
                }
            }
        }
        listensocket->setsockopt<SL::NET::SocketOptions::O_REUSEADDR>(SL::NET::SockOptStatus::ENABLED);
        Listener = io_context->CreateListener(std::move(listensocket));
    }
    ~asioserver() { close(); }
    void do_accept()
    {
        auto self(shared_from_this());
        Listener->accept([self](SL::NET::StatusCode code, const std::shared_ptr<SL::NET::Socket> &socket) {
            if (socket && SL::NET::StatusCode::SC_SUCCESS == code) {
                std::make_shared<session>(socket)->do_read();
                self->do_accept();
            }
        });
    }
    void close() { Listener->close(); }
    std::shared_ptr<SL::NET::IListener> Listener;
};

void do_write(const std::shared_ptr<SL::NET::Socket> &s)
{
    s->send(sizeof(writeecho), (unsigned char *)writeecho, [s](SL::NET::StatusCode code, size_t bytesread) {
        if (code == SL::NET::StatusCode::SC_SUCCESS) {
            writeechos += 1.0;
        }
    });
}
void do_read(const std::shared_ptr<SL::NET::Socket> &s)
{
    s->recv(sizeof(writeecho), (unsigned char *)writeecho, [s](SL::NET::StatusCode code, size_t bytesread) {
        if (code == SL::NET::StatusCode::SC_SUCCESS) {
            do_write(s);
        }
    });
}
std::vector<SL::NET::sockaddr> Addresses;
void do_connect(const std::shared_ptr<SL::NET::Context> &io_context)
{
    auto socket_ = std::make_shared<SL::NET::Socket>(io_context.get());
    socket_->connect(Addresses.back(), [socket_, io_context](SL::NET::StatusCode connectstatus) {
        if (connectstatus == SL::NET::StatusCode::SC_SUCCESS) {
            do_write(socket_);
        }
        else if (keepgoing) {
            do_connect(io_context);
        }
    });
}

void myconnectiontest()
{
    std::cout << "Starting My Connect Write Read per Second Test" << std::endl;
    writeechos = 0.0;
    auto iocontext = std::make_shared<SL::NET::Context>(SL::NET::ThreadCount(1));
    auto porttouse = static_cast<unsigned short>(std::rand() % 3000 + 10000);
    auto s(std::make_shared<asioserver>(iocontext, SL::NET::PortNumber(porttouse)));
    s->do_accept();
    auto[code, addrs] = SL::NET::getaddrinfo("127.0.0.1", SL::NET::PortNumber(porttouse), SL::NET::AddressFamily::IPV4);
    if (code != SL::NET::StatusCode::SC_SUCCESS) {
        std::cout << "Error code:" << code << std::endl;
    }
    Addresses = addrs;
    iocontext->run();
    do_connect(iocontext);
    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    keepgoing = false;
    std::cout << "My Connections Write Read per Second " << writeechos / 10 << std::endl;
    s->close();
}
} // namespace myconnectwrtest
