#pragma once
#include "Socket_Lite.h"
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace myechomodels {

	SL::NET::AsyncSocket listengetaddrinfo(const char *nodename, SL::NET::PortNumber port, SL::NET::AddressFamily family, SL::NET::Context &context)
	{
		auto addrs = SL::NET::getaddrinfo(nodename, port, family);
		for (auto &a : addrs) {
			auto[statuscode, socket] = SL::NET::AsyncSocket::Create(a.getFamily(), context);
			if (statuscode == SL::NET::StatusCode::SC_SUCCESS && socket.bind(a) == SL::NET::StatusCode::SC_SUCCESS) {
				if (socket.listen(INT_MAX) == SL::NET::StatusCode::SC_SUCCESS) {
					[[maybe_unused]] auto reter = socket.setsockopt(SL::NET::REUSEADDRTag{}, SL::NET::SockOptStatus::ENABLED);
					return std::move(socket);
				}
			}
		}
		return SL::NET::AsyncSocket(context); // empty socket
	}
	SL::NET::AwaitablePlatformSocket listengetaddrinfo(const char *nodename, SL::NET::PortNumber port, SL::NET::AddressFamily family)
	{
		auto addrs = SL::NET::getaddrinfo(nodename, port, family);
		for (auto &a : addrs) {
			auto[statuscode, socket] = SL::NET::PlatformSocket::Create(a.getFamily());
			if (statuscode == SL::NET::StatusCode::SC_SUCCESS && socket.bind(a) == SL::NET::StatusCode::SC_SUCCESS) {
				if (socket.listen(INT_MAX) == SL::NET::StatusCode::SC_SUCCESS) {
					[[maybe_unused]] auto reter = socket.setsockopt(SL::NET::REUSEADDRTag{}, SL::NET::SockOptStatus::ENABLED);
					return std::move(socket);
				}
			}
		}
		return SL::NET::AwaitablePlatformSocket(); // empty socket
	}

	std::vector<char> writeecho, readecho; 
	std::vector<std::byte> writeechob, readechob;

	auto writeechos = 0.0;
	bool keepgoing = true;

	class session : public std::enable_shared_from_this<session> {

	public:
		session(SL::NET::AsyncSocket socket) : socket_(std::move(socket)) {}
		void do_read()
		{
			auto self(shared_from_this());
			socket_.recv((unsigned char *)readecho.data(), readecho.size(), [self](SL::NET::StatusCode code) {
				if (code == SL::NET::StatusCode::SC_SUCCESS) {
					self->do_read();
				}
			});
		}

		void do_write()
		{
			auto self(shared_from_this());
			socket_.send((unsigned char *)writeecho.data(), writeecho.size(), [self](SL::NET::StatusCode code) {
				if (code == SL::NET::StatusCode::SC_SUCCESS) {
					writeechos += 1.0;
					self->do_write();
				}
			});
		}
		SL::NET::AsyncSocket socket_;
	};

	class asioclient {
	public:
		std::shared_ptr<session> socket_;
		std::vector<SL::NET::SocketAddress> addrs;
		asioclient(SL::NET::Context &io_context, const char *nodename, SL::NET::PortNumber port) : socket_(std::make_shared<session>(io_context))
		{
			addrs = SL::NET::getaddrinfo(nodename, port);
		}
		void do_connect()
		{
			SL::NET::AsyncSocket::connect(socket_->socket_, addrs.back(), [&](SL::NET::StatusCode connectstatus) {
				if (connectstatus == SL::NET::StatusCode::SC_SUCCESS) {
					socket_->do_write();
					socket_->do_read();
				}
			});
		}
	};

	class awaitclient {
	public:
		std::vector<SL::NET::SocketAddress> addrs;
		awaitclient(const char *nodename, SL::NET::PortNumber port)
		{
			addrs = SL::NET::getaddrinfo(nodename, port);
		}
		std::future<void>  do_connect()
		{
			auto[connectstatus, socket] = co_await SL::NET::async_connect(addrs.back());
			if (connectstatus == SL::NET::StatusCode::SC_SUCCESS) { 
				while (keepgoing) {
					co_await socket.async_recv(readechob.data(), readecho.size());
					co_await socket.async_send(writeechob.data(), writeechob.size());
					writeechos += 1.0;
				}
			}
		}
	};
} // namespace myechomodels
