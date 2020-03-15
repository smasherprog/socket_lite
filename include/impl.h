#ifndef SL_NETWORK_IMPL
#define SL_NETWORK_IMPL

#if __APPLE__
#include <experimental/optional>
#else
#include <optional>
#endif
#include <algorithm>
#include <assert.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <stdint.h>
#include <string>
#include <thread> 
#include <experimental/coroutine>  
#include <vector>
#include <compare>
#include <variant>
#include <functional>

#if _WIN32 
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 
#endif
#ifndef NOMINMAX
#define NOMINMAX 
#endif
#include <WinSock2.h>
#include <Windows.h>
#include <Ws2tcpip.h>
#include <mswsock.h>
#ifndef SOCKLEN_T
typedef int SOCKLEN_T;
#endif
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#ifndef SOCKLEN_T
typedef socklen_t SOCKLEN_T;
#endif
#endif

namespace SL::Network::Impl {
	class refcounter {
		std::atomic<size_t> PendingOps;
	public:
		refcounter() { PendingOps = 0; }
		size_t getOpCount() const { return PendingOps.load(std::memory_order::memory_order_relaxed); }
		size_t incOp() { return PendingOps.fetch_add(1, std::memory_order::memory_order_relaxed); }
		size_t decOp() { return PendingOps.fetch_sub(1, std::memory_order::memory_order_relaxed); }
	};
#if _WIN32 
	class safe_handle {
	public:
		safe_handle() : shandle(nullptr) {}
		explicit safe_handle(void* handle) : shandle(handle) {}
		safe_handle(const safe_handle& other) = delete;
		safe_handle(safe_handle&& other) noexcept : shandle(other.shandle) { other.shandle = nullptr; }
		~safe_handle() { close(); }
		safe_handle& operator=(safe_handle handle) noexcept
		{
			swap(handle);
			return *this;
		}
		constexpr void* handle() const { return shandle; }
		constexpr operator bool() const noexcept { return shandle != nullptr; }
		/// Calls CloseHandle() and sets the handle to NULL.
		void close() noexcept
		{
			if (shandle != nullptr && shandle != INVALID_HANDLE_VALUE) {
				::CloseHandle(shandle);
				shandle = nullptr;
			}
		}

		void swap(safe_handle& other) noexcept { std::swap(shandle, other.shandle); }
		constexpr std::weak_ordering operator<=>(safe_handle const& rhs) const = default;

	private:
		void* shandle;
	};

	extern LPFN_DISCONNECTEX DisconnectEx_;
	extern LPFN_CONNECTEX ConnectEx_;
	extern LPFN_ACCEPTEX AcceptEx_;
	extern std::uint8_t addressBuffer[(sizeof(SOCKADDR_STORAGE) + 16) * 2];
	inline void SetupWindowsEvents() {
		if (!DisconnectEx_) {
			auto temphandle = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
			GUID disconnectex_guid = WSAID_DISCONNECTEX;
			DWORD bytes = 0;
			WSAIoctl(temphandle, SIO_GET_EXTENSION_FUNCTION_POINTER, &disconnectex_guid, sizeof(disconnectex_guid), &DisconnectEx_, sizeof(DisconnectEx_), &bytes, NULL,
				NULL);
			assert(DisconnectEx_ != nullptr);
			closesocket(temphandle);
		}
		if (!AcceptEx_) {
			auto temphandle = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
			GUID acceptex_guid = WSAID_ACCEPTEX;
			DWORD bytes = 0;
			WSAIoctl(temphandle, SIO_GET_EXTENSION_FUNCTION_POINTER, &acceptex_guid, sizeof(acceptex_guid), &AcceptEx_, sizeof(AcceptEx_), &bytes, NULL,
				NULL);
			assert(AcceptEx_ != nullptr);
			closesocket(temphandle);
		}
		if (!ConnectEx_) {
			auto temphandle = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
			GUID guid = WSAID_CONNECTEX;
			DWORD bytes = 0;
			ConnectEx_ = nullptr;
			WSAIoctl(temphandle, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &ConnectEx_, sizeof(ConnectEx_), &bytes, NULL, NULL);
			assert(ConnectEx_ != nullptr);
			closesocket(temphandle);
		}
	}
#endif
} // namespace SL::Network
#endif