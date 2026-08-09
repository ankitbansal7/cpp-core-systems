#ifndef SOCKET_ERROR_HPP
#define SOCKET_ERROR_HPP

#include <string>

#ifdef _WIN32
#include <WinSock2.h>
#else
#include <cerrno>
#include <cstring>
#endif

// Lightweight, header-only helper for retrieving and formatting the last
// socket-related error in a platform-independent way.
class SocketError
{
public:
    using ErrorCode = int;

public:
    explicit SocketError(ErrorCode code) noexcept
        : m_code(code)
    {
    }

    [[nodiscard]]
    ErrorCode Code() const noexcept
    {
        return m_code;
    }

    [[nodiscard]]
    std::string Message() const
    {
#ifdef _WIN32
        char* msgBuffer = nullptr;
        DWORD length = ::FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            static_cast<DWORD>(m_code),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<char*>(&msgBuffer),
            0,
            nullptr);

        std::string result;

        if (length > 0 && msgBuffer != nullptr)
        {
            result.assign(msgBuffer, length);

            // FormatMessage often appends a trailing CRLF; trim it.
            while (!result.empty() && ((result.back() == '\n') || (result.back() == '\r')))
            {
                result.pop_back();
            }
        }
        else
        {
            result = "Unknown error " + std::to_string(m_code);
        }

        if (msgBuffer != nullptr)
        {
            ::LocalFree(msgBuffer);
        }

        return result;
#else
        // strerror is not guaranteed thread-safe on all platforms; use the
        // reentrant variant where available.
        char buffer[256] = {};
#if defined(__GLIBC__) && defined(_GNU_SOURCE)
        // GNU-specific strerror_r returns char*, may not use buffer.
        return std::string(::strerror_r(m_code, buffer, sizeof(buffer)));
#else
        // POSIX/XSI-compliant strerror_r returns int.
        if (::strerror_r(m_code, buffer, sizeof(buffer)) == 0)
        {
            return std::string(buffer);
        }

        return "Unknown error " + std::to_string(m_code);
#endif
#endif
    }

    // Captures the current last-error value (errno on POSIX, WSAGetLastError on Windows).
    static SocketError Last() noexcept
    {
#ifdef _WIN32
        return SocketError{ ::WSAGetLastError() };
#else
        return SocketError{ errno };
#endif
    }

private:
    ErrorCode m_code{ 0 };
};

#endif // SOCKET_ERROR_HPP
