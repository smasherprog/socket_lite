#ifndef SL_NETWORK_SOCKET_IO_EVENTS
#define SL_NETWORK_SOCKET_IO_EVENTS

#include "impl.h"
#include "error_handling.h"

namespace SL::Network {

	enum class OP_Type : unsigned char { OnConnect, OnAccept, OnSend, OnRead, OnReadFrom, OnSendTo };
	class overlapped_operation : public WSAOVERLAPPED {
	private:
		std::atomic<StatusCode> errorCode;
	public:
		OP_Type OpType;
		overlapped_operation(OP_Type o) :OpType(o), WSAOVERLAPPED({ 0 }), Socket(INVALID_SOCKET) {
			errorCode.store(StatusCode::SC_UNSET, std::memory_order::memory_order_relaxed);
		}
		overlapped_operation(OP_Type o, SOCKET s) :OpType(o), WSAOVERLAPPED({ 0 }), Socket(s) {
			errorCode.store(StatusCode::SC_UNSET, std::memory_order::memory_order_relaxed);
		}

		StatusCode trysetstatus(StatusCode code, StatusCode expected) {
			auto originalexpected = expected;
			while (!errorCode.compare_exchange_weak(expected, code, std::memory_order::memory_order_relaxed) && expected == originalexpected);
			return expected;
		}
		void setstatus(StatusCode code) {
			errorCode.store(code, std::memory_order::memory_order_relaxed);
		}
		StatusCode getstatus() {
			return errorCode.load(std::memory_order::memory_order_relaxed);
		}
		StatusCode exchangestatus(StatusCode code) {
			return errorCode.exchange(code, std::memory_order::memory_order_relaxed);
		}

		WSAOVERLAPPED* getOverlappedStruct() { return reinterpret_cast<WSAOVERLAPPED*>(this); }
		SOCKET Socket;
	};

	template<typename ONCONNECT, typename ONACCEPT, typename ONRECV, typename ONSEND> struct IO_Events {
		IO_Events(ONCONNECT&& onconnect, ONACCEPT&& onaccept, ONRECV&& onrecv, ONSEND&& onsend) :
			OnConnect(onconnect),
			OnAccept(onaccept),
			OnRecv(onrecv),
			OnSend(onsend)
		{}

		ONCONNECT OnConnect;
		ONACCEPT OnAccept;
		ONRECV OnRecv;
		ONSEND OnSend;
	};
} // namespace SL::Network

#endif
