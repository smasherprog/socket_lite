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
void do_read(std::shared_ptr<SL::NET::Socket> socket)
{
    socket->recv_async(readbuffer.size(), (unsigned char *)readbuffer.data(), [socket](SL::NET::StatusCode code, size_t) {
        if (code == SL::NET::StatusCode::SC_SUCCESS) {
            writeechos += 1.0;
            do_read(socket);
        }
    });
}
void do_write(std::shared_ptr<SL::NET::Socket> socket)
{
    socket->send_async(writebuffer.size(), (unsigned char *)writebuffer.data(), [socket](SL::NET::StatusCode code, size_t) {
        if (code == SL::NET::StatusCode::SC_SUCCESS) {
            do_write(socket);
        }
    });
}
class asioclient {
  public:
    asioclient(SL::NET::Context &io_context, const std::vector<SL::NET::SocketAddress> &endpoints) : Addresses(endpoints)
    {
        socket_ = std::make_shared<SL::NET::Socket>(io_context);
    }
    void close() { socket_->close(); }
    void do_connect()
    {
        SL::NET::connect_async(*socket_, Addresses.back(), [&](SL::NET::StatusCode connectstatus) {
            if (connectstatus == SL::NET::StatusCode::SC_SUCCESS) {
                do_write(socket_);
            }
            else {
                Addresses.pop_back();
                do_connect();
            }
        });
    }
    std::vector<SL::NET::SocketAddress> Addresses;
    std::shared_ptr<SL::NET::Socket> socket_;
};

void mytransfertest()
{
    std::cout << "Starting My MB per Second Test" << std::endl;
    auto porttouse = static_cast<unsigned short>(std::rand() % 3000 + 10000);
    writeechos = 0.0;
    writebuffer.resize(1024 * 1024 * 8);
    readbuffer.resize(1024 * 1024 * 8);
    SL::NET::Context iocontext(SL::NET::ThreadCount(1));
    SL::NET::Acceptor a;
    a.AcceptSocket = myechomodels::listengetaddrinfo(nullptr, SL::NET::PortNumber(porttouse), SL::NET::AddressFamily::IPV4);
    a.AcceptHandler = [](SL::NET::Socket socket) { 
        //do_read(socket); 
    };
    SL::NET::Listener Listener(iocontext, std::move(a));
    std::vector<SL::NET::SocketAddress> addresses;
    addresses = SL::NET::getaddrinfo("127.0.0.1", SL::NET::PortNumber(porttouse));

    auto c = std::make_shared<asioclient>(iocontext, addresses);
    c->do_connect();
    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    keepgoing = false;
    c->close();
    std::cout << "My MB per Second " << (writeechos / 10) * 8 << std::endl;
}

} // namespace mytransfertest
