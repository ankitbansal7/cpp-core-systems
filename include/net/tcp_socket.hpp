#pragma once

#include "net/detail/tcp_helpers.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <span>
#include <utility>

enum class SocketShutdown
{
    Receive,
    Send,
    Both
};

class TcpListener;

// Blocking TCP byte stream. Keep SocketRuntime alive longer than this object.
// Move-only; concurrent mutation/Close of the same object is not supported.
class TcpSocket final
{
public:
    TcpSocket() noexcept = default;
    TcpSocket(const TcpSocket&) = delete;
    TcpSocket& operator=(const TcpSocket&) = delete;
    TcpSocket(TcpSocket&&) noexcept = default;
    TcpSocket& operator=(TcpSocket&&) noexcept = default;

    [[nodiscard]] bool IsValid() const noexcept { return m_handle.IsValid(); }
    [[nodiscard]] explicit operator bool() const noexcept { return IsValid(); }

    // Borrowed handle for platform options; ownership stays with this object.
    [[nodiscard]] SocketHandle::NativeHandle GetNativeHandle() const noexcept { return m_handle.Get(); }

    // Creates and connects a fresh socket. On failure this object stays invalid;
    // a connected socket must be closed before connecting again. Interrupted
    // connect is returned as an error rather than retried on an uncertain socket.
    [[nodiscard]] SocketResult<void> Connect(const SocketEndpoint& endpoint) noexcept
    {
        if (IsValid())
        {
            return SocketResult<void>::Failure(
                SocketError{ std::make_error_code(std::errc::already_connected) });
        }
        auto created = net_detail::CreateTcpSocket(endpoint.GetVersion());
        if (!created)
        {
            return SocketResult<void>::Failure(created.Error());
        }
        auto handle = std::move(created).Value();
        const auto [storage, length] = endpoint.ToNative();
        if (::connect(handle.Get(), reinterpret_cast<const sockaddr*>(&storage), length) != 0)
        {
            return SocketResult<void>::Failure(SocketError::Last());
        }
        m_handle = std::move(handle);
        return SocketResult<void>::Success();
    }

    // One successful OS send; may transfer fewer bytes than requested. Loop
    // over the remaining bytes when the entire buffer must be transmitted.
    [[nodiscard]] SocketResult<std::size_t> Send(std::span<const std::byte> buffer) noexcept
    {
        if (!IsValid())
        {
            return SocketResult<std::size_t>::Failure(net_detail::InvalidSocket());
        }
        if (buffer.empty())
        {
            return SocketResult<std::size_t>::Success(0);
        }
        const auto length = TransferLength(buffer.size());
        int flags = 0;
#if !defined(_WIN32) && defined(MSG_NOSIGNAL)
        flags = MSG_NOSIGNAL;
#endif
        for (;;)
        {
            const auto count = ::send(m_handle.Get(), reinterpret_cast<const char*>(buffer.data()), length, flags);
            if (count >= 0)
            {
                return SocketResult<std::size_t>::Success(static_cast<std::size_t>(count));
            }
            const auto error = SocketError::Last();
            if (!error.IsInterrupted())
            {
                return SocketResult<std::size_t>::Failure(error);
            }
        }
    }

    // One successful OS receive; may fill only part of the buffer. Zero with
    // a nonempty buffer means the peer has gracefully closed its sending side.
    // An empty buffer is a successful no-op and does not probe peer closure.
    [[nodiscard]] SocketResult<std::size_t> Receive(std::span<std::byte> buffer) noexcept
    {
        if (!IsValid())
        {
            return SocketResult<std::size_t>::Failure(net_detail::InvalidSocket());
        }
        if (buffer.empty())
        {
            return SocketResult<std::size_t>::Success(0);
        }
        const auto length = TransferLength(buffer.size());
        for (;;)
        {
            const auto count = ::recv(m_handle.Get(), reinterpret_cast<char*>(buffer.data()), length, 0);
            if (count >= 0)
            {
                return SocketResult<std::size_t>::Success(static_cast<std::size_t>(count));
            }
            const auto error = SocketError::Last();
            if (!error.IsInterrupted())
            {
                return SocketResult<std::size_t>::Failure(error);
            }
        }
    }

    // Half-close Send to signal EOF while still receiving the peer's response.
    [[nodiscard]] SocketResult<void> Shutdown(SocketShutdown direction = SocketShutdown::Both) noexcept
    {
        if (!IsValid())
        {
            return SocketResult<void>::Failure(net_detail::InvalidSocket());
        }
        int how = 0;
        switch (direction)
        {
#ifdef _WIN32
        case SocketShutdown::Receive: how = SD_RECEIVE; break;
        case SocketShutdown::Send: how = SD_SEND; break;
        case SocketShutdown::Both: how = SD_BOTH; break;
#else
        case SocketShutdown::Receive: how = SHUT_RD; break;
        case SocketShutdown::Send: how = SHUT_WR; break;
        case SocketShutdown::Both: how = SHUT_RDWR; break;
#endif
        default: return SocketResult<void>::Failure(net_detail::InvalidArgument());
        }
        if (::shutdown(m_handle.Get(), how) != 0)
        {
            return SocketResult<void>::Failure(SocketError::Last());
        }
        return SocketResult<void>::Success();
    }

    [[nodiscard]] SocketResult<SocketEndpoint> GetLocalEndpoint() const noexcept
    {
        return net_detail::QueryEndpoint(m_handle.Get(), false);
    }

    [[nodiscard]] SocketResult<SocketEndpoint> GetPeerEndpoint() const noexcept
    {
        return net_detail::QueryEndpoint(m_handle.Get(), true);
    }

    void Close() noexcept { m_handle.Close(); }

private:
    friend class TcpListener;
    explicit TcpSocket(SocketHandle handle) noexcept : m_handle(std::move(handle)) {}

    // Winsock accepts int lengths. The same cap on POSIX avoids narrowing and
    // keeps each operation within the portable API's range.
    static int TransferLength(std::size_t length) noexcept
    {
        return static_cast<int>((std::min)(length,
            static_cast<std::size_t>((std::numeric_limits<int>::max)())));
    }

    SocketHandle m_handle;
};
