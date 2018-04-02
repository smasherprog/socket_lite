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
bool keepgoing = true;

class session : public std::enable_shared_from_this<session> {
  public:
    session(SL::NET::Socket &socket) : socket_(std::move(socket)) {}
    session(SL::NET::Socket &&socket) : socket_(std::move(socket)) {}
    void do_read()
    {
        auto self(shared_from_this());
        SL::NET::recv(socket_, sizeof(readecho), (unsigned char *)readecho, [self](SL::NET::StatusCode code, size_t bytesread) {
            if (code == SL::NET::StatusCode::SC_SUCCESS) {
                self->do_read();
            }
        });
    }

    void do_write()
    {
        auto self(shared_from_this());
        SL::NET::send(socket_, sizeof(writeecho), (unsigned char *)writeecho, [self](SL::NET::StatusCode code, size_t bytesread) {
            if (code == SL::NET::StatusCode::SC_SUCCESS) {
                writeechos += 1.0;
                self->do_write();
            }
        });
    }
    SL::NET::Socket socket_;
};

class asioserver : public std::enable_shared_from_this<asioserver> {
  public:
    asioserver(SL::NET::Context &io_context, SL::NET::PortNumber port) : Listener(io_context, port, SL::NET::AddressFamily::IPV4, ec)
    {
        if (ec != SL::NET::StatusCode::SC_SUCCESS) {
            std::cout << "Listener failed to create code:" << ec << std::endl;
        }
    }
    void do_accept()
    {
        auto self(shared_from_this());
        Listener.accept([self](SL::NET::StatusCode code, SL::NET::Socket socket) {
            if (keepgoing) {
                auto s = std::make_shared<session>(socket);
                s->do_read();
                s->do_write();
                self->do_accept();
            }
        });
    }
    void close() { Listener.close(); }
    SL::NET::StatusCode ec;
    SL::NET::Listener Listener;
};
class asioclient {
  public:
    std::vector<SL::NET::sockaddr> Addresses;
    std::shared_ptr<session> socket_;
    asioclient(SL::NET::Context &io_context, const std::vector<SL::NET::sockaddr> &endpoints) : Addresses(endpoints)
    {
        socket_ = std::make_shared<session>(SL::NET::Socket(io_context));
    }
    void close() { socket_->socket_.close(); }

    void do_connect()
    {
        if (Addresses.empty())
            return;
        connect(socket_->socket_, Addresses.back(), [this](SL::NET::StatusCode connectstatus) {
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
