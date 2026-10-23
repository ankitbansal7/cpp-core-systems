#pragma once

#include "net/platform.hpp"
#include "net/socket_error.hpp"

// RAII wrapper over winsock init and deinit
// no-op for linux
class SocketRuntime final
{
public:
    SocketRuntime() noexcept
    {
#ifdef _WIN32
        WSADATA data{};
        const int result = ::WSAStartup(MAKEWORD(2, 2), &data);

        if (result != 0)
        {
            m_error = SocketError{ result };
            return;
        }

        // Canonical Winsock 2.2 version check recommended by Microsoft:
        if (LOBYTE(data.wVersion) != 2 || HIBYTE(data.wVersion) != 2)
        {
            ::WSACleanup();
            m_error = SocketError{ WSAVERNOTSUPPORTED };
            return;
        }
#endif

        m_initialized = true;
    }

    ~SocketRuntime() noexcept
    {
#ifdef _WIN32
        if (m_initialized)
        {
            ::WSACleanup();
        }
#endif
    }

    SocketRuntime(const SocketRuntime&) = delete;
    SocketRuntime& operator=(const SocketRuntime&) = delete;
    SocketRuntime(SocketRuntime&&) = delete;
    SocketRuntime& operator=(SocketRuntime&&) = delete;

    [[nodiscard]]
    bool IsInitialized() const noexcept
    {
        return m_initialized;
    }

    [[nodiscard]]
    explicit operator bool() const noexcept
    {
        return IsInitialized();
    }

    [[nodiscard]]
    const SocketError& GetError() const noexcept
    {
        return m_error;
    }

private:
    SocketError m_error{};
    bool m_initialized{ false };
};

// Keep runtime alive until all sockets have been destroyed:
//
// int main()
// {
//     SocketRuntime runtime;
//
//     if (!runtime)
//     {
//         return runtime.GetError().Code();
//     }
//
//     SocketHandle socket{ ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP) };
// }
