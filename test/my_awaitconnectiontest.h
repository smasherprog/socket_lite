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

	std::future<void> connect(SL::Network::io_service& context)
	{
		while (keepgoing) {
			auto [statuscode, socket] = SL::Network::socket::create(context);
			if (statuscode == SL::Network::StatusCode::SC_SUCCESS) {
				co_await socket.connect(endpoints.back());
				connections += 1.0;
			}
		}
	}
	std::future<void> accept(SL::Network::socket& acceptsocket, SL::Network::io_service& context)
	{
		while (keepgoing) {
			auto [statuscode, socket] = SL::Network::socket::create(context);
			if (statuscode == SL::Network::StatusCode::SC_SUCCESS) {
				co_await acceptsocket.accept(socket);
			}
		}
	}
	void myconnectiontest()
	{
		SL::Network::io_thread_service context(1);
		std::cout << "Starting My Await Connections per Second Test" << std::endl;
		connections = 0.0;
		auto porttouse = static_cast<unsigned short>(std::rand() % 3000 + 10000);

		endpoints = SL::Network::getaddrinfo("127.0.0.1", porttouse);
		auto acceptsocket(myechomodels::Create_and_Bind_Listen_Socket(nullptr, porttouse, SL::Network::SocketType::TCP, SL::Network::AddressFamily::IPV4, context.getio_service()));
		auto t1(std::thread([&]() { accept(acceptsocket.value(), context.getio_service()).wait(); }));
		auto t2(std::thread([&]() { connect(context.getio_service()).wait(); }));

		std::this_thread::sleep_for(10s); // sleep for 10 seconds
		keepgoing = false;
		acceptsocket.value().close();
		t1.join();
		t2.join();

		std::cout << "My Await Connections per Second " << connections / 10 << std::endl;
	}
} // namespace myconnectiontest
