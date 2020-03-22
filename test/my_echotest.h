#pragma once
#include "Socket_Lite.h"
#include "examplehelpers.h"
#include <chrono>
#include <iostream> 
#include <vector>

using namespace std::chrono_literals;

namespace myechotest {
	void runtest(int buffersize = 128)
	{

		std::cout << "Starting My Echo Test with buffer size of " << buffersize << " bytes" << std::endl;
		ExampleHelpers::keepgoing = true;
		ExampleHelpers::operations = 0;
		ExampleHelpers::writeechob.resize(buffersize);
		ExampleHelpers::readechob.resize(buffersize);

		SL::Network::io_thread_service threadcontext(SL::Network::IO_Events(
			[&](auto& , auto socket, SL::Network::StatusCode) {
				if (ExampleHelpers::keepgoing) {
					socket.send(ExampleHelpers::writeechob.data(), ExampleHelpers::writeechob.size());
				}
				else {
					socket.close();
				}
			},
			[&](auto& ioservice, auto acceptsocket, SL::Network::StatusCode code, auto listensocket) {
				if (ExampleHelpers::keepgoing) {
					auto [astatuscode, as] = SL::Network::create_socket(ioservice, ExampleHelpers::connectendpoint.getSocketType(), ExampleHelpers::connectendpoint.getFamily());
					listensocket.accept(as);
					acceptsocket.recv(ExampleHelpers::readechob.data(), ExampleHelpers::readechob.size());
				}
				else {
					acceptsocket.close();
				}
			},
				[&](auto& , auto socket, SL::Network::StatusCode , int) {
				ExampleHelpers::operations += 1.0;
				if (ExampleHelpers::keepgoing) {
					socket.send(ExampleHelpers::writeechob.data(), ExampleHelpers::writeechob.size());
				}
				else {
					socket.close();
				}
			},
				[&](auto& , auto socket, SL::Network::StatusCode , int ) {
				ExampleHelpers::operations += 1.0;
				if (ExampleHelpers::keepgoing) {
					socket.recv(ExampleHelpers::readechob.data(), ExampleHelpers::readechob.size());
				}
				else {
					socket.close();
				}
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

		{
			auto [constatuscode, s] = SL::Network::create_socket(threadcontext.ioservice(), ExampleHelpers::connectendpoint.getSocketType(), ExampleHelpers::connectendpoint.getFamily());
			s.connect(ExampleHelpers::connectendpoint);
		}
		{
			auto [constatuscode, s] = SL::Network::create_socket(threadcontext.ioservice(), ExampleHelpers::connectendpoint.getSocketType(), ExampleHelpers::connectendpoint.getFamily());
			s.connect(ExampleHelpers::connectendpoint);
		}
		{
			auto [constatuscode, s] = SL::Network::create_socket(threadcontext.ioservice(), ExampleHelpers::connectendpoint.getSocketType(), ExampleHelpers::connectendpoint.getFamily());
			s.connect(ExampleHelpers::connectendpoint);
		}

		std::this_thread::sleep_for(10s); // sleep for 10 seconds
		ExampleHelpers::keepgoing = false;
		std::cout << "My  Echo per Second " << ExampleHelpers::operations / 10 << std::endl;
		acceptsocket.close();
	}
} // namespace myechotest
