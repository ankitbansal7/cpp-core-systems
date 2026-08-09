#ifndef SOCKET_HANDLE_HPP
#define SOCKET_HANDLE_HPP

#include <cstdio>

#include "net/platform.hpp"
#include "net/socket_error.hpp"

class SocketHandle
{
public:
#ifdef _WIN32
    using NativeHandle = SOCKET;
    static constexpr NativeHandle InvalidHandle = INVALID_SOCKET;
#else
    using NativeHandle = int;
    static constexpr NativeHandle InvalidHandle = -1;
#endif

public:
    SocketHandle() noexcept = default;

    explicit SocketHandle(NativeHandle handle) noexcept
        : m_handle(handle)
    {
    }

    ~SocketHandle()
    {
        Close();
    }

    SocketHandle(const SocketHandle&) = delete;
    SocketHandle& operator=(const SocketHandle&) = delete;

    SocketHandle(SocketHandle&& other) noexcept
        : m_handle(other.Release())
    {
    }

    SocketHandle& operator=(SocketHandle&& other) noexcept
    {
        if (this != &other)
        {
            Close();
            m_handle = other.Release();
        }

        return *this;
    }

    [[nodiscard]]
    bool IsValid() const noexcept
    {
        return (m_handle != InvalidHandle);
    }

    [[nodiscard]]
    NativeHandle Get() const noexcept
    {
        return m_handle;
    }

    [[nodiscard]]
    NativeHandle Release() noexcept
    {
        NativeHandle handle = m_handle;
        m_handle = InvalidHandle;
        return handle;
    }

    void Reset(NativeHandle handle = InvalidHandle) noexcept
    {
        if (m_handle != handle)
        {
            Close();
            m_handle = handle;
        }
    }

    void Close() noexcept
    {
        if (!IsValid())
        {
            return;
        }

#ifdef _WIN32
        int result = ::closesocket(m_handle);
#else
        int result = ::close(m_handle);
#endif

#ifndef NDEBUG
        if (result != 0)
        {
            SocketError error = SocketError::Last();
            std::fprintf(stderr,
                "SocketHandle::Close failed (handle=%lld): %s\n",
                static_cast<long long>(m_handle),
                error.Message().c_str());
        }
#else
        (void)result;
#endif

        m_handle = InvalidHandle;
    }

private:
    NativeHandle m_handle{ InvalidHandle };
};

#endif // SOCKET_HANDLE_HPP
