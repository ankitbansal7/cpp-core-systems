// Demonstrates the RAII ownership model of SocketHandle and the error
// reporting of SocketError. No connections are made -- every socket here is
// created, inspected, and closed locally.

#include <iostream>
#include <utility>
#include <unordered_set>

#include "net/socket_handle.hpp"
#include "net/socket_error.hpp"
#include "net/ip_address.hpp"

#ifndef _WIN32
#include <sys/socket.h>
#endif

namespace
{
    // Winsock refuses to hand out sockets until the library is initialised,
    // and expects a matching cleanup. Scoping that in a small RAII type keeps
    // main() free of platform noise; on POSIX it does nothing at all.
    class NetworkStartup
    {
    public:
        NetworkStartup()
        {
#ifdef _WIN32
            WSADATA data{};
            int result = ::WSAStartup(MAKEWORD(2, 2), &data);

            if (result != 0)
            {
                // WSAStartup returns the error directly rather than setting
                // the last-error slot, so report the returned code.
                std::cerr << "WSAStartup failed: "
                    << SocketError{ result }.Message() << '\n';
            }
#endif
        }

        ~NetworkStartup()
        {
#ifdef _WIN32
            ::WSACleanup();
#endif
        }

        NetworkStartup(const NetworkStartup&) = delete;
        NetworkStartup& operator=(const NetworkStartup&) = delete;
    };

    // Creates a plain TCP socket, or an invalid handle if the call fails.
    SocketHandle MakeTcpSocket()
    {
        SocketHandle::NativeHandle native = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        if (native == SocketHandle::InvalidHandle)
        {
            std::cerr << "socket() failed: " << SocketError::Last().Message() << '\n';
        }

        return SocketHandle{ native };
    }

    void Describe(const char* label, const SocketHandle& handle)
    {
        std::cout << label << ": valid=" << std::boolalpha << handle.IsValid()
            << ", native=" << static_cast<long long>(handle.Get()) << '\n';
    }
}

int main()
{
    NetworkStartup startup;

    // A default-constructed handle owns nothing and is safe to destroy.
    SocketHandle empty;
    Describe("default-constructed", empty);

    SocketHandle socket = MakeTcpSocket();
    Describe("freshly created", socket);

    if (!socket.IsValid())
    {
        return 1;
    }

    // Moving transfers ownership: the source is left invalid, and only the
    // destination will close the descriptor.
    SocketHandle moved{ std::move(socket) };
    Describe("after move-construct (source)", socket);
    Describe("after move-construct (dest)", moved);

    // Move assignment closes whatever the destination already held first.
    SocketHandle assigned = MakeTcpSocket();
    Describe("assignment target before", assigned);
    assigned = std::move(moved);
    Describe("assignment target after", assigned);
    Describe("assignment source after", moved);

    // Release hands the descriptor back to the caller, who becomes
    // responsible for closing it. The handle no longer owns anything.
    SocketHandle::NativeHandle raw = assigned.Release();
    std::cout << "released native handle: " << static_cast<long long>(raw) << '\n';
    Describe("after release", assigned);

    // Reset adopts a descriptor, closing any previously held one.
    assigned.Reset(raw);
    Describe("after reset", assigned);

    // Closing twice is harmless; the second call sees an invalid handle.
    assigned.Close();
    assigned.Close();
    Describe("after close", assigned);

    // SocketError reports whatever the last failing call recorded. Asking for
    // a nonsensical address family is a reliable way to provoke one.
    SocketHandle::NativeHandle bad = ::socket(-1, -1, -1);

    if (bad == SocketHandle::InvalidHandle)
    {
        SocketError error = SocketError::Last();
        std::cout << "expected failure, code " << error.Code()
            << ": " << error.Message() << '\n';
    }

    // Scope exit closes the socket without an explicit call.
    {
        SocketHandle scoped = MakeTcpSocket();
        Describe("scoped", scoped);
    }

    std::cout << "scoped socket closed on scope exit\n";

    std::cout << "Loopback IPv4 Address: " << IPAddress::Loopback(IPVersion::IPv4).ToString() << std::endl;
    std::cout << "Loopback IPv6 Address: " << IPAddress::Loopback(IPVersion::IPv6).ToString() << std::endl;
    std::cout << "Unspecified IPv4 Address: " << IPAddress::Any(IPVersion::IPv4).ToString() << std::endl;
    std::cout << "Unspecified IPv6 Address: " << IPAddress::Any(IPVersion::IPv6).ToString() << std::endl;

    auto address1 = IPAddress::Parse("192.168.0.21");

    if (address1)
    {
        std::cout << "address1: " << address1->ToString() << std::endl;
    }
    else
    {
        std::cout << "invalid address\n";
    }

    auto address2 = IPAddress::Parse("1001::34:FF:0:DB8");

    if (address2)
    {
        std::cout << "address2: " << address2->ToString() << std::endl;
    }
    else
    {
        std::cout << "invalid address\n";
    }

    auto address3 = IPAddress::Parse("ankit");

    if (address3)
    {
        std::cout << "address3: " << address3->ToString() << std::endl;
    }
    else
    {
        std::cout << "invalid address\n";
    }

    std::unordered_set<IPAddress> addresses;

    return 0;
}
