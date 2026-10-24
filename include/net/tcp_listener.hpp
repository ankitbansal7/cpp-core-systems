#pragma once

#include "net/tcp_socket.hpp"

namespace net
{
    // Blocking TCP listener. Bind, then Listen, then Accept one TcpSocket per peer.
    // Keep SocketRuntime alive until the listener and all accepted sockets close.
    class TcpListener final
    {
    public:
        TcpListener() noexcept = default;
        TcpListener(const TcpListener&) = delete;
        TcpListener& operator=(const TcpListener&) = delete;
        TcpListener(TcpListener&&) noexcept = default;
        TcpListener& operator=(TcpListener&&) noexcept = default;

        // Valid means a bound handle is owned; Listen must still be called.
        [[nodiscard]] bool IsValid() const noexcept { return m_handle.IsValid(); }
        [[nodiscard]] explicit operator bool() const noexcept { return IsValid(); }
        [[nodiscard]] SocketHandle::NativeHandle GetNativeHandle() const noexcept { return m_handle.Get(); }

        // Port zero asks the OS for an available port. Read it with GetLocalEndpoint.
        // A failed bind leaves this object invalid and reusable.
        [[nodiscard]] SocketResult<void> Bind(const SocketEndpoint& endpoint) noexcept
        {
            if (IsValid())
            {
                return SocketResult<void>::Failure(detail::InvalidArgument());
            }
            auto created = detail::CreateTcpSocket(endpoint.GetVersion());
            if (!created)
            {
                return SocketResult<void>::Failure(created.Error());
            }
            auto handle = std::move(created).Value();
            const auto [storage, length] = endpoint.ToNative();
            if (::bind(handle.Get(), reinterpret_cast<const sockaddr*>(&storage), length) != 0)
            {
                return SocketResult<void>::Failure(SocketError::Last());
            }
            m_handle = std::move(handle);
            return SocketResult<void>::Success();
        }

        [[nodiscard]] SocketResult<void> Listen(int backlog = SOMAXCONN) noexcept
        {
            if (!IsValid())
            {
                return SocketResult<void>::Failure(detail::InvalidSocket());
            }
            if (backlog <= 0)
            {
                return SocketResult<void>::Failure(detail::InvalidArgument());
            }
            if (::listen(m_handle.Get(), backlog) != 0)
            {
                return SocketResult<void>::Failure(SocketError::Last());
            }
            return SocketResult<void>::Success();
        }

        // Waits for one incoming connection. Closing this listener later does not
        // close the returned socket. Interrupted accepts are retried.
        [[nodiscard]] SocketResult<TcpSocket> Accept() noexcept
        {
            if (!IsValid())
            {
                return SocketResult<TcpSocket>::Failure(detail::InvalidSocket());
            }
            for (;;)
            {
                SocketHandle handle{ ::accept(m_handle.Get(), nullptr, nullptr) };
                if (!handle)
                {
                    const auto error = SocketError::Last();
                    if (error.IsInterrupted())
                    {
                        continue;
                    }
                    return SocketResult<TcpSocket>::Failure(error);
                }
                const auto configured = detail::ConfigureTcpSocket(handle.Get());
                if (!configured)
                {
                    return SocketResult<TcpSocket>::Failure(configured.Error());
                }
                return SocketResult<TcpSocket>::Success(TcpSocket{ std::move(handle) });
            }
        }

        [[nodiscard]] SocketResult<SocketEndpoint> GetLocalEndpoint() const noexcept
        {
            return detail::QueryEndpoint(m_handle.Get(), false);
        }

        void Close() noexcept { m_handle.Close(); }

    private:
        SocketHandle m_handle;
    };
}
