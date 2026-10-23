// Return values and errors together, including move-only handles and void.
// Binds only to IPv4 loopback on an OS-selected port; no connections or traffic.
#include "net/socket_endpoint.hpp"
#include "net/socket_handle.hpp"
#include "net/socket_result.hpp"
#include "net/socket_runtime.hpp"

#include <iostream>
#include <utility>

namespace
{
    SocketResult<SocketHandle> MakeTcpSocket(int family = AF_INET) noexcept
    {
        SocketHandle handle{ ::socket(family, SOCK_STREAM, IPPROTO_TCP) };
        if (!handle)
        {
            return SocketResult<SocketHandle>::Failure(SocketError::Last());
        }
        return SocketResult<SocketHandle>::Success(std::move(handle));
    }

    SocketResult<void> Bind(const SocketHandle& handle, const SocketEndpoint& endpoint) noexcept
    {
        const auto [storage, length] = endpoint.ToNative();
        if (::bind(handle.Get(), reinterpret_cast<const sockaddr*>(&storage), length) != 0)
        {
            return SocketResult<void>::Failure(SocketError::Last());
        }
        return SocketResult<void>::Success();
    }
}

int main()
{
    SocketRuntime runtime;
    if (!runtime)
    {
        std::cerr << "Runtime initialization failed: " << runtime.GetError() << '\n';
        return 1;
    }

    auto created = MakeTcpSocket();
    if (!created)
    {
        std::cerr << "Socket creation failed: " << created.Error() << '\n';
        return 1;
    }
    // Always check success before Value(). Move out to transfer ownership.
    SocketHandle handle = std::move(created).Value();
    std::cout << "Socket ownership extracted from result\n";

    const SocketEndpoint endpoint{ IPAddress::Loopback(IPVersion::IPv4), 0 };
    const auto bound = Bind(handle, endpoint);
    if (!bound)
    {
        std::cerr << "Bind failed: " << bound.Error() << '\n';
        return 1;
    }
    bound.Value(); // For void, this just asserts that the operation succeeded.
    std::cout << "Bound to IPv4 loopback on an OS-selected port\n";

    const auto failed = MakeTcpSocket(-1);
    if (failed)
    {
        std::cerr << "Expected unsupported address family to fail\n";
        return 1;
    }
    std::cout << "Expected failure returned as a result: " << failed.Error() << '\n';
    // handle closes before runtime is destroyed.
    return 0;
}
