#ifndef SL_NETWORK_IO_SERVICE
#define SL_NETWORK_IO_SERVICE

#include <cstdint> 
#include "impl.h"
#include "error_handling.h"
#include "socket_address.h"
#include "socket_io_events.h"
#include <deque>

namespace SL::Network {
#if _WIN32
	namespace win32 {
		LPFN_DISCONNECTEX DisconnectEx_ = nullptr;
		LPFN_CONNECTEX ConnectEx_ = nullptr;
		LPFN_ACCEPTEX AcceptEx_ = nullptr;
		std::uint8_t addressBuffer[(sizeof(SOCKADDR_STORAGE) + 16) * 2] = { 0 };
	}
#endif

	template<typename IOEVENTS>class io_service {
		IOEVENTS IOEvents;
		std::uint8_t addressBuffer[(sizeof(SOCKADDR_STORAGE) + 16) * 2];
		LPFN_CONNECTEX ConnectEx_;
		LPFN_ACCEPTEX AcceptEx_;
		std::atomic_flag AcceptLBufferLock = ATOMIC_FLAG_INIT;
		std::deque<accept_overlapped_operation*> AcceptBuffer;
		std::atomic_flag IOBufferLock = ATOMIC_FLAG_INIT;
		std::deque<overlapped_operation*> IOBuffer;

	public:

		io_service(IOEVENTS&& ioevents, std::uint32_t concurrencyHint = 1) :IOEvents(ioevents), KeepGoing(true)
		{
#if _WIN32
			WSADATA winsockData;
			if (WSAStartup(MAKEWORD(2, 2), &winsockData) == SOCKET_ERROR) {
				assert(false);
			}
			IOCPHandle = Impl::safe_handle(CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, concurrencyHint));
			if (!IOCPHandle) {
				assert(false);
			}
			auto temphandle = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
			GUID guid = WSAID_ACCEPTEX;
			DWORD bytes = 0;
			WSAIoctl(temphandle, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &AcceptEx_, sizeof(AcceptEx_), &bytes, NULL,
				NULL);
			assert(AcceptEx_ != nullptr);

			guid = WSAID_CONNECTEX;
			bytes = 0;
			ConnectEx_ = nullptr;
			WSAIoctl(temphandle, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &ConnectEx_, sizeof(ConnectEx_), &bytes, NULL, NULL);
			assert(ConnectEx_ != nullptr);
			closesocket(temphandle);
#endif 
		}

		~io_service()
		{
#if _WIN32
			WSACleanup();
#endif
			while (!IOBuffer.empty()) {
				auto p = IOBuffer.back();
				IOBuffer.pop_back();
				delete p;
			}
			while (!AcceptBuffer.empty()) {
				auto p = AcceptBuffer.back();
				AcceptBuffer.pop_back();
				delete p;
			} 
		}

		io_service(io_service&& other) = delete;
		io_service(const io_service& other) = delete;
		io_service& operator=(io_service&& other) = delete;
		io_service& operator=(const io_service& other) = delete;

		overlapped_operation* alloc_o() {
			overlapped_operation* obj = nullptr;
			while (IOBufferLock.test_and_set(std::memory_order_acquire));
			if (IOBuffer.empty()) {
				IOBufferLock.clear(std::memory_order_release);
				obj = new overlapped_operation();
			}
			else { 
				obj = IOBuffer.back();
				IOBuffer.pop_back();
				IOBufferLock.clear(std::memory_order_release);
			} 
			return obj;
		}

		accept_overlapped_operation* alloc_a() {
			accept_overlapped_operation* obj = nullptr; 
			while (AcceptLBufferLock.test_and_set(std::memory_order_acquire));
			if (AcceptBuffer.empty()) {
				AcceptLBufferLock.clear(std::memory_order_release);
				obj = new accept_overlapped_operation();
			}
			else {
				obj = AcceptBuffer.back();
				AcceptBuffer.pop_back();
				AcceptLBufferLock.clear(std::memory_order_release);
			} 
			return obj;
		}

		void free_o(overlapped_operation* obj) {
			while (IOBufferLock.test_and_set(std::memory_order_acquire));
			IOBuffer.push_back(obj);
			IOBufferLock.clear(std::memory_order_release);
		}

		void free_a(accept_overlapped_operation* obj) {
			while (AcceptLBufferLock.test_and_set(std::memory_order_acquire));
			AcceptBuffer.push_back(obj);
			AcceptLBufferLock.clear(std::memory_order_release);
		}

		void run() {
			while (true) {
				DWORD numberOfBytesTransferred = 0;
				ULONG_PTR completionKey = 0;
				LPOVERLAPPED overlapped = nullptr;
				auto bSuccess = ::GetQueuedCompletionStatus(IOCPHandle.handle(), &numberOfBytesTransferred, &completionKey, &overlapped, INFINITE) == TRUE && KeepGoing;
				if (overlapped != nullptr) {
					auto state = reinterpret_cast<overlapped_operation*>(overlapped);
					auto status = bSuccess ? ERROR_SUCCESS : ::WSAGetLastError();
					auto e = Impl::TranslateError(status);
					auto originalvalue = state->exchangestatus(e);
					if (originalvalue == StatusCode::SC_PENDINGIO || originalvalue == StatusCode::SC_UNSET) {
						switch (state->OpType)
						{
						case OP_Type::OnConnect:
							if (e == StatusCode::SC_SUCCESS && ::setsockopt(state->Socket, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, nullptr, 0) == SOCKET_ERROR) {
								e = Impl::TranslateError();
							}
							IOEvents.OnConnect(*this, SL::Network::socket(state->Socket, *this), e);
							free_o(state);
							break;
						case OP_Type::OnAccept:
						{
							auto acceptstate = reinterpret_cast<accept_overlapped_operation*>(overlapped);
							if (e == StatusCode::SC_SUCCESS && ::setsockopt(acceptstate->Socket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (const char*)&(acceptstate->ListenSocket), sizeof(SOCKET)) == SOCKET_ERROR) {
								e = Impl::TranslateError();
							}
							IOEvents.OnAccept(*this, SL::Network::socket(state->Socket, *this), e, SL::Network::socket(acceptstate->ListenSocket, *this));
							free_a(acceptstate);
						}
						break;
						case SL::Network::OP_Type::OnSend:
							IOEvents.OnSend(*this, SL::Network::socket(state->Socket, *this), e, numberOfBytesTransferred);
							free_o(state);
							break;
						case SL::Network::OP_Type::OnRead:
							IOEvents.OnRecv(*this, SL::Network::socket(state->Socket, *this), e, numberOfBytesTransferred);
							free_o(state);
							break;
						case SL::Network::OP_Type::OnReadFrom:
							break;
						case SL::Network::OP_Type::OnSendTo:
							break;
						default:
							break;
						}
						refcounter.decOp();
					}
				}
				if (!KeepGoing && refcounter.getOpCount() == 0) {
					PostQueuedCompletionStatus(IOCPHandle.handle(), 0, (DWORD)NULL, NULL);
					return;
				}
			}
		}
		void stop()
		{
			KeepGoing = false;
			PostQueuedCompletionStatus(IOCPHandle.handle(), 0, (DWORD)NULL, NULL);
		}

	private:
		Impl::refcounter refcounter;
		Impl::safe_handle IOCPHandle;
#ifndef _WIN32 
		int EventWakeFd;
#endif
		bool KeepGoing;
		template<typename>friend class socket;
		template <typename T> friend std::tuple<StatusCode, socket<T>> create_socket(T& ioSvc, SocketType sockettype, AddressFamily family);
	};
}

#endif
