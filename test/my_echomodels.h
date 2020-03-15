#pragma once
#include "Socket_Lite.h"
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <future>

using namespace std::chrono_literals;

namespace myechomodels {

	std::vector<std::byte> writeechob, readechob;

	auto writeechos = 0.0;
	bool keepgoing = true;

	//class asioserver {
	//public:
	//	SL::Network::socket listensocket;

	//	asioserver(SL::Network::socket& lsocket) : listensocket(std::move(lsocket)) {}
	//	std::future<void> do_accept()
	//	{
	//		while (keepgoing) {
	//			auto [statuscode, socket] = SL::Network::socket::create(listensocket.get_ioservice());
	//			if (statuscode == SL::Network::StatusCode::SC_SUCCESS) {
	//				auto status = co_await listensocket.accept(socket);
	//				auto [s, bytes] = co_await socket.send(writeechob.data(), writeechob.size());
	//				auto [s1, bytes1] = co_await socket.recv(readechob.data(), readechob.size());
	//			}
	//		}
	//	}
	//};

	template<class CONTEXTTYPE, class SOCKETTYPE>std::optional<SOCKETTYPE> Create_and_Bind_Listen_Socket(const char* nodename, unsigned short port, SL::Network::SocketType sockt, SL::Network::AddressFamily family, CONTEXTTYPE& context)
	{
		auto addrs = SL::Network::getaddrinfo(nodename, port, sockt, family);
		for (auto& a : addrs) {
			auto [statuscode, socket] = SL::Network::socket::create(context, sockt, family);
			if (statuscode == SL::Network::StatusCode::SC_SUCCESS && socket.bind(a) == SL::Network::StatusCode::SC_SUCCESS) {
				if (socket.listen(500) == SL::Network::StatusCode::SC_SUCCESS) {
					return socket;
				}
			}
		}
		return std::nullopt;
	}
	 /*
	class awaitclient {
		SL::Network::io_service& context_;
	public:
		std::vector<SL::Network::SocketAddress> addrs;
		awaitclient(const char* nodename, unsigned short port, SL::Network::io_service& context) : context_(context)
		{
			addrs = SL::Network::getaddrinfo(nodename, port);
		}
		std::future<void> do_connect()
		{
			auto [statuscode, socket] = SL::Network::socket::create(context_);
			if (statuscode == SL::Network::StatusCode::SC_SUCCESS) {
				auto connectstatus = co_await socket.connect(addrs.back());
				if (connectstatus == SL::Network::StatusCode::SC_SUCCESS) {
					while (keepgoing) {
						auto [s, bytes] = co_await socket.send(readechob.data(), readechob.size());
						auto [s1, bytes1] = co_await socket.recv(writeechob.data(), writeechob.size());
						writeechos += 1.0;
					}
				}
			}
		}
	};*/
} // namespace myechomodels
