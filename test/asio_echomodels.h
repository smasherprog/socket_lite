#pragma once

#define ASIO_STANDALONE
#include "asio.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
using namespace std::chrono_literals;

namespace asiomodels {

char writeecho[] = "echo test";
char readecho[] = "echo test";
auto readechos = 0.0;
auto writeechos = 0.0;
bool keepgoing = true;

using asio::ip::tcp;
class session : public std::enable_shared_from_this<session> {
  public:
    session(tcp::socket socket) : socket_(std::move(socket)) {}
    session(asio::io_context &io_context) : socket_(io_context) {}

    void do_read()
    {
        auto self(shared_from_this());
        asio::async_read(self->socket_, asio::buffer(readecho, sizeof(readecho)), [self](std::error_code ec, std::size_t) {
            if (!ec) {
                self->do_read();
            }
        });
    }

    void do_write()
    {
        auto self(shared_from_this());
        asio::async_write(socket_, asio::buffer(readecho, sizeof(readecho)), [self](std::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                writeechos += 1.0;
                self->do_write();
            }
        });
    }

    tcp::socket socket_;
};

class asioserver : public std::enable_shared_from_this<asioserver> {
  public:
    asioserver(asio::io_context &io_context, short port) : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {}

    void do_accept()
    {
        auto self(shared_from_this());
        acceptor_.async_accept([self](std::error_code ec, tcp::socket socket) {
            if (keepgoing) {
                auto s = std::make_shared<session>(std::move(socket));
                s->do_read();
                s->do_write();
                self->do_accept();
            }
        });
    }

    tcp::acceptor acceptor_;
};

class asioclient {

  public:
    std::shared_ptr<session> socket_;

    asioclient(asio::io_context &io_context) : socket_(std::make_shared<session>(io_context)) {}
    ~asioclient() {}
    void close() { socket_->socket_.close(); }
    void do_connect(const tcp::resolver::results_type &endpoints)
    {
        asio::async_connect(socket_->socket_, endpoints, [this](std::error_code ec, tcp::endpoint) {
            if (!ec) {
                socket_->do_write();
                socket_->do_read();
            }
        });
    }
};

} // namespace asiomodels
