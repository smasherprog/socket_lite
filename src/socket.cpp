#include "socket.h"
#include "io_service.h"

#if _WIN32

namespace SL::Network { 
	std::tuple<StatusCode, socket> socket::create(io_service& ioSvc, SocketType sockettype, AddressFamily family) {
		auto addressFamily = family;
		auto socketType = sockettype;
		auto protocol = sockettype == SocketType::TCP ? IPPROTO_TCP : IPPROTO_UDP;
		auto ioCompletionPort = ioSvc.getHandle();

		WSAPROTOCOL_INFOW stackInfos[4]; 

		INT protocols[] = { protocol, 0 };
		DWORD bufferSize = sizeof(stackInfos);
		WSAPROTOCOL_INFOW* infos = stackInfos;

		int protocolCount = ::WSAEnumProtocolsW(protocols, infos, &bufferSize);
		if (protocolCount == SOCKET_ERROR) {
			int errorCode = ::WSAGetLastError();
			if (errorCode == WSAENOBUFS) {
				DWORD requiredElementCount = bufferSize / sizeof(WSAPROTOCOL_INFOW);
				auto heapInfos = std::make_unique<WSAPROTOCOL_INFOW[]>(requiredElementCount);
				bufferSize = requiredElementCount * sizeof(WSAPROTOCOL_INFOW);
				infos = heapInfos.get();
				protocolCount = ::WSAEnumProtocolsW(protocols, infos, &bufferSize);
				if (protocolCount == SOCKET_ERROR) {
					errorCode = ::WSAGetLastError();
				}
			}

			if (protocolCount == SOCKET_ERROR) {
				return std::tuple(TranslateError(errorCode), socket());
			}
		}

		if (protocolCount == 0) {
			return std::tuple(StatusCode::SC_NOTSUPPORTED, socket());
		}

		WSAPROTOCOL_INFOW* selectedProtocolInfo = nullptr;
		for (int i = 0; i < protocolCount; ++i) {
			auto& info = infos[i];
			if (info.iAddressFamily == addressFamily && info.iProtocol == protocol && info.iSocketType == socketType) {
				selectedProtocolInfo = &info;
				break;
			}
		}

		if (selectedProtocolInfo == nullptr) {
			return std::tuple(StatusCode::SC_NOTSUPPORTED, socket());
		}

		auto socketHandle = ::WSASocketW(addressFamily, socketType, protocol, selectedProtocolInfo, 0, WSA_FLAG_OVERLAPPED);
		if (socketHandle == INVALID_SOCKET) {
			return std::tuple(TranslateError(), socket());
		}

		if (::CreateIoCompletionPort((HANDLE)socketHandle, ioCompletionPort, ULONG_PTR(0), DWORD(0)) == nullptr) {
			closesocket(socketHandle);
			return std::tuple(TranslateError(), socket());
		}

		if (socketType == SOCK_STREAM) {
			// Turn off linger so that the destructor doesn't block while closing
			// the socket or silently continue to flush remaining data in the
			// background after ::closesocket() is called, which could fail and
			// we'd never know about it.
			// We expect clients to call Disconnect() or use CloseSend() to cleanly
			// shut-down connections instead.
			BOOL value = TRUE; 
			if (::setsockopt(socketHandle, SOL_SOCKET, SO_DONTLINGER, reinterpret_cast<const char*>(&value), sizeof(value)) == SOCKET_ERROR) {
				closesocket(socketHandle);
				return std::tuple(TranslateError(), socket());
			}
		}
		if (SetFileCompletionNotificationModes(reinterpret_cast<HANDLE>(socketHandle), FILE_SKIP_COMPLETION_PORT_ON_SUCCESS) == FALSE) {
			closesocket(socketHandle);
			return std::tuple(TranslateError(), socket());
		}

		return std::tuple(StatusCode::SC_SUCCESS, socket(socketHandle));
	}

} // namespace SL::Network

#endif