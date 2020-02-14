#ifndef SL_NETWORK_UTILS
#define SL_NETWORK_UTILS
#include <string>
#include <system_error>
#include <compare>
#include <experimental/coroutine>

namespace SL::Network {
#if _WIN32 
	namespace win32
	{
		using handle_t = void*;
		using ulongptr_t = std::uintptr_t;
		using longptr_t = std::intptr_t;
		using dword_t = unsigned long;
		using socket_t = std::uintptr_t;
		using ulong_t = unsigned long;

# pragma warning(push)
# pragma warning(disable : 4201) // Non-standard anonymous struct/union

		/// Structure needs to correspond exactly to the builtin
		/// _OVERLAPPED structure from Windows.h.
		struct overlapped
		{
			ulongptr_t Internal;
			ulongptr_t InternalHigh;
			union
			{
				struct
				{
					dword_t Offset;
					dword_t OffsetHigh;
				};
				void* Pointer;
			};
			handle_t hEvent;
		};

# pragma warning(pop)

		struct wsabuf
		{
			constexpr wsabuf() noexcept : len(0), buf(nullptr) {}
			constexpr wsabuf(void* ptr, std::size_t size) : len(size <= ulong_t(-1) ? ulong_t(size) : ulong_t(-1)), buf(static_cast<char*>(ptr)) {}
			ulong_t len;
			char* buf;
		};

		class safe_handle
		{
		public:
			safe_handle() : m_handle(nullptr) {}
			explicit safe_handle(handle_t handle) : m_handle(handle) {}
			safe_handle(const safe_handle& other) = delete;
			safe_handle(safe_handle&& other) noexcept : m_handle(other.m_handle) { other.m_handle = nullptr; }
			~safe_handle() { close(); }
			safe_handle& operator=(safe_handle handle) noexcept { swap(handle); return *this; }
			constexpr handle_t handle() const { return m_handle; }
			constexpr operator bool() const noexcept { return handle != nullptr; }
			/// Calls CloseHandle() and sets the handle to NULL.
			void close() noexcept;

			void swap(safe_handle& other) noexcept { std::swap(m_handle, other.m_handle); }
			constexpr std::weak_ordering operator<=>(safe_handle const& rhs) const = default;
		private:
			handle_t m_handle;
		};

		template<typename OPERATION>
		class overlapped_operation : public overlapped
		{
		public:
			overlapped_operation() noexcept {}
			bool await_ready() const noexcept { return false; }

			bool await_suspend(std::experimental::coroutine_handle<> awaitingCoroutine)
			{ 
				m_awaitingCoroutine = awaitingCoroutine;
				return static_cast<OPERATION*>(this)->try_start();
			}

			decltype(auto) await_resume()
			{
				return static_cast<OPERATION*>(this)->get_result();
			}

			dword_t m_errorCode;
			dword_t m_numberOfBytesTransferred;
			std::experimental::coroutine_handle<> m_awaitingCoroutine;
		};

		static void on_operation_completed(overlapped* ioState, dword_t errorCode, dword_t numberOfBytesTransferred) noexcept
		{
			auto* operation = static_cast<overlapped_operation*>(ioState);
			operation->m_errorCode = errorCode;
			operation->m_numberOfBytesTransferred = numberOfBytesTransferred;
			operation->m_awaitingCoroutine.resume();
		}

		std::string GetErrorMessage(dword_t errorMessageID);
		dword_t GetLastError();
	}
#endif
}
#if _WIN32 
#define THROWEXCEPTION auto errorcode = SL::Network::win32::GetLastError(); \
	throw std::system_error(errorcode, std::system_category(), SL::Network::win32::GetErrorMessage(errorcode)); 
#endif
#define THROWEXCEPTIONWCODE(e) throw std::system_error(e, std::system_category(), SL::Network::win32::GetErrorMessage(e)); 
#endif