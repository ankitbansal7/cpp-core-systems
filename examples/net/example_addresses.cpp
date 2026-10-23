// IPAddress and SocketEndpoint are values: parsing, formatting, native
// conversion, and hashing do not require opening a socket.
#include "net/ip_address.hpp"
#include "net/socket_endpoint.hpp"
#include "net/socket_runtime.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <unordered_set>

int main()
{
    SocketRuntime runtime;
    if (!runtime)
    {
        std::cerr << "Runtime initialization failed: " << runtime.GetError() << '\n';
        return 1;
    }

    for (auto version : { IPVersion::IPv4, IPVersion::IPv6 })
    {
        std::cout << "Loopback: " << IPAddress::Loopback(version).ToString()
            << ", any: " << IPAddress::Any(version).ToString() << '\n';
    }

    const std::array<std::uint8_t, 4> bytes{ 192, 168, 0, 21 };
    const auto fromBytes = IPAddress::FromBytes(bytes, IPVersion::IPv4);
    const auto parsed = IPAddress::Parse("192.168.0.21");
    if (!fromBytes || !parsed || *fromBytes != *parsed)
    {
        std::cerr << "Address construction failed\n";
        return 1;
    }
    std::cout << "Parsed address: " << parsed->ToString()
        << " (" << parsed->SpanBytes().size() << " bytes)\n";
    std::unordered_set<IPAddress> addresses{ *parsed, *fromBytes };
    std::cout << "Unique addresses: " << addresses.size() << '\n';

    if (!IPAddress::Parse("invalid-address"))
    {
        std::cout << "Invalid address rejected\n";
    }

    std::unordered_set<SocketEndpoint> endpoints;
    // IPv6 endpoints use brackets; a numeric scope ID follows '%'.
    for (const char* text : { "127.0.0.1:8080", "[::1]:8080", "[fe80::1%3]:9000" })
    {
        const auto endpoint = SocketEndpoint::Parse(text);
        if (!endpoint)
        {
            std::cerr << "Endpoint parsing failed: " << text << '\n';
            return 1;
        }

        const auto [storage, length] = endpoint->ToNative();
        const auto restored = SocketEndpoint::FromNative(
            reinterpret_cast<const sockaddr*>(&storage), length);
        if (!restored || *restored != *endpoint)
        {
            std::cerr << "Native endpoint round trip failed\n";
            return 1;
        }
        endpoints.insert(*restored);
        std::cout << "Endpoint: " << restored->ToString()
            << ", port=" << restored->GetPort()
            << ", scope=" << restored->GetScopeId() << '\n';
    }
    std::cout << "Unique endpoints: " << endpoints.size() << '\n';
    if (!SocketEndpoint::Parse("127.0.0.1:70000"))
    {
        std::cout << "Out-of-range port rejected\n";
    }
    return 0;
}
