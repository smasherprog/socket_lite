#pragma once
#include "Socket_Lite.h"
#include "my_echomodels.h"
#include <chrono>
#include <iostream>
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
    session(SL::NET::Socket_impl socket) : socket_(std::move(socket)) {}

    void do_read()
    {
        auto self(shared_from_this());
        socket_.recv_async(readbuffer.size(), (unsigned char *)readbuffer.data(), [self](SL::NET::StatusCode code) {
            if (code == SL::NET::StatusCode::SC_SUCCESS) {
                self->do_read();
            }
        });
    }
    void do_write()
    {
        auto self(shared_from_this());
        socket_.send_async(writebuffer.size(), (unsigned char *)writebuffer.data(), [self](SL::NET::StatusCode code) {
            if (code == SL::NET::StatusCode::SC_SUCCESS) {
                writeechos += 1.0;
                self->do_write();
            }
        });
    }
    SL::NET::Socket_impl socket_;
};

template <class CONTEXTYPE> class asioclient {
  public:
    asioclient(CONTEXTYPE &io_context, const std::vector<SL::NET::SocketAddress> &endpoints) : Addresses(endpoints)
    {
        socket_ = std::make_shared<session>(io_context);
    }
    void do_connect()
    {
        auto self(socket_);
        SL::NET::connect_async(socket_->socket_, Addresses.back(), [self](SL::NET::StatusCode connectstatus) {
            if (connectstatus == SL::NET::StatusCode::SC_SUCCESS) {
                self->do_write();
            }
        });
    }
    std::vector<SL::NET::SocketAddress> Addresses;
    void close() { socket_->socket_.close(); }
    std::shared_ptr<session> socket_;
};

void mytransfertest()
{
    std::cout << "Starting My MB per Second Test" << std::endl;
    auto porttouse = static_cast<unsigned short>(std::rand() % 3000 + 10000);
    writeechos = 0.0;
    writebuffer.resize(1024 * 1024 * 8);
    readbuffer.resize(1024 * 1024 * 8);
    auto listencallback([](auto socket) { std::make_shared<session>(std::move(socket))->do_read(); });
    SL::NET::Context iocontext(SL::NET::ThreadCount(1));
    SL::NET::Listener listener(iocontext, myechomodels::listengetaddrinfo(nullptr, SL::NET::PortNumber(porttouse), SL::NET::AddressFamily::IPV4),
                               listencallback);

    iocontext.start();
    listener.start();
    asioclient c(iocontext, SL::NET::getaddrinfo("127.0.0.1", SL::NET::PortNumber(porttouse)));
    c.do_connect();
    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    keepgoing = false;
    listener.stop();
    iocontext.stop();
    std::cout << "My MB per Second " << (writeechos / 10) * 8 << std::endl;
}

} // namespace mytransfertest
