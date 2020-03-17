#ifndef SL_NETWORK_SOCKET
#define SL_NETWORK_SOCKET

#include "impl.h"
#include "error_handling.h"
#include "socket_address.h"
#include "io_service.h"
#include <tuple>

namespace SL::Network {
	template<typename IOCONTEXT>class socket {
	public:

		socket(const socket& other) noexcept : shandle(other.shandle), ios(other.ios) {}
		socket(socket&& other) noexcept : shandle(std::exchange(other.shandle, INVALID_SOCKET)), ios(other.ios) {}

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
			return std::tuple(Impl::TranslateError(), SocketAddress());
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
			return std::tuple(Impl::TranslateError(), SocketAddress());
		}

		auto bind(const SocketAddress& localEndPoint)
		{
			if (::bind(shandle, localEndPoint.getSocketAddr(), localEndPoint.getSocketAddrLen()) == SOCKET_ERROR) {
				return Impl::TranslateError();
			}
			return StatusCode::SC_SUCCESS;
		}

		auto listen(std::uint32_t backlog)
		{
			if (backlog > SOMAXCONN) {
				backlog = SOMAXCONN;
			}

			if (::listen(shandle, (int)backlog) != 0) {
				return Impl::TranslateError();
			}
			return StatusCode::SC_SUCCESS;
		}
		auto listen() { return listen(SOMAXCONN); }
		void close() {
			if (shandle != INVALID_SOCKET) {
				auto s = shandle;
				shandle = INVALID_SOCKET; 
				::closesocket(s);
			}
		}

		void connect(const SocketAddress& remoteEndPoint) noexcept {
			if (Impl::win32Bind(remoteEndPoint.getFamily(), shandle) == SOCKET_ERROR) {
				return ios.IOEvents.OnConnect(ios, *this, Impl::TranslateError());
			}
			if (SetFileCompletionNotificationModes((HANDLE)shandle, FILE_SKIP_COMPLETION_PORT_ON_SUCCESS) == FALSE) {
				return ios.IOEvents.OnConnect(ios, *this, Impl::TranslateError());
			}
			auto overlapped = new overlapped_operation(OP_Type::OnConnect, shandle);
			ios.refcounter.incOp();
			DWORD transferedbytes = 0;
			auto e = StatusCode::SC_SUCCESS;
			if (ios.ConnectEx_(shandle, remoteEndPoint.getSocketAddr(), remoteEndPoint.getSocketAddrLen(), 0, 0, &transferedbytes, overlapped->getOverlappedStruct()) == FALSE) {
				e = Impl::TranslateError();
				auto originalvalue = overlapped->trysetstatus(e, StatusCode::SC_UNSET);
				if (originalvalue == StatusCode::SC_UNSET) {///successfully change from unset to my value and a real error has occured, no iocp will be fired
					if (e != StatusCode::SC_PENDINGIO) {
						ios.refcounter.decOp();
						delete overlapped;
						return ios.IOEvents.OnConnect(ios, *this, Impl::TranslateError()); 
					}
				}
				////otherwise, the op is pending and ioservice will handle this
			}
			else {
				if (::setsockopt(shandle, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, nullptr, 0) == SOCKET_ERROR) {
					e = Impl::TranslateError();
				}
				ios.refcounter.decOp();
				delete overlapped;
				return ios.IOEvents.OnConnect(ios, *this, Impl::TranslateError());
			} 
		}

		void accept() noexcept {  
			auto overlapped = new overlapped_operation(OP_Type::OnAccept, shandle);
			ios.refcounter.incOp();    
			PostQueuedCompletionStatus((HANDLE)ios.IOCPHandle.handle(), 0, 0, overlapped->getOverlappedStruct());
		}

		auto send(std::byte* buffer, std::size_t size) noexcept {
			auto overlapped = new overlapped_operation(OP_Type::OnSend, shandle);
			ios.refcounter.incOp();
			WSABUF wsabuf;
			wsabuf.buf = reinterpret_cast<char*>(buffer);
			wsabuf.len = static_cast<decltype(wsabuf.len)>(size);
			DWORD transferedbytes = 0;
			auto e = StatusCode::SC_SUCCESS;
			if (::WSASend(shandle, &wsabuf, 1, &transferedbytes, 0, overlapped->getOverlappedStruct(), nullptr) == SOCKET_ERROR) {
				e = Impl::TranslateError();
				auto originalvalue = overlapped->trysetstatus(e, StatusCode::SC_UNSET);
				if (originalvalue == StatusCode::SC_UNSET) {///successfully change from unset to my value and a real error has occured, no iocp will be fired
					if (e != StatusCode::SC_PENDINGIO) {
						ioservice.refcounter.decOp();
						delete overlapped;
						return ios.IOEvents.OnSend(ios, *this, e, 0); 
					}
				}
			}
			else {
				ios.refcounter.decOp();
				delete overlapped;
				return ios.IOEvents.OnSend(ios, *this, e, numberOfBytesTransferred); 
			} 
		}
		/*
		template<class T>auto send_to(const SocketAddress& remoteEndPoint, std::byte* buffer, std::size_t size, T&& cb) noexcept {
			auto overlapped = new st_overlapped_operation(shandle, cb);
			overlapped->remoteEndPoint = remoteEndPoint;
			ioservice.refcounter.incOp();
			overlapped->wsabuf.buf = reinterpret_cast<char*>(buffer);
			overlapped->wsabuf.len = static_cast<decltype(wsabuf.len)>(size);
			DWORD numberOfBytesTransferred(0), dwFlags(0);
			auto e = StatusCode::SC_SUCCESS;
			SocketAddress sa;
			if (::WSASendTo(shandle, &(overlapped->wsabuf), 1, &numberOfBytesTransferred, &dwFlags, overlapped->remoteEndPoint.getSocketAddr(), overlapped->remoteEndPoint.getSocketAddrLen(), overlapped->getOverlappedStruct(), nullptr) == SOCKET_ERROR) {
				e = Impl::TranslateError();
				auto originalvalue = overlapped->trysetstatus(e, StatusCode::SC_UNSET);
				if (originalvalue == StatusCode::SC_UNSET) {///successfully change from unset to my value and a real error has occured, no iocp will be fired
					if (e != StatusCode::SC_PENDINGIO) {
						ioservice.refcounter.decOp();
						delete overlapped;
					}
				}
			}
			else {
				ioservice.refcounter.decOp();
				delete overlapped;
			}
			return std::tuple(e, numberOfBytesTransferred);
		}*/
		auto recv(std::byte* buffer, std::size_t size) noexcept {
			auto overlapped = new overlapped_operation(OP_Type::OnRead, shandle);
			ioservice.refcounter.incOp();
			WSABUF wsabuf;
			wsabuf.buf = reinterpret_cast<char*>(buffer);
			wsabuf.len = static_cast<decltype(wsabuf.len)>(size);
			DWORD dwSendNumBytes(0), dwFlags(0);
			auto e = StatusCode::SC_SUCCESS;
			if (::WSARecv(shandle, &wsabuf, 1, &dwSendNumBytes, &dwFlags, overlapped->getOverlappedStruct(), nullptr) == SOCKET_ERROR) {
				e = Impl::TranslateError();
				auto originalvalue = overlapped->trysetstatus(e, StatusCode::SC_UNSET);
				if (originalvalue == StatusCode::SC_UNSET) {///successfully change from unset to my value and a real error has occured, no iocp will be fired
					if (e != StatusCode::SC_PENDINGIO) {
						ioservice.refcounter.decOp();
						delete overlapped;
						return ios.IOEvents.OnRecv(ios, *this, e, dwSendNumBytes);
					}
				}
			}
			else {
				ioservice.refcounter.decOp();
				delete overlapped;
				return ios.IOEvents.OnRecv(ios, *this, e, dwSendNumBytes);
			} 
		}

		//auto recv_from(std::byte* buffer, std::size_t size) noexcept {
		//	auto overlapped = new rf_overlapped_operation(shandle, cb);
		//	ioservice.refcounter.incOp();
		//	overlapped->wsabuf.buf = reinterpret_cast<char*>(buffer);
		//	overlapped->wsabuf.len = static_cast<decltype(wsabuf.len)>(size);
		//	DWORD numberOfBytesTransferred(0), dwFlags(0);
		//	auto e = StatusCode::SC_SUCCESS;
		//	SocketAddress sa;
		//	if (::WSARecvFrom(shandle, &(overlapped->wsabuf), 1, &numberOfBytesTransferred, &dwFlags, reinterpret_cast<sockaddr*>(&(overlapped->storage)), &(overlapped->socklen), overlapped->getOverlappedStruct(), nullptr) == SOCKET_ERROR) {
		//		e = Impl::TranslateError();
		//		auto originalvalue = overlapped->trysetstatus(e, StatusCode::SC_UNSET);
		//		if (originalvalue == StatusCode::SC_UNSET) {///successfully change from unset to my value and a real error has occured, no iocp will be fired
		//			if (e != StatusCode::SC_PENDINGIO) {
		//				ioservice.refcounter.decOp();
		//				delete overlapped;
		//			}
		//		}
		//	}
		//	else {
		//		ioservice.refcounter.decOp();
		//		sa = std::move(SocketAddress(overlapped->storage, socklen));
		//		delete overlapped;
		//	}
		//	return std::tuple(e, sa, numberOfBytesTransferred);
		//}
		/*
		[[nodiscard]] auto recv_from(std::byte* buffer, std::size_t size) noexcept { return socket_recv_from_operation<socket>(*this, buffer, size); }
		[[nodiscard]] auto send_to(const SocketAddress& destination, std::byte* buffer, std::size_t size) noexcept { return socket_send_to_operation<socket>(*this, destination, buffer, size); }*/

		explicit socket(SOCKET handle, IOCONTEXT& ioSvc) noexcept : shandle(handle), ios(ioSvc) {}
		explicit socket(IOCONTEXT& ioSvc) noexcept : shandle(INVALID_SOCKET), ios(ioSvc) {}
	private:
		template<typename>friend class async_acceptor;
		template<typename>friend class io_service;
#if _WIN32

		SOCKET shandle;
		IOCONTEXT& ios;
#endif
	};
	template<typename IOCONTEXT>std::tuple<StatusCode, socket<IOCONTEXT>> create_socket(IOCONTEXT& ioSvc, SocketType sockettype = SocketType::TCP, AddressFamily family = AddressFamily::IPV4) {
		auto protocol = sockettype == SocketType::TCP ? IPPROTO_TCP : IPPROTO_UDP;

		auto socketHandle = ::socket(family, sockettype, protocol);
		if (socketHandle == INVALID_SOCKET) {
			return std::tuple(Impl::TranslateError(), socket(ioSvc));
		}

		if (sockettype == SocketType::TCP) {
			BOOL value = TRUE;
			if (::setsockopt(socketHandle, SOL_SOCKET, SO_DONTLINGER, reinterpret_cast<const char*>(&value), sizeof(value)) == SOCKET_ERROR) {
				closesocket(socketHandle);
				return std::tuple(Impl::TranslateError(), socket(ioSvc));
			}
		}			
		if (::CreateIoCompletionPort((HANDLE)socketHandle, ioSvc.IOCPHandle.handle(), 0, 0) == NULL) {
			return std::tuple(Impl::TranslateError(), socket(ioSvc));
		}
		return std::tuple(StatusCode::SC_SUCCESS, socket(socketHandle, ioSvc));
	}
} // namespace SL::Network

#endif
