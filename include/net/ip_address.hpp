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

/// Represents an IPv4 or IPv6 address.
///
/// IPAddress supports equality comparison and can be used directly
/// as a key in standard unordered containers such as
/// std::unordered_map and std::unordered_set.
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

    [[nodiscard]]
    bool operator==(const IPAddress& other) const noexcept = default;

    [[nodiscard]]
    bool operator!=(const IPAddress& other) const noexcept = default;

    [[nodiscard]]
    std::size_t Hash() const noexcept;

private:
    IPVersion m_version{ IPVersion::IPv4 };
    std::array<std::uint8_t, 16> m_bytes{};
};

namespace std
{

template <>
struct hash<IPAddress>
{
    std::size_t operator()(const IPAddress& address) const noexcept
    {
        return address.Hash();
    }
};

}

#endif // IP_ADDRESS_HPP
