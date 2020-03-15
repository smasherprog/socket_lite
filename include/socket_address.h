#ifndef SL_NETWORK_UTILS
#define SL_NETWORK_UTILS

#include "impl.h"

namespace SL::Network { 
	enum AddressFamily :int { IPV4 = AF_INET, IPV6 = AF_INET6 };
	enum SocketType :int { TCP = SOCK_STREAM, UDP = SOCK_DGRAM };
	class SocketAddress {
		::sockaddr_storage Storage;
		int Length;
		SocketType socktype;
	public:
		SocketAddress() : Storage{ 0 }, Length(0), socktype(SocketType::TCP){}
		SocketAddress(SocketAddress&& addr) noexcept : Length(addr.Length), socktype(addr.socktype)
		{
			memcpy(&Storage, &addr.Storage, addr.Length);
			addr.Length = 0;
		}
		SocketAddress& operator=(const SocketAddress& other)
		{
			if (this != &other) {
				memcpy(&Storage, &other.Storage, other.Length);
				Length = other.Length;
			}
			return *this;
		}
		SocketAddress(const SocketAddress& addr) noexcept : Length(addr.Length), socktype(addr.socktype) { memcpy(&Storage, &addr.Storage, addr.Length); }
		SocketAddress(::sockaddr* buffer, socklen_t len, SocketType socktype) noexcept : Length(static_cast<int>(len)), socktype(socktype)
		{
			assert(len < sizeof(Storage));
			memcpy(&Storage, buffer, len);

		}
		SocketAddress(::sockaddr_storage* buffer, socklen_t len, SocketType socktype) noexcept : Length(static_cast<int>(len)), socktype(socktype)
		{
			assert(len < sizeof(Storage));
			memcpy(&Storage, buffer, len);
		}
		[[nodiscard]] const sockaddr* getSocketAddr() const noexcept { return reinterpret_cast<const ::sockaddr*>(&Storage); }
		[[nodiscard]] int getSocketAddrLen() const noexcept { return Length; }
		[[nodiscard]] SocketType getSocketType() const noexcept { return socktype; }
		[[nodiscard]] std::string getHost() const noexcept
		{
			char str[INET_ADDRSTRLEN] = {};
			auto sockin = reinterpret_cast<const ::sockaddr_in*>(&Storage);
			inet_ntop(Storage.ss_family, &(sockin->sin_addr), str, INET_ADDRSTRLEN);
			return std::string(str);
		}
		[[nodiscard]] unsigned short getPort() const noexcept
		{
			// both ipv6 and ipv4 structs have their port in the same place!
			auto sockin = reinterpret_cast<const sockaddr_in*>(&Storage);
			return ntohs(sockin->sin_port);
		}
		[[nodiscard]] AddressFamily getFamily() const noexcept { return static_cast<AddressFamily>(Storage.ss_family); }
	};

	[[nodiscard]] inline std::vector<SocketAddress> getaddrinfo(const char* address, unsigned short port, SocketType sockettype = SocketType::TCP, AddressFamily family = AddressFamily::IPV4)
	{
		::addrinfo hints = { 0 };
		::addrinfo* result(nullptr);
		memset(&hints, 0, sizeof(addrinfo));
		hints.ai_family = family;
		hints.ai_socktype = sockettype;
		hints.ai_protocol = sockettype == SocketType::TCP ? IPPROTO_TCP : IPPROTO_UDP;
		hints.ai_flags = AI_PASSIVE; /* All interfaces */
		std::vector<SocketAddress> addrs;
		auto portstr = std::to_string(port);
		auto s = ::getaddrinfo(address, portstr.c_str(), &hints, &result);
		if (s != 0) {
			return addrs;
		}
		for (auto ptr = result; ptr != NULL; ptr = ptr->ai_next) {
			addrs.emplace_back(SocketAddress(ptr->ai_addr, static_cast<socklen_t>(ptr->ai_addrlen), sockettype));
		}
		freeaddrinfo(result);
		return addrs;
	}
	namespace impl {
		auto inline win32Bind(AddressFamily family, SOCKET socket) {
			if (family == AddressFamily::IPV4) {
				sockaddr_in bindaddr = { 0 };
				bindaddr.sin_family = AF_INET;
				bindaddr.sin_addr.s_addr = INADDR_ANY;
				bindaddr.sin_port = 0;
				return ::bind(socket, (::sockaddr*) & bindaddr, sizeof(bindaddr));
			}
			else {
				sockaddr_in6 bindaddr = { 0 };
				bindaddr.sin6_family = AF_INET6;
				bindaddr.sin6_addr = in6addr_any;
				bindaddr.sin6_port = 0;
				return ::bind(socket, (::sockaddr*) & bindaddr, sizeof(bindaddr));
			}
		}
	} 

	//class rf_overlapped_operation : public overlapped_operation {
	//public:
	//	rf_overlapped_operation(std::function<void(StatusCode, size_t)>&& coro) : overlapped_operation(OP_Type::RF), awaitingCoroutine(coro), socklen(0), storage({ 0 }) {}
	//	std::function<void(StatusCode, size_t)> awaitingCoroutine;
	//	DWORD socklen;
	//	::sockaddr_storage storage;
	//};
	//class st_overlapped_operation : public overlapped_operation {
	//public:
	//	st_overlapped_operation(std::function<void(StatusCode, size_t)>&& coro) : overlapped_operation(OP_Type::ST), awaitingCoroutine(coro) {}
	//	std::function<void(StatusCode, size_t)> awaitingCoroutine;
	//	SocketAddress remoteEndPoint;
	//};
	
} // namespace SL::Network
#endif