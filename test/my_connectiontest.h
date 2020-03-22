#pragma once
#include "Socket_Lite.h"
#include "examplehelpers.h"
#include <chrono>
#include <iostream> 
#include <vector>

using namespace std::chrono_literals;

namespace myconnectiontest {
	void runtest()
	{
		SL::Network::io_thread_service threadcontext(SL::Network::IO_Events(
			[&](auto& ioservice, auto socket, SL::Network::StatusCode) {
				ExampleHelpers::operations += 1.0;
				socket.close();
				if (ExampleHelpers::keepgoing) {
					auto [statuscode, s] = SL::Network::create_socket(ioservice, ExampleHelpers::connectendpoint.getSocketType(), ExampleHelpers::connectendpoint.getFamily());
					s.connect(ExampleHelpers::connectendpoint);
				}
			},
			[&](auto& ioservice, auto acceptsocket, SL::Network::StatusCode, auto listensocket) {
				acceptsocket.close();
				if (ExampleHelpers::keepgoing) {
					auto [statuscode, s] = SL::Network::create_socket(ioservice, ExampleHelpers::connectendpoint.getSocketType(), ExampleHelpers::connectendpoint.getFamily());
					listensocket.accept(s);
				}
			},
				[&](auto&, auto socket, SL::Network::StatusCode, int) {
				socket.close();
			},
				[&](auto&, auto socket, SL::Network::StatusCode, int) {
				socket.close();
			}));

		std::cout << "Starting My Connections per Second Test" << std::endl;
		ExampleHelpers::operations = 0.0;
		auto porttouse = static_cast<unsigned short>(std::rand() % 3000 + 10000);
		auto addrs = SL::Network::getaddrinfo(nullptr, porttouse, SL::Network::SocketType::TCP, SL::Network::AddressFamily::IPV4);
		auto [statuscode, acceptsocket] = SL::Network::create_socket(threadcontext.ioservice(), SL::Network::SocketType::TCP, SL::Network::AddressFamily::IPV4);
		auto binderro = acceptsocket.bind(addrs.back());
		auto listenerror = acceptsocket.listen(500);
		auto [astatuscode, as] = SL::Network::create_socket(threadcontext.ioservice(), addrs.back().getSocketType(), addrs.back().getFamily());
		acceptsocket.accept(as);

		auto endpoints = SL::Network::getaddrinfo("127.0.0.1", porttouse);
		ExampleHelpers::connectendpoint = endpoints.back();
		auto [constatuscode, s] = SL::Network::create_socket(threadcontext.ioservice(), ExampleHelpers::connectendpoint.getSocketType(), ExampleHelpers::connectendpoint.getFamily());
		s.connect(ExampleHelpers::connectendpoint);

		std::this_thread::sleep_for(10s); // sleep for 10 seconds
		ExampleHelpers::keepgoing = false;
		std::cout << "My Connections per Second " << ExampleHelpers::operations / 10 << std::endl;
		acceptsocket.close();
	}
} // namespace myconnectiontest
