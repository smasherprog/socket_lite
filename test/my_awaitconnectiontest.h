#pragma once
#include "Socket_Lite.h"
#include "my_echomodels.h"
#include <chrono>
#include <iostream>
#include <vector>
#include <future>

using namespace std::chrono_literals;
namespace myawaitconnectiontest {
	auto connections = 0.0;
	bool keepgoing = true;
	std::vector<SL::Network::SocketAddress> endpoints;

	void connect(SL::Network::io_service& context)
	{
		if (keepgoing) {
			SL::Network::socket::connect(context, endpoints.back(), [&context](SL::Network::StatusCode, SL::Network::socket&&) {
				connections += 1.0;
				connect(context);
				});
		}
	}

	void myconnectiontest()
	{
		SL::Network::io_thread_service context(4);
		std::cout << "Starting My Await Connections per Second Test" << std::endl;
		connections = 0.0;
		auto porttouse = static_cast<unsigned short>(std::rand() % 3000 + 10000);

		endpoints = SL::Network::getaddrinfo("127.0.0.1", porttouse);
		auto acceptsocket(myechomodels::Create_and_Bind_Listen_Socket(nullptr, porttouse, SL::Network::SocketType::TCP, SL::Network::AddressFamily::IPV4, context.getio_service()));

		auto t2(std::thread([&]() { connect(context.getio_service()); }));
		SL::Network::async_acceptor acceptor(acceptsocket.value(), [](SL::Network::socket&&) {});
		std::this_thread::sleep_for(10s); // sleep for 10 seconds
		keepgoing = false;
		acceptor.stop();
		std::cout << "My Await Connections per Second " << connections / 10 << std::endl;
		t2.join();

	}
} // namespace myconnectiontest
