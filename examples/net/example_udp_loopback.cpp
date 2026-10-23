// A local UDP request/reply plus an empty datagram. No listener or accept step.
#include "net/socket_runtime.hpp"
#include "net/udp_socket.hpp"

#include <array>
#include <iostream>
#include <span>
#include <string_view>

namespace
{
    bool SetReceiveTimeout(UdpSocket& socket)
    {
#ifdef _WIN32
        const DWORD timeout = 5000;
        const auto* value = reinterpret_cast<const char*>(&timeout);
#else
        const timeval timeout{ 5, 0 };
        const auto* value = &timeout;
#endif
        if (::setsockopt(socket.GetNativeHandle(), SOL_SOCKET, SO_RCVTIMEO, value, sizeof(timeout)) != 0)
        {
            std::cerr << "Setting receive timeout failed: " << SocketError::Last() << '\n';
            return false;
        }
        return true;
    }
}

int main()
{
    SocketRuntime runtime; // Must outlive both sockets.
    if (!runtime)
    {
        std::cerr << "Runtime initialization failed: " << runtime.GetError() << '\n';
        return 1;
    }
    UdpSocket server;
    const auto bound = server.Bind({ IPAddress::Loopback(IPVersion::IPv4), 0 });
    if (!bound)
    {
        std::cerr << "Bind failed: " << bound.Error() << '\n';
        return 1;
    }
    if (!SetReceiveTimeout(server)) { return 1; }
    const auto endpoint = server.GetLocalEndpoint();
    if (!endpoint)
    {
        std::cerr << "GetLocalEndpoint failed: " << endpoint.Error() << '\n';
        return 1;
    }
    std::cout << "Server bound at " << endpoint.Value().ToString() << '\n';

    UdpSocket client;
    constexpr std::string_view message = "Hello from the UDP client!";
    const auto payload = std::as_bytes(std::span{ message.data(), message.size() });
    // First SendTo creates the client's socket and binds an ephemeral port.
    const auto sent = client.SendTo(payload, endpoint.Value());
    if (!sent)
    {
        std::cerr << "SendTo failed: " << sent.Error() << '\n';
        return 1;
    }
    if (!SetReceiveTimeout(client)) { return 1; }

    std::array<std::byte, 256> buffer{};
    const auto received = server.ReceiveFrom(buffer);
    if (!received)
    {
        std::cerr << "Server ReceiveFrom failed: " << received.Error() << '\n';
        return 1;
    }
    std::cout << "Received " << received.Value().BytesReceived << " bytes from "
        << received.Value().Sender.ToString() << '\n';
    // Reply to the sender reported by ReceiveFrom. One send is one datagram.
    const auto reply = server.SendTo(
        std::span{buffer}.first(received.Value().BytesReceived), received.Value().Sender);
    if (!reply)
    {
        std::cerr << "Reply failed: " << reply.Error() << '\n';
        return 1;
    }
    const auto echoed = client.ReceiveFrom(buffer);
    if (!echoed)
    {
        std::cerr << "Client ReceiveFrom failed: " << echoed.Error() << '\n';
        return 1;
    }
    const std::string_view text{
        reinterpret_cast<const char*>(buffer.data()), echoed.Value().BytesReceived };
    if (text != message || echoed.Value().Sender != endpoint.Value())
    {
        std::cerr << "Echo did not match the request or expected sender\n";
        return 1;
    }
    std::cout << "Client received: " << text << '\n';

    const auto sentEmpty = client.SendTo({}, endpoint.Value());
    if (!sentEmpty)
    {
        std::cerr << "Empty SendTo failed: " << sentEmpty.Error() << '\n';
        return 1;
    }
    const auto empty = server.ReceiveFrom({});
    if (!empty)
    {
        std::cerr << "Empty ReceiveFrom failed: " << empty.Error() << '\n';
        return 1;
    }
    if (empty.Value().BytesReceived != 0)
    {
        std::cerr << "Expected an empty datagram\n";
        return 1;
    }
    std::cout << "Received an empty datagram (a message, not EOF)\n";
    return 0;
}
