#pragma once

#define ASIO_STANDALONE
#include "asio.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
using namespace std::chrono_literals;

namespace asiotransfertest {

std::vector<char> writebuffer;
std::vector<char> readbuffer;
double writeechos = 0.0;

using asio::ip::tcp;
class session : public std::enable_shared_from_this<session> {
  public:
    session(tcp::socket socket) : socket_(std::move(socket)) {}

    void start() { do_read(); }

  private:
    void do_read()
    {
        auto self(shared_from_this());
        asio::async_read(self->socket_, asio::buffer(writebuffer.data(), writebuffer.size()), [self](std::error_code ec, std::size_t) {
            if (!ec) {
                self->do_read();
            }
        });
    }

    tcp::socket socket_;
};

class asioserver {
  public:
    asioserver(asio::io_context &io_context, short port) : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) { do_accept(); }

    void do_accept()
    {
        acceptor_.async_accept([this](std::error_code ec, tcp::socket socket) {
            if (!ec) {
                std::make_shared<session>(std::move(socket))->start();
            }

            //  do_accept();
        });
    }

    tcp::acceptor acceptor_;
};

class asioclient : public std::enable_shared_from_this<asioclient> {
  public:
    asioclient(asio::io_context &io_context, const tcp::resolver::results_type &endpoints) : socket_(io_context) { do_connect(endpoints); }
    void do_connect(const tcp::resolver::results_type &endpoints)
    {
        asio::async_connect(socket_, endpoints, [this](std::error_code ec, tcp::endpoint) {
            if (!ec) {
                do_write();
            }
        });
    }

    void do_write()
    {
        auto self(shared_from_this());
        asio::async_write(socket_, asio::buffer(readbuffer.data(), readbuffer.size()), [this, self](std::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                writeechos += 1.0;
                do_write();
            }
        });
    }

    tcp::socket socket_;
};

void asiotransfertest()
{
    writeechos = 0.0;
    auto porttouse = static_cast<unsigned short>(std::rand() % 3000 + 10000);
    writebuffer.resize(1024 * 1024 * 8);
    readbuffer.resize(1024 * 1024 * 8);
    asio::io_context iocontext;
    asioserver s(iocontext, porttouse);
    tcp::resolver resolver(iocontext);
    auto endpoints = resolver.resolve("127.0.0.1", std::to_string(porttouse));
    auto c = std::make_shared<asioclient>(iocontext, endpoints);

    std::thread t([&iocontext]() { iocontext.run(); });

    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    std::cout << "ASIO MB per Second " << (writeechos / 10) * 8 << std::endl;
    iocontext.stop();
    s.acceptor_.cancel();
    s.acceptor_.close();
    c->socket_.close();
    t.join();
}

} // namespace asiotransfertest
