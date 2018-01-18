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
        socket_->recv(writebuffer.size(), (unsigned char *)writebuffer.data(), [self, this](long long bytesread) {
            if (bytesread == writebuffer.size()) {
                do_read();
            }
        });
    }

    std::shared_ptr<SL::NET::ISocket> socket_;
};

class asioserver {
  public:
    asioserver(std::shared_ptr<SL::NET::IIO_Context> &io_context, SL::NET::PortNumber port) : IOContext(io_context)
    {

        std::shared_ptr<SL::NET::ISocket> listensocket;
        for (auto &address : SL::NET::getaddrinfo(nullptr, port, SL::NET::Address_Family::IPV4)) {
            auto lsock = SL::NET::CreateSocket(IOContext, SL::NET::Address_Family::IPV4);
            if (lsock->bind(address)) {
                if (lsock->listen(5)) {
                    listensocket = lsock;
                }
            }
        }
        Listener = SL::NET::CreateListener(io_context, std::move(listensocket));
        do_accept();
    }
    ~asioserver() { close(); }
    void do_accept()
    {
        auto newsocket = SL::NET::CreateSocket(IOContext, SL::NET::Address_Family::IPV4);
        Listener->async_accept(newsocket, [newsocket, this](bool connectsuccess) {
            if (connectsuccess) {
                if (auto peerinfo = newsocket->getsockname(); peerinfo.has_value()) {
                    std::cout << "Address: '" << peerinfo->get_Host() << "' Port:'" << peerinfo->get_Port() << "' Family:'"
                              << (peerinfo->get_Family() == SL::NET::Address_Family::IPV4 ? "ipv4'\n" : "ipv6'\n");
                }
                std::make_shared<session>(newsocket)->start();
                do_accept();
            }
        });
    }
    void close() { Listener->close(); }
    std::shared_ptr<SL::NET::IIO_Context> &IOContext;
    std::shared_ptr<SL::NET::IListener> Listener;
};

class asioclient : public std::enable_shared_from_this<asioclient> {
  public:
    asioclient(std::shared_ptr<SL::NET::IIO_Context> &io_context, const std::vector<SL::NET::sockaddr> &endpoints)
        : IOcontext(io_context), Addresses(endpoints)
    {
        socket_ = SL::NET::CreateSocket(io_context, SL::NET::Address_Family::IPV4);
        do_connect();
    }
    ~asioclient() {}
    void do_connect()
    {
        if (Addresses.empty())
            return;
        socket_->connect(IOcontext, Addresses.back(), [this](SL::NET::ConnectionAttemptStatus connectstatus) {
            if (connectstatus == SL::NET::ConnectionAttemptStatus::SuccessfullConnect) {
                if (auto peerinfo = socket_->getpeername(); peerinfo.has_value()) {
                    std::cout << "Address: '" << peerinfo->get_Host() << "' Port:'" << peerinfo->get_Port() << "' Family:'"
                              << (peerinfo->get_Family() == SL::NET::Address_Family::IPV4 ? "ipv4'\n" : "ipv6'\n");
                }
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
        socket_->send(readbuffer.size(), (unsigned char *)readbuffer.data(), [self, this](long long bytesread) {
            if (bytesread == readbuffer.size()) {
                writeechos += 1.0;
                do_write();
            }
        });
    }
    std::shared_ptr<SL::NET::IIO_Context> &IOcontext;
    std::vector<SL::NET::sockaddr> Addresses;
    std::shared_ptr<SL::NET::ISocket> socket_;
};

void mytransfertest()
{
    writebuffer.resize(1024 * 1024 * 8);
    readbuffer.resize(1024 * 1024 * 8);
    auto iocontext = SL::NET::CreateIO_Context();
    asioserver s(iocontext, SL::NET::PortNumber(3000));
    auto addresses = SL::NET::getaddrinfo("127.0.0.1", SL::NET::PortNumber(3000), SL::NET::Address_Family::IPV4);
    auto c = std::make_shared<asioclient>(iocontext, addresses);

    iocontext->run(SL::NET::ThreadCount(2));
    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    c->socket_->close();
    s.close();
    std::cout << "My MB per Second " << (writeechos / 10) * 8 << std::endl;
}

} // namespace mytransfertest
