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
    bool keepgoing = true;
    class session : public std::enable_shared_from_this<session> {
    public:
        session(SL::NET::Socket &socket) : socket_(std::move(socket)) {}
        session(SL::NET::Socket &&socket) : socket_(std::move(socket)) {}

        void do_read()
        {
            auto self(shared_from_this());
            SL::NET::recv(socket_, readbuffer.size(), (unsigned char *)readbuffer.data(), [self](SL::NET::StatusCode code, size_t bytesread) {
                if (code == SL::NET::StatusCode::SC_SUCCESS) {
                    writeechos += 1.0;
                    self->do_read();
                }
            });
        }
        void do_write()
        {
            auto self(shared_from_this());
            SL::NET::send(socket_, writebuffer.size(), (unsigned char *)writebuffer.data(), [self](SL::NET::StatusCode code, size_t bytesread) {
                if (code == SL::NET::StatusCode::SC_SUCCESS) {
                    self->do_write();
                }
            });
        }
        void close() { socket_.close(); }
        SL::NET::Socket socket_;
    };


    class asioclient {
    public:
        asioclient(SL::NET::Context &io_context, const std::vector<SL::NET::sockaddr> &endpoints) : Addresses(endpoints)
        {
            socket_ = std::make_shared<session>(SL::NET::Socket(io_context));
        }
        void close() { socket_->socket_.close(); }
        void do_connect()
        {
            connect(socket_->socket_, Addresses.back(), [this](SL::NET::StatusCode connectstatus) {
                if (connectstatus == SL::NET::StatusCode::SC_SUCCESS) {
                    socket_->do_write();
                }
                else {
                    Addresses.pop_back();
                    do_connect();
                }
            });
        }
        std::vector<SL::NET::sockaddr> Addresses;
        std::shared_ptr<session> socket_;
    };

    void mytransfertest()
    {
        std::cout << "Starting My MB per Second Test" << std::endl;
        auto porttouse = static_cast<unsigned short>(std::rand() % 3000 + 10000);
        writeechos = 0.0;
        writebuffer.resize(1024 * 1024 * 8);
        readbuffer.resize(1024 * 1024 * 8);
        SL::NET::Context iocontext(SL::NET::ThreadCount(1));
        SL::NET::StatusCode ec;
        SL::NET::Acceptor acceptor(SL::NET::PortNumber(porttouse), SL::NET::AddressFamily::IPV4, ec);
        if (ec != SL::NET::StatusCode::SC_SUCCESS) {
            std::cout << "Acceptor failed to create code:" << ec << std::endl;
        } 
        acceptor.set_handler([&](SL::NET::Socket socket) {
            std::make_shared<session>(socket)->do_read();
        });
        SL::NET::Listener Listener(iocontext, std::move(acceptor));

        auto[code, addresses] = SL::NET::getaddrinfo("127.0.0.1", SL::NET::PortNumber(porttouse), SL::NET::AddressFamily::IPV4);
        if (code != SL::NET::StatusCode::SC_SUCCESS) {
            std::cout << "Error code:" << code << std::endl;
        }
        auto c = std::make_shared<asioclient>(iocontext, addresses);
        c->do_connect();
        iocontext.run();
        std::this_thread::sleep_for(10s); // sleep for 10 seconds
        keepgoing = false;
        c->close();
        std::cout << "My MB per Second " << (writeechos / 10) * 8 << std::endl;
 
    }

} // namespace mytransfertest
