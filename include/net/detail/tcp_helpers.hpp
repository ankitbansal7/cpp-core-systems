#pragma once

#include "net/socket_endpoint.hpp"
#include "net/socket_handle.hpp"
#include "net/socket_result.hpp"

namespace net_detail
{
    inline SocketError InvalidSocket() noexcept
    {
        return SocketError{ std::make_error_code(std::errc::bad_file_descriptor) };
    }

    inline SocketError InvalidArgument() noexcept
    {
        return SocketError{ std::make_error_code(std::errc::invalid_argument) };
    }

    // Linux uses MSG_NOSIGNAL in Send. BSD/macOS use this per-socket option.
    inline SocketResult<void> ConfigureTcpSocket(SocketHandle::NativeHandle handle) noexcept
    {
#if !defined(_WIN32) && defined(SO_NOSIGPIPE) && !defined(MSG_NOSIGNAL)
        const int enabled = 1;
        if (::setsockopt(handle, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled)) != 0)
        {
            return SocketResult<void>::Failure(SocketError::Last());
        }
#else
        (void)handle;
#endif
        return SocketResult<void>::Success();
    }

    inline SocketResult<SocketHandle> CreateTcpSocket(IPVersion version) noexcept
    {
        if (version != IPVersion::IPv4 && version != IPVersion::IPv6)
        {
            return SocketResult<SocketHandle>::Failure(InvalidArgument());
        }
        const int family = version == IPVersion::IPv4 ? AF_INET : AF_INET6;
        SocketHandle handle{ ::socket(family, SOCK_STREAM, IPPROTO_TCP) };
        if (!handle)
        {
            return SocketResult<SocketHandle>::Failure(SocketError::Last());
        }
        const auto configured = ConfigureTcpSocket(handle.Get());
        if (!configured)
        {
            return SocketResult<SocketHandle>::Failure(configured.Error());
        }
        return SocketResult<SocketHandle>::Success(std::move(handle));
    }

    inline SocketResult<SocketEndpoint> QueryEndpoint(
        SocketHandle::NativeHandle handle, bool peer) noexcept
    {
        if (handle == SocketHandle::InvalidHandle)
        {
            return SocketResult<SocketEndpoint>::Failure(InvalidSocket());
        }
        sockaddr_storage storage{};
        socklen_t length = sizeof(storage);
        auto* address = reinterpret_cast<sockaddr*>(&storage);
        const int result = peer ? ::getpeername(handle, address, &length)
            : ::getsockname(handle, address, &length);
        if (result != 0)
        {
            return SocketResult<SocketEndpoint>::Failure(SocketError::Last());
        }
        const auto endpoint = SocketEndpoint::FromNative(address, length);
        if (!endpoint)
        {
            return SocketResult<SocketEndpoint>::Failure(InvalidArgument());
        }
        return SocketResult<SocketEndpoint>::Success(*endpoint);
    }
}
