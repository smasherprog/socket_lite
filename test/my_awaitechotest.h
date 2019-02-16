#pragma once
#include "Socket_Lite.h" 
#include "my_echomodels.h"
#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include <future>

using namespace std::chrono_literals;

namespace myawaitechotest {
	auto writeechos = 0.0;
	bool keepgoing = true;
	std::vector<SL::NET::SocketAddress> endpoints;
	std::vector<std::byte> writeecho, readecho;

	std::future<void> connect()
	{
		auto[connectstatus, socket] = co_await SL::NET::async_connect(endpoints.back());
		if (connectstatus == SL::NET::StatusCode::SC_SUCCESS) {
			while (keepgoing) {
				co_await socket.async_recv(readecho.data(), readecho.size());
				co_await socket.async_send(writeecho.data(), writeecho.size());
			}
		}
	}

	std::future<void> accept(SL::NET::AwaitablePlatformSocket& acceptsocket)
	{
		auto[status, socket] = co_await acceptsocket.async_accept();
		if (status == SL::NET::StatusCode::SC_SUCCESS) {
			while (keepgoing) {
				co_await socket.async_recv(readecho.data(), readecho.size());
				co_await socket.async_send(writeecho.data(), writeecho.size());
				writeechos += 1.0;
			}
		}
	}

	void myechotest(int buffersize = 128)
	{
		std::cout << "Starting My Await Echo Test with buffer size of " << buffersize << " bytes" << std::endl;
		keepgoing = true;
		writeechos = 0;
		writeecho.resize(buffersize);
		readecho.resize(buffersize);

		auto porttouse = static_cast<unsigned short>(std::rand() % 3000 + 10000);
		endpoints = SL::NET::getaddrinfo("127.0.0.1", SL::NET::PortNumber(porttouse));

		std::thread([=]() {
			auto socket(myechomodels::listengetaddrinfo(nullptr, SL::NET::PortNumber(porttouse), SL::NET::AddressFamily::IPV4));
			accept(socket).wait();
		}).detach();
		std::thread([]() {  connect().wait(); }).detach();

		std::this_thread::sleep_for(10s); // sleep for 10 seconds
		keepgoing = false;
		std::cout << "My Await Echo per Second " << writeechos / 10 << std::endl;
	}
} // namespace myechotest
