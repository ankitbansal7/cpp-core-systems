#pragma once

#include "net/detail/socket_helpers.hpp"

#include <cstddef>
#include <limits>
#include <span>
#include <utility>

struct UdpReceiveResult
{
    std::size_t BytesReceived;
    SocketEndpoint Sender;
};

// Blocking, move-only UDP socket. Keep SocketRuntime alive until destruction.
// Each SendTo/ReceiveFrom transfers one datagram, including empty datagrams.
// Concurrent mutation or Close of the same object requires coordination.
class UdpSocket final
{
public:
    UdpSocket() noexcept = default;
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;
    UdpSocket(UdpSocket&&) noexcept = default;
    UdpSocket& operator=(UdpSocket&&) noexcept = default;

    [[nodiscard]] bool IsValid() const noexcept { return m_handle.IsValid(); }
    [[nodiscard]] explicit operator bool() const noexcept { return IsValid(); }

    // Borrowed handle for platform options; do not close it directly.
    [[nodiscard]] SocketHandle::NativeHandle GetNativeHandle() const noexcept { return m_handle.Get(); }

    // Creates and binds a socket. Port zero selects an available port.
    // Close an existing socket before rebinding. Failure leaves it invalid.
    [[nodiscard]] SocketResult<void> Bind(const SocketEndpoint& endpoint) noexcept
    {
        if (IsValid())
        {
            return SocketResult<void>::Failure(net_detail::InvalidArgument());
        }
        auto created = net_detail::CreateSocket(endpoint.GetVersion(), SOCK_DGRAM, IPPROTO_UDP);
        if (!created)
        {
            return SocketResult<void>::Failure(created.Error());
        }
        auto handle = std::move(created).Value();
        const auto [storage, length] = endpoint.ToNative();
        if (::bind(handle.Get(), reinterpret_cast<const sockaddr*>(&storage), length) != 0)
        {
            return SocketResult<void>::Failure(SocketError::Last());
        }
        m_handle = std::move(handle);
        return SocketResult<void>::Success();
    }

    // Sends the whole buffer as ONE datagram. Never split it into multiple
    // sends. If initially invalid, creates a socket for the destination family
    // and lets the OS bind an ephemeral port. Failed first sends leave it invalid.
    [[nodiscard]] SocketResult<std::size_t> SendTo(
        std::span<const std::byte> buffer, const SocketEndpoint& destination) noexcept
    {
        if (buffer.size() > MaxTransferLength())
        {
            return SocketResult<std::size_t>::Failure(MessageSizeError());
        }
        SocketHandle pending;
        if (!IsValid())
        {
            auto created = net_detail::CreateSocket(destination.GetVersion(), SOCK_DGRAM, IPPROTO_UDP);
            if (!created)
            {
                return SocketResult<std::size_t>::Failure(created.Error());
            }
            pending = std::move(created).Value();
        }
        const auto handle = IsValid() ? m_handle.Get() : pending.Get();
        const auto [storage, length] = destination.ToNative();
        // Winsock expects a nonnull pointer even for a zero-length datagram.
        const char empty = 0;
        const auto* data = buffer.empty() ? &empty : reinterpret_cast<const char*>(buffer.data());
        for (;;)
        {
            const auto count = ::sendto(handle, data, static_cast<int>(buffer.size()), 0,
                reinterpret_cast<const sockaddr*>(&storage), length);
            if (count < 0)
            {
                const auto error = SocketError::Last();
                if (error.IsInterrupted()) { continue; }
                return SocketResult<std::size_t>::Failure(error);
            }
            // UDP is atomic: a partial result must never be treated as a prefix
            // that the caller should retransmit as another datagram.
            if (static_cast<std::size_t>(count) != buffer.size())
            {
                return SocketResult<std::size_t>::Failure(MessageSizeError());
            }
            if (pending) { m_handle = std::move(pending); }
            return SocketResult<std::size_t>::Success(static_cast<std::size_t>(count));
        }
    }

    // Receives ONE datagram and its sender. Zero bytes is an empty message,
    // not EOF. Unlike TCP, an empty buffer still consumes the next datagram.
    // If too small, consumes/discards the datagram and returns message_size;
    // buffer contents on failure must not be interpreted as a complete message.
    [[nodiscard]] SocketResult<UdpReceiveResult> ReceiveFrom(std::span<std::byte> buffer) noexcept
    {
        if (!IsValid())
        {
            return SocketResult<UdpReceiveResult>::Failure(net_detail::InvalidSocket());
        }
        if (buffer.size() > MaxTransferLength())
        {
            return SocketResult<UdpReceiveResult>::Failure(MessageSizeError());
        }
        for (;;)
        {
            sockaddr_storage storage{};
            socklen_t length = sizeof(storage);
#ifdef _WIN32
            char empty = 0;
            auto* data = buffer.empty() ? &empty : reinterpret_cast<char*>(buffer.data());
            const auto count = ::recvfrom(m_handle.Get(), data, static_cast<int>(buffer.size()), 0,
                reinterpret_cast<sockaddr*>(&storage), &length);
#else
            iovec vector{ buffer.data(), buffer.size() };
            msghdr message{};
            message.msg_name = &storage;
            message.msg_namelen = length;
            message.msg_iov = &vector;
            message.msg_iovlen = 1;
            const auto count = ::recvmsg(m_handle.Get(), &message, 0);
            length = message.msg_namelen;
#endif
            if (count < 0)
            {
                const auto error = SocketError::Last();
                if (error.IsInterrupted()) { continue; }
                return SocketResult<UdpReceiveResult>::Failure(error);
            }
#ifndef _WIN32
            if ((message.msg_flags & MSG_TRUNC) != 0)
            {
                return SocketResult<UdpReceiveResult>::Failure(MessageSizeError());
            }
#endif
            const auto sender = SocketEndpoint::FromNative(reinterpret_cast<sockaddr*>(&storage), length);
            if (!sender)
            {
                return SocketResult<UdpReceiveResult>::Failure(net_detail::InvalidArgument());
            }
            return SocketResult<UdpReceiveResult>::Success(
                UdpReceiveResult{ static_cast<std::size_t>(count), *sender });
        }
    }

    [[nodiscard]] SocketResult<SocketEndpoint> GetLocalEndpoint() const noexcept
    {
        return net_detail::QueryEndpoint(m_handle.Get(), false);
    }

    void Close() noexcept { m_handle.Close(); }

private:
    static std::size_t MaxTransferLength() noexcept
    {
        return static_cast<std::size_t>((std::numeric_limits<int>::max)());
    }

    static SocketError MessageSizeError() noexcept
    {
        return SocketError{ std::make_error_code(std::errc::message_size) };
    }

    SocketHandle m_handle;
};
