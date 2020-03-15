#pragma once
#include "Socket_Lite.h"
#include "my_echomodels.h"
#include <chrono>
#include <iostream>
#include <vector>
#include <future>
#include <vector>

using namespace std::chrono_literals;

namespace myawaitconnectiontest {
	auto connections = 0.0;
	bool keepgoing = true;
	SL::Network::SocketAddress connectendpoint;

	template<class CONTEXTTYPE>void connect(CONTEXTTYPE& context)
	{
		if (keepgoing) {
			auto [statuscode, s] = SL::Network::create(context, connectendpoint.getSocketType(), connectendpoint.getFamily());
			s.connect(connectendpoint);
		}
	}

	void myconnectiontest()
	{
		SL::Network::io_thread_service threadcontext(SL::Network::IO_Events(
			[&](auto ioservice, auto socket, SL::Network::StatusCode code) {
				connections += 1.0;
				socket.close();
				connect(ioservice);
			},
			[&](auto ioservice, auto socket, SL::Network::StatusCode code, auto acceptsocket) {
				socket.close();
				auto [statuscode, s] = SL::Network::create_socket(ioservice, connectendpoint.getSocketType(), connectendpoint.getFamily());
				acceptsocket.accept(s);
			},
			[&](auto ioservice, auto socket, SL::Network::StatusCode code, int bytestransfered) {
				socket.close();
			},
			[&](auto ioservice, auto socket, SL::Network::StatusCode code, int bytestransfered) {
				socket.close();
			}), 4);

		std::cout << "Starting My Await Connections per Second Test" << std::endl;
		connections = 0.0;
		auto porttouse = static_cast<unsigned short>(std::rand() % 3000 + 10000);

		auto endpoints = SL::Network::getaddrinfo("127.0.0.1", porttouse);
		connectendpoint = endpoints.back(); 

		auto t2(std::thread([&]() { connect(threadcontext.ioservice()); }));
		auto [astatuscode, as] = SL::Network::create_socket(threadcontext.ioservice(), connectendpoint.getSocketType(), connectendpoint.getFamily());	
		auto addrs = SL::Network::getaddrinfo(nullptr, porttouse, SL::Network::SocketType::TCP, SL::Network::AddressFamily::IPV4);
		auto [statuscode, acceptsocket] = SL::Network::create_socket(threadcontext.ioservice(), SL::Network::SocketType::TCP, SL::Network::AddressFamily::IPV4);
		acceptsocket.bind(addrs.back());
		acceptsocket.listen(500);
		acceptsocket.accept(as);

		std::this_thread::sleep_for(10s); // sleep for 10 seconds
		keepgoing = false;
		std::cout << "My Await Connections per Second " << connections / 10 << std::endl;
		t2.join();

	}
} // namespace myconnectiontest
