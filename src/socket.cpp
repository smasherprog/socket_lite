#include "socket.h"
#include "io_service.h"

#if _WIN32

namespace SL::Network {

	namespace local {
		SOCKET create_socket(int addressFamily, int socketType, int protocol, HANDLE ioCompletionPort)
		{
			// Enumerate available protocol providers for the specified socket type.

			WSAPROTOCOL_INFOW stackInfos[4];
			std::unique_ptr<WSAPROTOCOL_INFOW[]> heapInfos;
			WSAPROTOCOL_INFOW* selectedProtocolInfo = nullptr;

			{
				INT protocols[] = { protocol, 0 };
				DWORD bufferSize = sizeof(stackInfos);
				WSAPROTOCOL_INFOW* infos = stackInfos;

				int protocolCount = ::WSAEnumProtocolsW(protocols, infos, &bufferSize);
				if (protocolCount == SOCKET_ERROR) {
					int errorCode = ::WSAGetLastError();
					if (errorCode == WSAENOBUFS) {
						DWORD requiredElementCount = bufferSize / sizeof(WSAPROTOCOL_INFOW);
						heapInfos = std::make_unique<WSAPROTOCOL_INFOW[]>(requiredElementCount);
						bufferSize = requiredElementCount * sizeof(WSAPROTOCOL_INFOW);
						infos = heapInfos.get();
						protocolCount = ::WSAEnumProtocolsW(protocols, infos, &bufferSize);
						if (protocolCount == SOCKET_ERROR) {
							errorCode = ::WSAGetLastError();
						}
					}

					if (protocolCount == SOCKET_ERROR) {
						THROWEXCEPTIONWCODE(errorCode)
					}
				}

				if (protocolCount == 0) {
					throw std::system_error(std::make_error_code(std::errc::protocol_not_supported));
				}

				for (int i = 0; i < protocolCount; ++i) {
					auto& info = infos[i];
					if (info.iAddressFamily == addressFamily && info.iProtocol == protocol && info.iSocketType == socketType) {
						selectedProtocolInfo = &info;
						break;
					}
				}

				if (selectedProtocolInfo == nullptr) {
					throw std::system_error(std::make_error_code(std::errc::address_family_not_supported));
				}
			}

			// WSA_FLAG_NO_HANDLE_INHERIT for SDKs earlier than Windows 7.
			constexpr DWORD flagNoInherit = 0x80;

			const DWORD flags = WSA_FLAG_OVERLAPPED | flagNoInherit;

			auto socketHandle = ::WSASocketW(addressFamily, socketType, protocol, selectedProtocolInfo, 0, flags);
			if (socketHandle == INVALID_SOCKET) {
				THROWEXCEPTION
			}

			// This is needed on operating systems earlier than Windows 7 to prevent
			// socket handles from being inherited. On Windows 7 or later this is
			// redundant as the WSA_FLAG_NO_HANDLE_INHERIT flag passed to creation
			// above causes the socket to be atomically created with this flag cleared.
			if (!::SetHandleInformation((HANDLE)socketHandle, HANDLE_FLAG_INHERIT, 0)) {
				THROWEXCEPTION
			}

			// Associate the socket with the I/O completion port.
			{
				const HANDLE result = ::CreateIoCompletionPort((HANDLE)socketHandle, ioCompletionPort, ULONG_PTR(0), DWORD(0));
				if (result == nullptr) {
					THROWEXCEPTION
				}
			}

			if (socketType == SOCK_STREAM) {
				// Turn off linger so that the destructor doesn't block while closing
				// the socket or silently continue to flush remaining data in the
				// background after ::closesocket() is called, which could fail and
				// we'd never know about it.
				// We expect clients to call Disconnect() or use CloseSend() to cleanly
				// shut-down connections instead.
				BOOL value = TRUE;
				const int result = ::setsockopt(socketHandle, SOL_SOCKET, SO_DONTLINGER, reinterpret_cast<const char*>(&value), sizeof(value));
				if (result == SOCKET_ERROR) {
					THROWEXCEPTION
				}
			}
			if (SetFileCompletionNotificationModes(reinterpret_cast<HANDLE>(socketHandle), FILE_SKIP_COMPLETION_PORT_ON_SUCCESS) == FALSE) {
				THROWEXCEPTION
			}

			return socketHandle;
		}
	} // namespace local

	socket socket::create(io_service& ioSvc, SocketType sockettype, AddressFamily family) {
		return socket(local::create_socket(family, sockettype, sockettype == SocketType::TCP ? IPPROTO_TCP : IPPROTO_UDP, ioSvc.getHandle()));
	}

} // namespace SL::Network

#endif