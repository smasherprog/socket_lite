#ifndef SL_NETWORK_IP4_ENDPOINTADDRESS
#define SL_NETWORK_IP4_ENDPOINTADDRESS

#include "ipv4_address.h"

#include <optional>
#include <string>
#include <string_view>
#include <compare>

namespace SL::Network {

	class ipv4_endpoint
	{
	public: 
		// Construct to 0.0.0.0:0
		constexpr ipv4_endpoint() noexcept : m_address(), m_port(0) {}
		constexpr explicit ipv4_endpoint(ipv4_address address, std::uint16_t port) noexcept : m_address(address), m_port(port) {}

		constexpr const ipv4_address& address() const noexcept { return m_address; }
		constexpr std::uint16_t port() const noexcept { return m_port; }

		std::string to_string() const;
		static std::optional<ipv4_endpoint> from_string(std::string_view string) noexcept;

		constexpr std::weak_ordering operator<=>(ipv4_endpoint const& rhs) const = default;

	private:

		ipv4_address m_address;
		std::uint16_t m_port;

	};
}

#endif
