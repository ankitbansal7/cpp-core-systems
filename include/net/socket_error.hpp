#pragma once

#include <string>
#include <system_error>
#include <ostream>

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
    static SocketError Success() noexcept
    {
        return SocketError{};
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

    [[nodiscard]]
    bool IsWouldBlock() const noexcept
    {
        return Matches(std::errc::operation_would_block) ||
            Matches(std::errc::resource_unavailable_try_again);
    }

    [[nodiscard]]
    bool IsInterrupted() const noexcept
    {
        return Matches(std::errc::interrupted);
    }

    [[nodiscard]]
    bool IsConnectionReset() const noexcept
    {
        return Matches(std::errc::connection_reset);
    }

    [[nodiscard]]
    bool IsConnectionRefused() const noexcept
    {
        return Matches(std::errc::connection_refused);
    }

    [[nodiscard]]
    bool IsTimedOut() const noexcept
    {
        return Matches(std::errc::timed_out);
    }

    [[nodiscard]]
    bool IsAddressInUse() const noexcept
    {
        return Matches(std::errc::address_in_use);
    }

    // Temporary network issues where retrying immediately or after a delay makes sense
    [[nodiscard]]
    bool IsTransient() const noexcept
    {
        return (IsWouldBlock() || IsInterrupted() || IsTimedOut());
    }

    [[nodiscard]]
    bool IsConnectionAborted() const noexcept
    {
        return Matches(std::errc::connection_aborted);
    }

    [[nodiscard]]
    bool IsDisconnected() const noexcept
    {
        return IsConnectionReset() ||
            IsConnectionAborted() ||
            Matches(std::errc::broken_pipe) ||
            Matches(std::errc::not_connected);
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

    // Returns true if an error is present (non-zero error code).
    [[nodiscard]]
    bool HasError() const noexcept
    {
        return static_cast<bool>(m_ec);
    }

    /// Returns true if the operation completed with no error (code 0).
    [[nodiscard]]
    bool IsSuccess() const noexcept
    {
        return !m_ec;
    }

    // Explicit conversion to bool matching std::error_code semantics:
    // true = ERROR, false = SUCCESS (no error).
    [[nodiscard]]
    explicit operator bool() const noexcept
    {
        return HasError();
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
        return lhs.Matches(rhs);
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

    friend std::ostream& operator<<(std::ostream& os, const SocketError& error)
    {
        return os << error.Message() << " (" << error.Code() << ")";
    }

private:
    [[nodiscard]]
    bool Matches(std::errc condition) const noexcept
    {
#ifdef _WIN32
        // MinGW does not consistently map Winsock system errors to std::errc.
        // Keep native codes intact and normalize only condition comparisons.
        if (m_ec.category() == std::system_category())
        {
            switch (m_ec.value())
            {
            case WSAEWOULDBLOCK:
                return condition == std::errc::operation_would_block ||
                    condition == std::errc::resource_unavailable_try_again;
            case WSAEINTR:
                return condition == std::errc::interrupted;
            case WSAECONNRESET:
                return condition == std::errc::connection_reset;
            case WSAECONNREFUSED:
                return condition == std::errc::connection_refused;
            case WSAETIMEDOUT:
                return condition == std::errc::timed_out;
            case WSAEADDRINUSE:
                return condition == std::errc::address_in_use;
            case WSAECONNABORTED:
                return condition == std::errc::connection_aborted;
            case WSAENOTCONN:
                return condition == std::errc::not_connected;
            case WSAEMSGSIZE:
                return condition == std::errc::message_size;
            default:
                break;
            }
        }
#endif
        return m_ec == condition;
    }

    std::error_code m_ec{};
};

