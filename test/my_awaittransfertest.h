#pragma once
#include "Socket_Lite.h"
#include "my_echomodels.h"
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <future>

using namespace std::chrono_literals;

namespace myawaittransfertest {
	auto writeechos = 0.0;
	bool keepgoing = true;
	std::vector<SL::NET::SocketAddress> endpoints;
	std::vector<std::byte> writebuffer, readbuffer;

	std::future<void> connect()
	{
		auto[connectstatus, socket] = co_await SL::NET::async_connect(endpoints.back());
		if (connectstatus == SL::NET::StatusCode::SC_SUCCESS) {
			while (keepgoing) {
				auto[status, bytestransfered] = co_await socket.async_send(writebuffer.data(), writebuffer.size());
				if (status == SL::NET::StatusCode::SC_SUCCESS) {
					writeechos += 1.0;
				}
			}
		}
	}

	std::future<void> accept(SL::NET::AwaitablePlatformSocket& acceptsocket)
	{
		auto[status, socket] = co_await acceptsocket.async_accept();
		if (status == SL::NET::StatusCode::SC_SUCCESS) {
			while (keepgoing) {
				co_await socket.async_recv(readbuffer.data(), readbuffer.size());
			}
		}
	}

	void mytransfertest()
	{
		std::cout << "Starting My Await MB per Second Test" << std::endl;
		keepgoing = true;
		writeechos = 0.0;
		writebuffer.resize(1024 * 1024 * 8);
		readbuffer.resize(1024 * 1024 * 8);

		auto porttouse = static_cast<unsigned short>(std::rand() % 3000 + 10000);
		endpoints = SL::NET::getaddrinfo("127.0.0.1", SL::NET::PortNumber(porttouse));

		std::thread([=]() {
			auto socket(myechomodels::listengetaddrinfo(nullptr, SL::NET::PortNumber(porttouse), SL::NET::AddressFamily::IPV4));
			accept(socket).wait();
		}).detach();
		std::thread([]() {  connect().wait(); }).detach();

		std::this_thread::sleep_for(10s); // sleep for 10 seconds
		keepgoing = false;
		std::cout << "My Await MB per Second " << (writeechos / 10) * 8 << std::endl;
	}

} // namespace mytransfertest
