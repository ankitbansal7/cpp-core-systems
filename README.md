# cpp-core-systems
Low-level C++ runtime and systems foundations with performance-focused experiments.
This repository is used as a personal reference and learning codebase.

## TCP networking

`TcpSocket` is a move-only blocking TCP connection. Call `Connect(endpoint)` on
a default-constructed socket to create a client. `TcpListener` provides the
server side: call `Bind(endpoint)`, `Listen()`, then `Accept()` for each client.
Each accepted `TcpSocket` owns its connection independently of the listener.
Both support IPv4 and IPv6 numeric endpoints. Keep a `SocketRuntime` alive until
all sockets and listeners have been destroyed.

Operations return `SocketResult<T>` rather than throwing socket errors. Check
the result before calling `Value()`; use `Error()` on failure. `Send` and
`Receive` return the number of bytes transferred and may transfer only part of
the buffer. A successful receive of zero bytes into a nonempty buffer signals
peer EOF. Empty buffers are successful no-ops. `Shutdown(SocketShutdown::Send)`
signals EOF to the peer while allowing responses to be received. `Close()` and
destruction release the native handle. `IsValid()` reports handle ownership,
not whether the peer is still connected.

These initial APIs block and do not impose deadlines or resolve hostnames.
`GetNativeHandle()` borrows the handle for platform-specific socket options;
do not close it directly. Concurrent mutation or closing of the same object
requires external coordination.

[example_tcp_loopback.cpp](examples/net/example_tcp_loopback.cpp) runs a small
client/server echo exchange entirely on loopback using an OS-selected port.
It handles partial transfers and half-close without requiring a separate server.
On Windows, from a Visual Studio developer shell:

```powershell
cmake --preset x64-debug-tests
cmake --build --preset x64-debug-tests --target example_tcp_loopback test_net
.\out\build\x64-debug-tests\examples\example_tcp_loopback.exe
ctest --preset x64-debug-tests -L net
```

In Visual Studio, select **x64 Debug (Tests)**, configure CMake, and select
`example_tcp_loopback.exe` to run the example. The tests share the `test_net`
executable and are discovered individually after building it.

## Linux setup

Install a C++20-capable compiler, CMake 3.20 or newer, Git, and Make
(or Ninja if selected as your CMake generator). On Ubuntu/Debian:

```bash
sudo apt update
sudo apt install build-essential cmake git
```

## Building on Linux

Build the library and examples with:

```bash
bash scripts/build.sh
```

The default is a Debug build in `out/build/linux-debug`. To build Release:

```bash
bash scripts/build.sh Release
```

Build configurations use separate directories, such as `out/build/linux-release`.
The script also supports `RelWithDebInfo` and `MinSizeRel`. Subsequent runs
rebuild changed files. You can invoke it by its absolute path from any directory.

## Running tests on Linux

From the repository root, run:

```bash
bash scripts/run-tests.sh
```

The script configures a Debug build in `out/build/linux-tests`, builds the
project, and runs all registered container and networking tests. Subsequent
runs reuse the build directory and rebuild changed files. The first run
downloads GoogleTest automatically. Any configuration, build, or test failure
returns a nonzero exit code.

You can also invoke the script by its absolute path from any directory.
Additional arguments go directly to CTest, for example:

```bash
bash scripts/run-tests.sh -L net
bash scripts/run-tests.sh -R SocketHandle
```

## Running clang-tidy on Linux

Install clang-tidy alongside the Linux build prerequisites:

```bash
sudo apt install clang-tidy
bash scripts/run-clang-tidy.sh
```

The script generates a compile database in `out/build/linux-clang-tidy` and
checks the library and example sources, including project headers they use.
The minimal `.clang-tidy` configuration enables Clang static analyzer and
bug-prone code checks. It reports diagnostics without modifying files.
Headers that are not included by these sources are not analyzed.

Additional arguments go directly to clang-tidy. To make enabled warnings fail
the run:

```bash
bash scripts/run-clang-tidy.sh '--warnings-as-errors=*'
```
