// A complete, local TCP exchange in one process. The request is deliberately
// small so it can be queued before the other endpoint reads; a general server
// would service connections concurrently or use an event loop.
#include "net/socket_runtime.hpp"
#include "net/tcp_listener.hpp"

#include <array>
#include <iostream>
#include <span>
#include <string_view>
#include <utility>

namespace
{
    SocketResult<void> SendAll(TcpSocket& socket, std::span<const std::byte> bytes) noexcept
    {
        while (!bytes.empty())
        {
            const auto sent = socket.Send(bytes);
            if (!sent)
            {
                return SocketResult<void>::Failure(sent.Error());
            }
            if (sent.Value() == 0)
            {
                return SocketResult<void>::Failure(
                    SocketError{ std::make_error_code(std::errc::io_error) });
            }
            bytes = bytes.subspan(sent.Value());
        }
        return SocketResult<void>::Success();
    }

    bool Check(const SocketResult<void>& result, const char* operation)
    {
        if (!result)
        {
            std::cerr << operation << " failed: " << result.Error() << '\n';
            return false;
        }
        return true;
    }
}

int main()
{
    // Runtime must be destroyed last, after every socket and the listener.
    SocketRuntime runtime;
    if (!runtime)
    {
        std::cerr << "Runtime initialization failed: " << runtime.GetError() << '\n';
        return 1;
    }

    TcpListener listener;
    if (!Check(listener.Bind({ IPAddress::Loopback(IPVersion::IPv4), 0 }), "Bind") ||
        !Check(listener.Listen(), "Listen"))
    {
        return 1;
    }
    const auto endpoint = listener.GetLocalEndpoint();
    if (!endpoint)
    {
        std::cerr << "GetLocalEndpoint failed: " << endpoint.Error() << '\n';
        return 1;
    }
    std::cout << "Listening at " << endpoint.Value().ToString() << '\n';

    TcpSocket client;
    if (!Check(client.Connect(endpoint.Value()), "Connect"))
    {
        return 1;
    }
    auto accepted = listener.Accept();
    if (!accepted)
    {
        std::cerr << "Accept failed: " << accepted.Error() << '\n';
        return 1;
    }
    TcpSocket server = std::move(accepted).Value();
    listener.Close(); // The accepted connection remains usable.

    constexpr std::string_view message = "Hello from the TCP client!";
    const auto request = std::as_bytes(std::span{ message.data(), message.size() });
    if (!Check(SendAll(client, request), "Send request") ||
        !Check(client.Shutdown(SocketShutdown::Send), "Finish request"))
    {
        return 1;
    }

    std::array<std::byte, 64> buffer{};
    for (;;)
    {
        const auto received = server.Receive(buffer);
        if (!received)
        {
            std::cerr << "Server receive failed: " << received.Error() << '\n';
            return 1;
        }
        if (received.Value() == 0)
        {
            break; // Client half-closed: the complete request has arrived.
        }
        if (!Check(SendAll(server, std::span{buffer}.first(received.Value())), "Echo response"))
        {
            return 1;
        }
    }
    if (!Check(server.Shutdown(SocketShutdown::Send), "Finish response"))
    {
        return 1;
    }

    std::size_t echoed = 0;
    bool matches = true;
    std::cout << "Client received: ";
    for (;;)
    {
        const auto received = client.Receive(buffer);
        if (!received)
        {
            std::cerr << "Client receive failed: " << received.Error() << '\n';
            return 1;
        }
        if (received.Value() == 0)
        {
            break;
        }
        const auto* data = reinterpret_cast<const char*>(buffer.data());
        std::cout.write(data, static_cast<std::streamsize>(received.Value()));
        const std::string_view chunk{ data, received.Value() };
        matches = matches && echoed <= message.size() && chunk == message.substr(echoed, chunk.size());
        echoed += received.Value();
    }
    std::cout << '\n';
    if (!matches || echoed != message.size())
    {
        std::cerr << "Echo did not match the request\n";
        return 1;
    }
    // Both TcpSockets close automatically before SocketRuntime is destroyed.
    return 0;
}
