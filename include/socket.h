#ifndef SL_NETWORK_SOCKET
#define SL_NETWORK_SOCKET

#include "socket_accept_operation.h"
#include "socket_connect_operation.h"
#include "socket_disconnect_operation.h"
#include "socket_recv_from_operation.h"
#include "socket_recv_operation.h"
#include "socket_send_operation.h"
#include "socket_send_to_operation.h"

namespace SL::Network {
	class io_service;

	class socket {
	public:
		static std::tuple<StatusCode, socket>  create(io_service& ioSvc, SocketType sockettype = SocketType::TCP, AddressFamily family = AddressFamily::IPV4) {
			auto protocol = sockettype == SocketType::TCP ? IPPROTO_TCP : IPPROTO_UDP;

			auto socketHandle = ::socket(family, sockettype, protocol);
			if (socketHandle == INVALID_SOCKET) {
				return std::tuple(TranslateError(), socket(ioSvc));
			}

			if (sockettype == SocketType::TCP) {
				// Turn off linger so that the destructor doesn't block while closing
				// the socket or silently continue to flush remaining data in the
				// background after ::closesocket() is called, which could fail and
				// we'd never know about it.
				// We expect clients to call Disconnect() or use CloseSend() to cleanly
				// shut-down connections instead.
				BOOL value = TRUE;
				if (::setsockopt(socketHandle, SOL_SOCKET, SO_DONTLINGER, reinterpret_cast<const char*>(&value), sizeof(value)) == SOCKET_ERROR) {
					closesocket(socketHandle);
					return std::tuple(TranslateError(), socket(ioSvc));
				}
			}
			u_long iMode = 1;
			ioctlsocket(socketHandle, FIONBIO, &iMode);
			return std::tuple(StatusCode::SC_SUCCESS, socket(socketHandle, ioSvc));
		}

		socket(socket&& other) noexcept : shandle(std::exchange(other.shandle, INVALID_SOCKET)), ioservice(other.ioservice) {}
		~socket()
		{
			close();
		}

		[[nodiscard]] auto local_endpoint() const
		{
			sockaddr_storage addr = { 0 };
			socklen_t len = sizeof(addr);
			if (::getsockname(shandle, (::sockaddr*) & addr, &len) == 0) {
				std::tuple(StatusCode::SC_SUCCESS, SocketAddress(&addr, len));
			}
			return std::tuple(TranslateError(), SocketAddress());
		}

		[[nodiscard]] auto remote_endpoint() const
		{
			sockaddr_storage addr = { 0 };
			socklen_t len = sizeof(addr);
			if (::getpeername(shandle, (::sockaddr*) & addr, &len) == 0) {
				std::tuple(StatusCode::SC_SUCCESS, SocketAddress(&addr, len));
			}
			return std::tuple(TranslateError(), SocketAddress());
		}

		auto bind(const SocketAddress& localEndPoint)
		{
			if (::bind(shandle, localEndPoint.getSocketAddr(), localEndPoint.getSocketAddrLen()) == SOCKET_ERROR) {
				return TranslateError();
			}
			return StatusCode::SC_SUCCESS;
		}

		auto listen(std::uint32_t backlog)
		{
			if (backlog > SOMAXCONN) {
				backlog = SOMAXCONN;
			}

			if (::listen(shandle, (int)backlog) != 0) {
				return TranslateError();
			}
			return StatusCode::SC_SUCCESS;
		}
		auto listen() { return listen(SOMAXCONN); }
		void close() {
			if (shandle != INVALID_SOCKET) {
				::closesocket(shandle);
			}
			shandle = INVALID_SOCKET;
		}
		[[nodiscard]] auto connect(const SocketAddress& remoteEndPoint) noexcept { return socket_connect_operation<socket>(*this, remoteEndPoint); }
		[[nodiscard]] auto accept(socket& acceptingSocket) noexcept { return socket_accept_operation<socket>(*this, acceptingSocket); }
		[[nodiscard]] auto disconnect() noexcept { return socket_disconnect_operation<socket>(*this); }
		[[nodiscard]] auto send(std::byte* buffer, std::size_t size) noexcept { return socket_send_operation<socket>(*this, buffer, size); }
		[[nodiscard]] auto recv(std::byte* buffer, std::size_t size) noexcept { return socket_recv_operation<socket>(*this, buffer, size); }
		[[nodiscard]] auto recv_from(std::byte* buffer, std::size_t size) noexcept { return socket_recv_from_operation<socket>(*this, buffer, size); }
		[[nodiscard]] auto send_to(const SocketAddress& destination, std::byte* buffer, std::size_t size) noexcept { return socket_send_to_operation<socket>(*this, destination, buffer, size); }

		SOCKET native_handle() const { return shandle; }
		io_service& get_ioservice() const { return ioservice; }

	private:
#if _WIN32
		explicit socket(SOCKET handle, io_service& ioSvc) noexcept : shandle(handle), ioservice(ioSvc) {}
		explicit socket(io_service& ioSvc) noexcept : shandle(INVALID_SOCKET), ioservice(ioSvc) {}
		SOCKET shandle;
		io_service& ioservice;
#endif
	};
} // namespace SL::Network

#endif
