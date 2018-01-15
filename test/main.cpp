#include "Socket_Lite.h"
#define ASIO_STANDALONE
#include "asio.hpp"
#include "async_tcp_echo_server.h"
#include <assert.h>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <utility>

using namespace std::chrono_literals;
char writeecho[] = "echo test";
char readecho[] = "echo test";
auto readechos = 0.0;
auto writeechos = 0.0;

void echolistenread(const std::shared_ptr<SL::NET::ISocket> &socket)
{
    // std::cout << "echolistenread " << std::endl;
    socket->recv(sizeof(readecho), (unsigned char *)readecho, [socket](long long bytesread) {
        if (bytesread == sizeof(readecho)) {
            // std::cout << "Listen echo received " << std::endl;
            socket->async_write(sizeof(readecho), (unsigned char *)readecho, [socket](long long bytesread) {
                if (bytesread == sizeof(readecho)) {
                    //   std::cout << "Listen echo responce sent  " << std::endl;
                    readechos += 1.0;
                    echolistenread(socket);
                }
                else {
                    std::cout << "Listen read Disconnected r" << std::endl;
                }
            });
        }
        else {
            std::cout << "Listen read Disconnected r" << std::endl;
        }
    });
}

void echolistenwrite(const std::shared_ptr<SL::NET::ISocket> &socket)
{
    //   std::cout << "echolistenwrite " << std::endl;
    socket->async_write(sizeof(writeecho), (unsigned char *)writeecho, [socket](long long bytesread) {
        if (bytesread == sizeof(writeecho)) {
            //   std::cout << "Listen echo sent " << std::endl;
            socket->recv(sizeof(writeecho), (unsigned char *)writeecho, [socket](long long bytesread) {
                if (bytesread == sizeof(writeecho)) {
                    //  std::cout << "Listen echo received " << std::endl;
                    writeechos += 1.0;
                    echolistenwrite(socket);
                }
                else {
                    std::cout << "Listen write Disconnected r" << std::endl;
                }
            });
        }
        else {
            std::cout << "Listen write Disconnected w " << std::endl;
        }
    });
}
void tryconnect(const std::shared_ptr<SL::NET::ISocket> &socket, std::vector<SL::NET::sockaddr> &addresses)
{
    if (addresses.empty()) {
        std::cout << "Failed to connect, no more addresses " << std::endl;
    }
    socket->connect(addresses.back(), [socket, addresses](SL::NET::ConnectionAttemptStatus connectstatus) {
        if (connectstatus == SL::NET::ConnectionAttemptStatus::SuccessfullConnect) {
            std::cout << "Client Socket Connected " << std::endl;
            if (auto peerinfo = socket->getpeername(); peerinfo.has_value()) {
                std::cout << "Address: '" << peerinfo->get_Host() << "' Port:'" << peerinfo->get_Port() << "' Family:'"
                          << (peerinfo->get_Family() == SL::NET::Address_Family::IPV4 ? "ipv4'\n" : "ipv6'\n");
            }
            echolistenwrite(socket);
        }
        else {
            auto tmpaddr = addresses;
            tmpaddr.pop_back();
            tryconnect(socket, tmpaddr);
        }

    });
}
void myechotest()
{
    auto iocontext = SL::NET::CreateIO_Context();
    std::shared_ptr<SL::NET::ISocket> listensocket;
    for (auto &address : SL::NET::getaddrinfo(nullptr, SL::NET::PortNumber(3000), SL::NET::Address_Family::IPV6)) {
        auto lsock = SL::NET::CreateSocket(iocontext, SL::NET::Address_Family::IPV6);
        if (lsock->bind(address)) {
            std::cout << "Listener bind success " << std::endl;
            if (lsock->listen(5)) {
                std::cout << "listen success " << std::endl;
                listensocket = lsock;
            }
            else {
                std::cout << "Listen failed " << std::endl;
            }
        }
    }
    auto listener = SL::NET::CreateListener(iocontext, std::move(listensocket));
    auto newsocket = SL::NET::CreateSocket(iocontext, SL::NET::Address_Family::IPV6);
    listener->async_accept(newsocket, [newsocket](bool connectsuccess) {
        if (connectsuccess) {
            std::cout << "Listen Socket Accept Success " << std::endl;
            if (auto peerinfo = newsocket->getsockname(); peerinfo.has_value()) {
                std::cout << "Address: '" << peerinfo->get_Host() << "' Port:'" << peerinfo->get_Port() << "' Family:'"
                          << (peerinfo->get_Family() == SL::NET::Address_Family::IPV4 ? "ipv4'\n" : "ipv6'\n");
            }
            echolistenread(newsocket);
        }
        else {
            std::cout << "Listen Socket Acceot Failed " << std::endl;
        }
    });
    auto clientsocket = SL::NET::CreateSocket(iocontext, SL::NET::Address_Family::IPV6);
    auto addresses = SL::NET::getaddrinfo("::1", SL::NET::PortNumber(3000), SL::NET::Address_Family::IPV6);
    tryconnect(clientsocket, addresses);

    iocontext->run(SL::NET::ThreadCount(2));
    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    std::cout << "MY Echo per Second " << writeechos / 10 << std::endl;
    newsocket->close();
    clientsocket->close();
}

using asio::ip::tcp;
class session : public std::enable_shared_from_this<session> {
  public:
    session(tcp::socket socket) : socket_(std::move(socket)) {}

    void start() { do_read(); }

  private:
    void do_read()
    {
        auto self(shared_from_this());
        socket_.async_read_some(asio::buffer(readecho, sizeof(readecho)), [this, self](std::error_code ec, std::size_t length) {
            if (!ec) {
                do_write(length);
            }
        });
    }

    void do_write(std::size_t length)
    {
        auto self(shared_from_this());
        asio::async_write(socket_, asio::buffer(readecho, sizeof(readecho)), [this, self](std::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                do_read();
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

            do_accept();
        });
    }

    tcp::acceptor acceptor_;
};

class asioclient : public std::enable_shared_from_this<asioclient> {
  public:
    asioclient(asio::io_context &io_context, const tcp::resolver::results_type &endpoints) : io_context_(io_context), socket_(io_context)
    {
        do_connect(endpoints);
    }
    void do_connect(const tcp::resolver::results_type &endpoints)
    {
        asio::async_connect(socket_, endpoints, [this](std::error_code ec, tcp::endpoint) {
            if (!ec) {
                do_write();
            }
        });
    }

    void do_read()
    {
        auto self(shared_from_this());
        socket_.async_read_some(asio::buffer(writeecho, sizeof(writeecho)), [this, self](std::error_code ec, std::size_t length) {
            if (!ec) {
                do_write();
            }
        });
    }

    void do_write()
    {
        auto self(shared_from_this());
        asio::async_write(socket_, asio::buffer(writeecho, sizeof(writeecho)), [this, self](std::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                writeechos += 1.0;
                do_read();
            }
        });
    }

    asio::io_context &io_context_;
    tcp::socket socket_;
};

void asioechotest()
{
    asio::io_context listenio_context;

    asioserver s(listenio_context, 3001);
    std::thread listent([&listenio_context]() { listenio_context.run(); });

    asio::io_context clientio_context;
    tcp::resolver resolver(clientio_context);
    auto endpoints = resolver.resolve("127.0.0.1", "3001");
    auto c = std::make_shared<asioclient>(clientio_context, endpoints);

    std::thread t([&clientio_context]() { clientio_context.run(); });
    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    std::cout << "ASIO Echo per Second " << writeechos / 10 << std::endl;
    listenio_context.stop();
    clientio_context.stop();
    s.acceptor_.cancel();
    s.acceptor_.close();
    c->socket_.close();
    listent.join();
    t.join();
}

int main(int argc, char *argv[])
{
    // the verbode ways to use this library
    myechotest();
    writeechos = 0.0;
    asioechotest();

    int k = 0;
    std::cin >> k;
    return 0;
}
