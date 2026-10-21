#pragma once

#include <string>
#include <system_error>

#include "net/platform.hpp"

// Lightweight, header-only helper for retrieving and formatting the last
// socket-related error in a platform-independent way.
class SocketError final
{
public:
    constexpr SocketError() noexcept = default;

    explicit SocketError(int code) noexcept
        : m_ec(code, std::system_category())
    {
    }

    explicit SocketError(std::error_code ec) noexcept
        : m_ec(ec)
    {
    }

    [[nodiscard]]
    int Code() const noexcept
    {
        return m_ec.value();
    }

    [[nodiscard]]
    const std::error_code& ErrorCode() const noexcept
    {
        return m_ec;
    }

    [[nodiscard]]
    std::string Message() const
    {
        // std::system_category().message() delegates to FormatMessageA on Windows 
        // and strerror_r (or equivalent) on POSIX.
        return m_ec.message();
    }

    // Checking against std::errc allows cross-platform comparison,
    // as std::system_category maps WSAE* and errno codes to generic std::errc conditions.
    [[nodiscard]]
    bool IsWouldBlock() const noexcept
    {
        return (m_ec == std::errc::operation_would_block) ||
            (m_ec == std::errc::resource_unavailable_try_again);
    }

    [[nodiscard]]
    bool IsInterrupted() const noexcept
    {
        return (m_ec == std::errc::interrupted);
    }

    [[nodiscard]]
    bool IsConnectionReset() const noexcept
    {
        return (m_ec == std::errc::connection_reset);
    }

    [[nodiscard]]
    bool IsConnectionRefused() const noexcept
    {
        return (m_ec == std::errc::connection_refused);
    }

    [[nodiscard]]
    bool IsTimedOut() const noexcept
    {
        return (m_ec == std::errc::timed_out);
    }

    [[nodiscard]]
    bool IsAddressInUse() const noexcept
    {
        return (m_ec == std::errc::address_in_use);
    }

    // Temporary network issues where retrying immediately or after a delay makes sense
    [[nodiscard]]
    bool IsTransient() const noexcept
    {
        return (IsWouldBlock() || IsInterrupted() || IsTimedOut());
    }

    // Disconnections that signal the peer is gone
    [[nodiscard]]
    bool IsDisconnected() const noexcept
    {
        return IsConnectionReset() || (m_ec == std::errc::broken_pipe);
    }

    [[nodiscard]]
    static SocketError Last() noexcept
    {
#ifdef _WIN32
        return SocketError{ ::WSAGetLastError() };
#else
        return SocketError{ errno };
#endif
    }

    // Implicit conversion to bool for checking success/failure
    explicit operator bool() const noexcept
    {
        return static_cast<bool>(m_ec);
    }

    // --- SocketError vs. SocketError ---
    [[nodiscard]]
    friend bool operator==(const SocketError& lhs, const SocketError& rhs) noexcept
    {
        return lhs.m_ec == rhs.m_ec;
    }

    [[nodiscard]]
    friend bool operator!=(const SocketError& lhs, const SocketError& rhs) noexcept
    {
        return !(lhs == rhs);
    }

    // --- SocketError vs. std::errc (Cross-Platform) ---
    [[nodiscard]]
    friend bool operator==(const SocketError& lhs, std::errc rhs) noexcept
    {
        return lhs.m_ec == rhs;
    }

    [[nodiscard]]
    friend bool operator!=(const SocketError& lhs, std::errc rhs) noexcept
    {
        return !(lhs == rhs);
    }

    // --- SocketError vs. Raw Code (int) ---
    [[nodiscard]]
    friend bool operator==(const SocketError& lhs, int rhs) noexcept
    {
        return lhs.Code() == rhs;
    }

    [[nodiscard]]
    friend bool operator!=(const SocketError& lhs, int rhs) noexcept
    {
        return !(lhs == rhs);
    }

    // --- SocketError vs. std::error_code ---
    [[nodiscard]]
    friend bool operator==(const SocketError& lhs, std::error_code rhs) noexcept
    {
        return lhs.m_ec == rhs;
    }

    [[nodiscard]]
    friend bool operator!=(const SocketError& lhs, std::error_code rhs) noexcept
    {
        return !(lhs == rhs);
    }

private:
    std::error_code m_ec{};
};

