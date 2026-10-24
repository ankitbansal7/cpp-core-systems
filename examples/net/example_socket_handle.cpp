// Demonstrates SocketRuntime lifetime and SocketHandle ownership.
// All sockets are created and closed locally; no connections are made.

#include <iostream>
#include <utility>

#include "net/socket_runtime.hpp"
#include "net/socket_handle.hpp"
#include "net/socket_error.hpp"

using namespace net;

namespace
{
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
    // Declare runtime first so it outlives every socket (required on Windows).
    SocketRuntime runtime;

    if (!runtime)
    {
        std::cerr << "SocketRuntime initialization failed: "
            << runtime.GetError().Message() << '\n';
        return runtime.GetError().Code();
    }

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
    if (!assigned)
    {
        return 1;
    }
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

    // Scope exit closes the socket without an explicit call.
    {
        SocketHandle scoped = MakeTcpSocket();
        if (!scoped)
        {
            return 1;
        }
        Describe("scoped", scoped);
    }

    std::cout << "scoped socket closed on scope exit\n";

    return 0;
}
