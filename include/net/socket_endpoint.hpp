#pragma once

#include "net/platform.hpp"
#include "net/ip_address.hpp"

#include <cstdint>
#include <string>
#include <optional>
#include <cstring>
#include <array>
#include <utility>

class SocketEndpoint final
{
public:
    SocketEndpoint() noexcept = default;

    SocketEndpoint(const IPAddress& address, std::uint16_t port, std::uint32_t scopeId = 0) noexcept
        : m_address(address)
        , m_port(port)
        , m_scopeId(address.GetVersion() == IPVersion::IPv6 ? scopeId : 0)
    {
    }

    [[nodiscard]]
    const IPAddress& GetAddress() const noexcept
    {
        return m_address;
    }

    [[nodiscard]]
    std::uint16_t GetPort() const noexcept
    {
        return m_port;
    }

    [[nodiscard]]
    IPVersion GetVersion() const noexcept
    {
        return m_address.GetVersion();
    }

    [[nodiscard]]
    std::uint32_t GetScopeId() const noexcept
    {
        return m_scopeId;
    }

    [[nodiscard]]
    std::string ToString() const
    {
        if (m_address.GetVersion() == IPVersion::IPv6)
        {
            const auto scope = m_scopeId == 0 ? std::string{} : "%" + std::to_string(m_scopeId);
            return "[" + m_address.ToString() + scope + "]:" + std::to_string(m_port);
        }

        return m_address.ToString() + ":" + std::to_string(m_port);
    }

    // --- OS Interop Helpers ---

    // Populates a native sockaddr_storage and returns its byte size
    [[nodiscard]]
    std::pair<sockaddr_storage, socklen_t> ToNative() const noexcept
    {
        sockaddr_storage storage{};
        socklen_t length = 0;

        if (m_address.GetVersion() == IPVersion::IPv4)
        {
            auto* addr4 = reinterpret_cast<sockaddr_in*>(&storage);

            if (addr4 == nullptr)
            {
                return { storage, length };
            }

            addr4->sin_family = AF_INET;
            addr4->sin_port = htons(m_port);
            std::memcpy(&addr4->sin_addr, m_address.Bytes(), IPAddress::IPV4_BYTE_COUNT);
            length = sizeof(sockaddr_in);
        }
        else
        {
            auto* addr6 = reinterpret_cast<sockaddr_in6*>(&storage);

            if (addr6 == nullptr)
            {
                return { storage, length };
            }

            addr6->sin6_family = AF_INET6;
            addr6->sin6_port = htons(m_port);
            addr6->sin6_scope_id = m_scopeId;
            std::memcpy(&addr6->sin6_addr, m_address.Bytes(), IPAddress::IPV6_BYTE_COUNT);
            length = sizeof(sockaddr_in6);
        }

        return { storage, length };
    }

    // Constructs a SocketEndpoint from an OS sockaddr_storage (e.g., from accept() or getpeername())
    [[nodiscard]]
    static std::optional<SocketEndpoint> FromNative(const sockaddr* addr, socklen_t len)
    {
        if (addr == nullptr)
        {
            return std::nullopt;
        }

        if ((addr->sa_family == AF_INET) && (len >= static_cast<socklen_t>(sizeof(sockaddr_in))))
        {
            const auto* addr4 = reinterpret_cast<const sockaddr_in*>(addr);

            if (addr4 == nullptr)
            {
                return std::nullopt;
            }

            std::array<std::uint8_t, IPAddress::IPV4_BYTE_COUNT> bytes{};
            std::memcpy(bytes.data(), &addr4->sin_addr, bytes.size());

            if (auto ip = IPAddress::FromBytes(bytes, IPVersion::IPv4))
            {
                return SocketEndpoint{ *ip, ntohs(addr4->sin_port) };
            }
        }
        else if (addr->sa_family == AF_INET6 && len >= static_cast<socklen_t>(sizeof(sockaddr_in6)))
        {
            const auto* addr6 = reinterpret_cast<const sockaddr_in6*>(addr);

            if (addr6 == nullptr)
            {
                return std::nullopt;
            }

            std::array<std::uint8_t, IPAddress::IPV6_BYTE_COUNT> bytes{};
            std::memcpy(bytes.data(), &addr6->sin6_addr, bytes.size());

            if (auto ip = IPAddress::FromBytes(bytes, IPVersion::IPv6))
            {
                return SocketEndpoint{ *ip, ntohs(addr6->sin6_port), addr6->sin6_scope_id };
            }
        }

        return std::nullopt;
    }

    [[nodiscard]]
    bool operator==(const SocketEndpoint& other) const noexcept
    {
        return (m_address == other.m_address) && (m_port == other.m_port) && (m_scopeId == other.m_scopeId);
    }

    [[nodiscard]]
    bool operator!=(const SocketEndpoint& other) const noexcept
    {
        return !(*this == other);
    }

private:
    IPAddress m_address{ IPAddress::Any(IPVersion::IPv4) };
    std::uint16_t m_port{ 0 };
    std::uint32_t m_scopeId{ 0 };
};

