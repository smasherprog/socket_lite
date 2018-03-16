#pragma once
#include "Socket_Lite.h"
#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace myechomodels {

char writeecho[] = "echo test";
char readecho[] = "echo test";
auto readechos = 0.0;
auto writeechos = 0.0;

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
                writeechos += 1.0;
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
            if (socket && SL::NET::StatusCode::SC_SUCCESS == code) {
                std::make_shared<session>(socket)->do_read();
                self->do_accept();
            }
        });
    }
    void close() { Listener->close(); }
    std::shared_ptr<SL::NET::IListener> Listener;
};

class asioclient {
  public:
    std::vector<SL::NET::sockaddr> Addresses;
    std::shared_ptr<session> socket_;
    asioclient(std::shared_ptr<SL::NET::Context> &io_context, const std::vector<SL::NET::sockaddr> &endpoints) : Addresses(endpoints)
    {
        socket_ = std::make_shared<session>(std::make_shared<SL::NET::Socket>(io_context.get()));
    }
    ~asioclient() {}
    void close() { socket_->socket_->close(); }
    void do_connect()
    {
        if (Addresses.empty())
            return;
        socket_->socket_->connect(Addresses.back(), [this](SL::NET::StatusCode connectstatus) {
            if (connectstatus == SL::NET::StatusCode::SC_SUCCESS) {
                socket_->do_write();
            }
            else {
                Addresses.pop_back();
                do_connect();
            }
        });
    }
};

} // namespace myechomodels
