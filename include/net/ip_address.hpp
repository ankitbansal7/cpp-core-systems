#ifndef IP_ADDRESS_HPP
#define IP_ADDRESS_HPP

#include <string>
#include <string_view>
#include <optional>
#include <array>
#include <cstdint>

enum class IPVersion
{
    IPv4,
    IPv6
};

class IPAddress
{
private:
    IPAddress() = default;

public:
    [[nodiscard]]
    static std::optional<IPAddress> Parse(std::string_view address);

    [[nodiscard]]
    static IPAddress Loopback(IPVersion version) noexcept;

    [[nodiscard]]
    static IPAddress Any(IPVersion version) noexcept;

    [[nodiscard]]
    IPVersion GetVersion() const noexcept;

    [[nodiscard]]
    std::string ToString() const;

private:
    IPVersion m_version{ IPVersion::IPv4 };
    std::array<std::uint8_t, 16> m_bytes{};
};

#endif // IP_ADDRESS_HPP
