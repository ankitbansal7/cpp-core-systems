#pragma once

#include "net/socket_endpoint.hpp"
#include "net/socket_handle.hpp"
#include "net/socket_result.hpp"

namespace net::detail
{
    inline SocketError InvalidSocket() noexcept
    {
        return SocketError{ std::make_error_code(std::errc::bad_file_descriptor) };
    }

    inline SocketError InvalidArgument() noexcept
    {
        return SocketError{ std::make_error_code(std::errc::invalid_argument) };
    }

    inline SocketResult<SocketHandle> CreateSocket(IPVersion version, int type, int protocol) noexcept
    {
        if (version != IPVersion::IPv4 && version != IPVersion::IPv6)
        {
            return SocketResult<SocketHandle>::Failure(InvalidArgument());
        }
        const int family = version == IPVersion::IPv4 ? AF_INET : AF_INET6;
        SocketHandle handle{ ::socket(family, type, protocol) };
        if (!handle)
        {
            return SocketResult<SocketHandle>::Failure(SocketError::Last());
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
