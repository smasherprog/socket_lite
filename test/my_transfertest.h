#pragma once
#include "Socket_Lite.h"
#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace mytransfertest {

std::vector<char> writebuffer;
std::vector<char> readbuffer;
double writeechos = 0.0;
class session : public std::enable_shared_from_this<session> {
  public:
    session(const std::shared_ptr<SL::NET::Socket> &socket) : socket_(socket) {}

    void start() { do_read(); }
    void do_read()
    {
        auto self(shared_from_this());
        socket_->recv(writebuffer.size(), (unsigned char *)writebuffer.data(), [self](SL::NET::StatusCode code, size_t bytesread) {
            if (code == SL::NET::StatusCode::SC_SUCCESS) {
                self->do_read();
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
            if (socket && code == SL::NET::StatusCode::SC_SUCCESS) {
                std::make_shared<session>(socket)->start();
                self->do_accept();
            }
        });
    }
    void close() { Listener->close(); }
    std::shared_ptr<SL::NET::IListener> Listener;
};

class asioclient : public std::enable_shared_from_this<asioclient> {
  public:
    asioclient(std::shared_ptr<SL::NET::Context> &io_context, const std::vector<SL::NET::sockaddr> &endpoints) : Addresses(endpoints)
    {
        socket_ = std::make_shared<SL::NET::Socket>(io_context.get());
    }
    ~asioclient() {}
    void do_connect()
    {
        auto self(shared_from_this());
        socket_->connect(Addresses.back(), [self](SL::NET::StatusCode connectstatus) {
            if (connectstatus == SL::NET::StatusCode::SC_SUCCESS) {
                self->do_write();
            }
            else {
                self->Addresses.pop_back();
                self->do_connect();
            }
        });
    }
    void do_write()
    {
        auto self(shared_from_this());
        socket_->send(readbuffer.size(), (unsigned char *)readbuffer.data(), [self](SL::NET::StatusCode code, size_t bytesread) {
            if (code == SL::NET::StatusCode::SC_SUCCESS) {
                writeechos += 1.0;
                self->do_write();
            }
        });
    }
    std::vector<SL::NET::sockaddr> Addresses;
    std::shared_ptr<SL::NET::Socket> socket_;
};

void mytransfertest()
{
    std::cout << "Starting My MB per Second Test" << std::endl;
    auto porttouse = static_cast<unsigned short>(std::rand() % 3000 + 10000);
    writeechos = 0.0;
    writebuffer.resize(1024 * 1024 * 8);
    readbuffer.resize(1024 * 1024 * 8);
    auto iocontext = std::make_shared<SL::NET::Context>(SL::NET::ThreadCount(1));
    auto s(std::make_shared<asioserver>(iocontext, SL::NET::PortNumber(porttouse)));
    s->do_accept();
    auto[code, addresses] = SL::NET::getaddrinfo("127.0.0.1", SL::NET::PortNumber(porttouse), SL::NET::AddressFamily::IPV4);
    if (code != SL::NET::StatusCode::SC_SUCCESS) {
        std::cout << "Error code:" << code << std::endl;
    }
    auto c = std::make_shared<asioclient>(iocontext, addresses);
    c->do_connect();
    iocontext->run();
    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    c->socket_->close();
    s->close();
    std::cout << "My MB per Second " << (writeechos / 10) * 8 << std::endl;
}

} // namespace mytransfertest
