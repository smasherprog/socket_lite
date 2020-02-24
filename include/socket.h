#ifndef SL_NETWORK_SOCKET
#define SL_NETWORK_SOCKET

#include "utils.h"
#include "io_service.h"
#include <tuple>

namespace SL::Network {
	class io_service;
	class socket {
	public:
		static auto create(io_service& ioSvc, SocketType sockettype = SocketType::TCP, AddressFamily family = AddressFamily::IPV4) {
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
			return std::tuple(StatusCode::SC_SUCCESS, std::move(socket(socketHandle, ioSvc)));
		}
		socket(socket&& other) noexcept : shandle(std::exchange(other.shandle, INVALID_SOCKET)), ioservice(other.ioservice) {}
		~socket() {
			close();
		}
		[[nodiscard]] auto local_endpoint() const
		{
			sockaddr_storage addr = { 0 };
			socklen_t len = sizeof(addr);
			if (::getsockname(shandle, (::sockaddr*) & addr, &len) == 0) {
				int type;
				int length = sizeof(int);
				getsockopt(shandle, SOL_SOCKET, SO_TYPE, (char*)&type, &length);
				std::tuple(StatusCode::SC_SUCCESS, SocketAddress(&addr, len, type == IPPROTO_TCP ? SocketType::TCP : SocketType::UDP));
			}
			return std::tuple(TranslateError(), SocketAddress());
		}

		[[nodiscard]] auto remote_endpoint() const
		{
			sockaddr_storage addr = { 0 };
			socklen_t len = sizeof(addr);
			if (::getpeername(shandle, (::sockaddr*) & addr, &len) == 0) {
				int type;
				int length = sizeof(int);
				getsockopt(shandle, SOL_SOCKET, SO_TYPE, (char*)&type, &length);
				std::tuple(StatusCode::SC_SUCCESS, SocketAddress(&addr, len, type == IPPROTO_TCP ? SocketType::TCP : SocketType::UDP));
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
				::CancelIoEx((HANDLE)shandle, NULL);
				::closesocket(shandle);
				shandle = INVALID_SOCKET;
			}
		}
		auto onconnectsuccess() {
			if (::setsockopt(shandle, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, nullptr, 0) == SOCKET_ERROR) {
				return TranslateError();
			}
			return StatusCode::SC_SUCCESS;
		}

		template<class T>static void connect(io_service& context, const SocketAddress& remoteEndPoint, T&& cb) noexcept {
			auto [statuscode, s] = SL::Network::socket::create(context, remoteEndPoint.getSocketType(), remoteEndPoint.getFamily());
			if (statuscode != StatusCode::SC_SUCCESS) {
				return cb(statuscode, std::move(s));
			}
			auto handle = s.shandle;
			s.shandle = INVALID_SOCKET;
			auto& ioservice = s.get_ioservice();
			if (::CreateIoCompletionPort((HANDLE)handle, ioservice.getHandle(), handle, 0) == NULL) {
				return cb(TranslateError(), std::move(s));
			}
			if (win32::win32Bind(remoteEndPoint.getFamily(), handle) == SOCKET_ERROR) {
				return cb(TranslateError(), std::move(s));
			}
			if (SetFileCompletionNotificationModes((HANDLE)handle, FILE_SKIP_COMPLETION_PORT_ON_SUCCESS) == FALSE) {
				return cb(TranslateError(), std::move(s));
			}
			auto overlapped = new overlapped_operation([callback(std::move(cb)), handle = handle, &c(context)](StatusCode status, size_t) {
				if (status == StatusCode::SC_SUCCESS) {
					if (::setsockopt(handle, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, nullptr, 0) == SOCKET_ERROR) {
						status = TranslateError();
					}
				}
				callback(status, std::move(socket(handle, c)));
			});

			ioservice.getRefCounter().incOp();
			DWORD transferedbytes = 0;
			if (win32::ConnectEx_(handle, remoteEndPoint.getSocketAddr(), remoteEndPoint.getSocketAddrLen(), 0, 0, &transferedbytes, overlapped->getOverlappedStruct()) == FALSE) {
				auto e = TranslateError();
				auto originalvalue = overlapped->trysetstatus(e, StatusCode::SC_UNSET);
				if (originalvalue == StatusCode::SC_UNSET) {///successfully change from unset to my value and a real error has occured, no iocp will be fired
					if (e != StatusCode::SC_PENDINGIO) {
						ioservice.getRefCounter().decOp();
						overlapped->awaitingCoroutine(e, 0);
						delete overlapped;
					}
				}
				////otherwise, the op is pending and ioservice will handle this
			}
			else {
				ioservice.getRefCounter().decOp();
				overlapped->awaitingCoroutine(StatusCode::SC_SUCCESS, 0);
				delete overlapped;
			}
		}

		/*
		[[nodiscard]] auto disconnect() noexcept { return socket_disconnect_operation<socket>(*this); }
		[[nodiscard]] auto send(std::byte* buffer, std::size_t size) noexcept { return socket_send_operation<socket>(*this, buffer, size); }
		[[nodiscard]] auto recv(std::byte* buffer, std::size_t size) noexcept { return socket_recv_operation<socket>(*this, buffer, size); }
		[[nodiscard]] auto recv_from(std::byte* buffer, std::size_t size) noexcept { return socket_recv_from_operation<socket>(*this, buffer, size); }
		[[nodiscard]] auto send_to(const SocketAddress& destination, std::byte* buffer, std::size_t size) noexcept { return socket_send_to_operation<socket>(*this, destination, buffer, size); }*/

		SOCKET native_handle() const { return shandle; }
		io_service& get_ioservice() const { return ioservice; }

		explicit socket(SOCKET handle, io_service& ioSvc) noexcept : shandle(handle), ioservice(ioSvc) {}
	private:
#if _WIN32
		explicit socket(io_service& ioSvc) noexcept : shandle(INVALID_SOCKET), ioservice(ioSvc) {}
		SOCKET shandle;
		io_service& ioservice;
#endif
	};
} // namespace SL::Network

#endif
