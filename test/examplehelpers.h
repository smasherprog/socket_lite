#pragma once
#include "Socket_Lite.h" 
 
namespace ExampleHelpers {

	std::vector<std::byte> writeechob, readechob; 
	auto operations = 0.0;
	bool keepgoing = true;
	SL::Network::SocketAddress connectendpoint; 

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
} 
