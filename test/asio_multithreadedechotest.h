#pragma once

#define ASIO_STANDALONE
#include "asio.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
using namespace std::chrono_literals;

namespace asio_multithreadedechotest {

char writeecho[] = "echo test";
char readecho[] = "echo test";
auto writeechos = 0.0;
bool keepgoing = true;

using asio::ip::tcp;
class session : public std::enable_shared_from_this<session> {
  public:
    session(tcp::socket &&socket, asio::io_context &context) : socket_(std::move(socket)), strand_(context) {}
    session(asio::io_context &context) : socket_(context), strand_(context) {}

    void do_read()
    {
        auto self(shared_from_this());
        strand_.post([self]() {
            asio::async_read(self->socket_, asio::buffer(readecho, sizeof(readecho)), [self](std::error_code ec, std::size_t) {
                if (!ec) {
                    self->do_read();
                }
            });
        });
    }

    void do_write()
    {
        auto self(shared_from_this());
        strand_.post([self]() {
            asio::async_write(self->socket_, asio::buffer(readecho, sizeof(readecho)), [self](std::error_code ec, std::size_t /*length*/) {
                writeechos += 1.0;
                if (!ec) {
                    self->do_write();
                }
            });
        });
    }
    tcp::socket socket_;
    asio::io_service::strand strand_;
};

class asioserver : public std::enable_shared_from_this<asioserver> {
  public:
    asioserver(asio::io_context &io_context, short port) : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)), io_context_(io_context) {}

    void do_accept()
    {
        auto self(shared_from_this());
        acceptor_.async_accept([self](std::error_code ec, tcp::socket socket) {
            if (keepgoing) {
                auto s = std::make_shared<session>(std::move(socket), self->io_context_);
                s->do_read();
                s->do_write();
                self->do_accept();
            }
        });
    }
    tcp::acceptor acceptor_;
    asio::io_context &io_context_;
};

class asioclient {
  public:
    asioclient(asio::io_context &io_context, const tcp::resolver::results_type &endpoints) : socket_(std::make_shared<session>(io_context))
    {
        do_connect(endpoints);
    }
    void do_connect(const tcp::resolver::results_type &endpoints)
    {
        asio::async_connect(socket_->socket_, endpoints, [this](std::error_code ec, tcp::endpoint) {
            if (!ec) {
                socket_->do_write();
                socket_->do_read();
            }
        });
    }
    void close() { socket_->socket_.close(); }
    std::shared_ptr<session> socket_;
};

void asioechotest()
{
    std::cout << "Starting ASIO 4 Threads Echo Test" << std::endl;
    keepgoing = true;
    writeechos = 0;
    auto porttouse = static_cast<unsigned short>(std::rand() % 3000 + 10000);
    asio::io_context iocontext;
    auto s(std::make_shared<asioserver>(iocontext, porttouse));
    s->do_accept();

    tcp::resolver resolver(iocontext);
    auto endpoints = resolver.resolve("127.0.0.1", std::to_string(porttouse));
    auto c = std::make_shared<asioclient>(iocontext, endpoints);
    auto c1 = std::make_shared<asioclient>(iocontext, endpoints);
    auto c2 = std::make_shared<asioclient>(iocontext, endpoints);

    std::thread t([&iocontext]() { iocontext.run(); });
    std::thread t2([&iocontext]() { iocontext.run(); });
    std::thread t3([&iocontext]() { iocontext.run(); });
    std::thread t4([&iocontext]() { iocontext.run(); });

    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    keepgoing = false;

    std::cout << "ASIO 4 Threads Echo per Second " << writeechos / 10 << std::endl;
    iocontext.stop();
    s->acceptor_.cancel();
    s->acceptor_.close();
    c->close();
    c1->close();
    c2->close();
    t.join();
    t2.join();
    t3.join();
    t4.join();
}

} // namespace asio_multithreadedechotest
