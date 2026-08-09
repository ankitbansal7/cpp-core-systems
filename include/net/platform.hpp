#ifndef PLATFORM_HPP
#define PLATFORM_HPP

// The one place the platform socket headers get pulled in. Every other header
// under net/ includes this first and never includes a system socket header
// directly, so the include-order rules below only need to be right once.

#ifdef _WIN32

// WinSock2.h must be seen before windows.h. If anything drags in the older
// winsock.h first, the two disagree about dozens of declarations and the build
// dies in a wall of redefinition errors.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// windows.h defines min and max as macros, which breaks std::min, std::max,
// and any member function that shares those names.
#ifndef NOMINMAX
#define NOMINMAX
#endif

// inet_pton, inet_ntop and getaddrinfo are only declared when the target is
// Vista or newer. MinGW in particular can default below that.
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601 // Windows 7
#endif

#include <WinSock2.h>
#include <ws2tcpip.h>

#else

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>

#endif

#endif // PLATFORM_HPP
