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
    session(const std::shared_ptr<SL::NET::ISocket> &socket) : socket_(socket) {}
    void do_read()
    {
        auto self(shared_from_this());
        socket_->recv(sizeof(writeecho), (unsigned char *)writeecho, [self](SL::NET::StatusCode code, size_t bytesread) {
            if (code == SL::NET::StatusCode::SC_SUCCESS) {
                self->do_read();
            }
        });
    }

    void do_write()
    {
        auto self(shared_from_this());
        socket_->send(sizeof(writeecho), (unsigned char *)writeecho, [self](SL::NET::StatusCode code, size_t bytesread) {
            if (code == SL::NET::StatusCode::SC_SUCCESS) {
                writeechos += 1.0;
                self->do_write();
            }
        });
    }
    std::shared_ptr<SL::NET::ISocket> socket_;
};

class asioserver : public std::enable_shared_from_this<asioserver> {
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
            if (auto bret = lsock->bind(address); bret == SL::NET::StatusCode::SC_SUCCESS) {
                if (auto lret = lsock->listen(5); lret == SL::NET::StatusCode::SC_SUCCESS) {
                    listensocket = lsock;
                }
                else {
                    std::cout << "Listen Error code:" << lret << std::endl;
                }
            }
            else {
                std::cout << "Bind Error code:" << bret << std::endl;
            }
        }
        Listener = io_context->CreateListener(std::move(listensocket));
    }
    ~asioserver() { close(); }
    void do_accept() { do_accept(Listener); }
    static void do_accept(const std::shared_ptr<SL::NET::IListener> &listener)
    {
        listener->accept([listener](SL::NET::StatusCode code, const std::shared_ptr<SL::NET::ISocket> &socket) {
            if (socket && SL::NET::StatusCode::SC_SUCCESS == code) {
                auto s = std::make_shared<session>(socket);
                s->do_read();
                s->do_write();
                do_accept(listener);
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
    asioclient(std::shared_ptr<SL::NET::IContext> &io_context, const std::vector<SL::NET::sockaddr> &endpoints) : Addresses(endpoints)
    {
        socket_ = std::make_shared<session>(io_context->CreateSocket());
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
                socket_->do_read();
            }
            else {
                Addresses.pop_back();
                do_connect();
            }
        });
    }
};

} // namespace myechomodels
