// SocketError captures a native failure by value and classifies error conditions.
#include "net/socket_error.hpp"
#include "net/socket_handle.hpp"
#include "net/socket_runtime.hpp"

#include <iostream>
#include <system_error>

using namespace net;

int main()
{
    SocketRuntime runtime;
    if (!runtime)
    {
        std::cerr << "Runtime initialization failed: " << runtime.GetError() << '\n';
        return 1;
    }

    // An unsupported address family deliberately fails without network traffic.
    SocketHandle invalid{ ::socket(-1, SOCK_STREAM, IPPROTO_TCP) };
    if (invalid)
    {
        std::cerr << "Expected socket creation to fail\n";
        return 1;
    }
    // Capture immediately: another OS call can overwrite the last error.
    const SocketError captured = SocketError::Last();
    std::cout << "Captured failure: " << captured << '\n';
    if (!captured.HasError())
    {
        std::cerr << "Expected a nonzero socket error\n";
        return 1;
    }

    // Generic conditions demonstrate portable classification without relying
    // on an actual timeout or remote host refusing a connection.
    const SocketError wouldBlock{ std::make_error_code(std::errc::operation_would_block) };
    const SocketError refused{ std::make_error_code(std::errc::connection_refused) };
    std::cout << std::boolalpha
        << "Would block: " << wouldBlock.IsWouldBlock() << '\n'
        << "Connection refused: " << refused.IsConnectionRefused() << '\n'
        << "Matches std::errc: " << (refused == std::errc::connection_refused) << '\n';

    // SocketError's bool is true for an ERROR; SocketResult's is true for SUCCESS.
    const auto success = SocketError::Success();
    std::cout << "Success has an error: " << static_cast<bool>(success) << '\n';
    return 0;
}
