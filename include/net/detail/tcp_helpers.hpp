#pragma once

#include "net/detail/socket_helpers.hpp"

namespace net::detail
{
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
        auto created = CreateSocket(version, SOCK_STREAM, IPPROTO_TCP);
        if (!created)
        {
            return created;
        }
        auto handle = std::move(created).Value();
        const auto configured = ConfigureTcpSocket(handle.Get());
        if (!configured)
        {
            return SocketResult<SocketHandle>::Failure(configured.Error());
        }
        return SocketResult<SocketHandle>::Success(std::move(handle));
    }
}
