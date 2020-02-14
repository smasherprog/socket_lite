#ifndef SL_NETWORK_IP_ADDRESS
#define SL_NETWORK_IP_ADDRESS

#include "ipv4_address.h"
#include "ipv6_address.h" 
#include <cassert>
#include <optional>
#include <string>
#include <compare>

namespace SL::Network {

	class ip_address
	{
	public:

		constexpr ip_address() noexcept : m_family(family::ipv4), m_ipv4() {}
		constexpr ip_address(ipv4_address address) noexcept : m_family(family::ipv4), m_ipv4(address) {}
		constexpr ip_address(ipv6_address address) noexcept : m_family(family::ipv6), m_ipv6(address) {}

		constexpr bool is_ipv4() const noexcept { return m_family == family::ipv4; }
		constexpr bool is_ipv6() const noexcept { return m_family == family::ipv6; }

		constexpr const ipv4_address& to_ipv4() const { assert(is_ipv4()); return m_ipv4; }
		constexpr const ipv6_address& to_ipv6() const { assert(is_ipv6()); return m_ipv6; }

		constexpr const std::uint8_t* bytes() const noexcept { return is_ipv4() ? m_ipv4.bytes() : m_ipv6.bytes(); }

		std::string to_string() const { return is_ipv4() ? m_ipv4.to_string() : m_ipv6.to_string(); }
		static std::optional<ip_address> from_string(std::string_view string) noexcept;

		constexpr std::weak_ordering operator<=>(ip_address const& rhs) const { return (rhs < *this) <=> false; }

	private:

		enum class family
		{
			ipv4,
			ipv6
		};

		family m_family;

		union
		{
			ipv4_address m_ipv4;
			ipv6_address m_ipv6;
		};

	}; 
	std::optional<SL::Network::ip_address> SL::Network::ip_address::from_string(std::string_view string) noexcept
	{
		if (auto ipv4 = ipv4_address::from_string(string); ipv4)
		{
			return *ipv4;
		}

		if (auto ipv6 = ipv6_address::from_string(string); ipv6)
		{
			return *ipv6;
		}

		return std::nullopt;
	}

}

#endif