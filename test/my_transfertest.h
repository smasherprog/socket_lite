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
    session(const std::shared_ptr<SL::NET::ISocket> &socket) : socket_(socket) {}

    void start() { do_read(); }
    void do_read()
    {
        auto self(shared_from_this());
        socket_->recv(writebuffer.size(), (unsigned char *)writebuffer.data(), [self, this](SL::NET::StatusCode code, size_t bytesread) {
            if (bytesread == writebuffer.size() && code == SL::NET::StatusCode::SC_SUCCESS) {
                do_read();
            }
        });
    }

    std::shared_ptr<SL::NET::ISocket> socket_;
};

class asioserver {
  public:
    asioserver(std::shared_ptr<SL::NET::IContext> &io_context, SL::NET::PortNumber port)
    {

        std::shared_ptr<SL::NET::ISocket> listensocket;
        auto[code, addresses] = SL::NET::getaddrinfo(nullptr, port, SL::NET::AddressFamily::IPV4);
        if (code != SL::NET::StatusCode::SC_SUCCESS) {
            std::cout << "Error code:" << code << std::endl;
        }
        for (auto &address : addresses) {
            auto lsock = io_context->CreateSocket();
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
            if (socket && code == SL::NET::StatusCode::SC_SUCCESS) {
                std::make_shared<session>(socket)->start();
                do_accept();
            }
        });
    }
    void close() { Listener->close(); }
    std::shared_ptr<SL::NET::IListener> Listener;
};

class asioclient : public std::enable_shared_from_this<asioclient> {
  public:
    asioclient(std::shared_ptr<SL::NET::IContext> &io_context, const std::vector<SL::NET::sockaddr> &endpoints) : Addresses(endpoints)
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
    void do_write()
    {
        auto self(shared_from_this());
        socket_->send(readbuffer.size(), (unsigned char *)readbuffer.data(), [self, this](SL::NET::StatusCode code, size_t bytesread) {
            if (bytesread == readbuffer.size() && code == SL::NET::StatusCode::SC_SUCCESS) {
                writeechos += 1.0;
                do_write();
            }
        });
    }
    std::vector<SL::NET::sockaddr> Addresses;
    std::shared_ptr<SL::NET::ISocket> socket_;
};

void mytransfertest()
{
    auto porttouse = static_cast<unsigned short>(std::rand() % 3000 + 10000);
    writeechos = 0.0;
    writebuffer.resize(1024 * 1024 * 8);
    readbuffer.resize(1024 * 1024 * 8);
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
    std::cout << "My MB per Second " << (writeechos / 10) * 8 << std::endl;
}

} // namespace mytransfertest
