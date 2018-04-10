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
    void close() { socket_.close(); }
    SL::NET::Socket socket_;
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
        SL::NET::connect(socket_->socket_, Addresses.back(), [this](SL::NET::StatusCode connectstatus) {
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
